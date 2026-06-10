#include "rg_system.h"
#include "rg_audio.h"

#if RG_AUDIO_USE_INT_DAC || RG_AUDIO_USE_EXT_DAC

#ifndef ESP_PLATFORM
#error "I2S support can only be built inside esp-idf!"
#elif !CONFIG_IDF_TARGET_ESP32 && RG_AUDIO_USE_INT_DAC
#error "Your chip has no DAC! Please set RG_AUDIO_USE_INT_DAC to 0 in your target file."
#endif

#include <driver/gpio.h>
#if RG_AUDIO_USE_ES8389
#include <driver/i2s_std.h>
#else
#include <driver/i2s.h>
#endif

#if RG_AUDIO_USE_ES8389
#include "rg_i2c.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_data_if.h"
#include "esp_codec_dev_defaults.h"
#include "esp_codec_dev.h"
#include "audio_codec_gpio_if.h"
#include "audio_codec_if.h"
#include "es8389_codec.h"
#endif

#ifdef RG_GPIO_SND_AMP_ENABLE_INVERT
#define MUTE_ENABLE 1
#define MUTE_DISABLE 0
#else
#define MUTE_ENABLE 0
#define MUTE_DISABLE 1
#endif

// We can safely assume that no application will submit more than 640 audio frames per call to
// driver_submit (32000/50). Using a single large buffer risks blocking the call needlessly because
// some apps submit more than once per cycle or there could be occasional jitter (early submission).
#define DMA_BUFFER_COUNT 4
#define DMA_BUFFER_LEN 180

static struct {
    const char *last_error;
    int device;
    int volume;
    bool muted;
#if RG_AUDIO_USE_ES8389
    i2s_chan_handle_t tx_chan;
    const audio_codec_if_t *codec;
    const audio_codec_data_if_t *codec_data;
    const audio_codec_gpio_if_t *codec_gpio;
    esp_codec_dev_handle_t codec_dev;
#endif
} state;

#if RG_AUDIO_USE_ES8389
#define ES8389_I2C_ADDR_7BIT (ES8389_CODEC_DEFAULT_ADDR >> 1)

typedef struct
{
    audio_codec_ctrl_if_t base;
    bool open;
} rg_codec_ctrl_t;

static int rg_codec_ctrl_open(const audio_codec_ctrl_if_t *ctrl, void *cfg, int cfg_size)
{
    (void)cfg;
    (void)cfg_size;
    ((rg_codec_ctrl_t *)ctrl)->open = rg_i2c_init();
    return ((rg_codec_ctrl_t *)ctrl)->open ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_DRV_ERR;
}

static bool rg_codec_ctrl_is_open(const audio_codec_ctrl_if_t *ctrl)
{
    return ctrl && ((rg_codec_ctrl_t *)ctrl)->open;
}

static int rg_codec_ctrl_read_reg(const audio_codec_ctrl_if_t *ctrl, int reg, int reg_len, void *data, int data_len)
{
    if (!rg_codec_ctrl_is_open(ctrl) || !data || reg_len != 1)
        return ESP_CODEC_DEV_INVALID_ARG;
    return rg_i2c_read(ES8389_I2C_ADDR_7BIT, reg, data, data_len) ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_READ_FAIL;
}

static int rg_codec_ctrl_write_reg(const audio_codec_ctrl_if_t *ctrl, int reg, int reg_len, void *data, int data_len)
{
    if (!rg_codec_ctrl_is_open(ctrl) || !data || reg_len != 1)
        return ESP_CODEC_DEV_INVALID_ARG;
    return rg_i2c_write(ES8389_I2C_ADDR_7BIT, reg, data, data_len) ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_WRITE_FAIL;
}

static int rg_codec_ctrl_close(const audio_codec_ctrl_if_t *ctrl)
{
    ((rg_codec_ctrl_t *)ctrl)->open = false;
    return ESP_CODEC_DEV_OK;
}

static rg_codec_ctrl_t rg_codec_ctrl = {
    .base = {
        .open = rg_codec_ctrl_open,
        .is_open = rg_codec_ctrl_is_open,
        .read_reg = rg_codec_ctrl_read_reg,
        .write_reg = rg_codec_ctrl_write_reg,
        .close = rg_codec_ctrl_close,
    },
};

static bool es8389_init(int sample_rate)
{
    rg_codec_ctrl.open = false;
    if (rg_codec_ctrl.base.open(&rg_codec_ctrl.base, NULL, 0) != ESP_CODEC_DEV_OK)
    {
        state.last_error = "ES8389 I2C init failed";
        return false;
    }

    state.codec_gpio = audio_codec_new_gpio();
    if (!state.codec_gpio)
    {
        state.last_error = "ES8389 GPIO interface failed";
        return false;
    }

    const esp_codec_dev_hw_gain_t gain = {
        .pa_voltage = 5.0f,
        .codec_dac_voltage = 3.3f,
    };
    es8389_codec_cfg_t codec_cfg = {
        .ctrl_if = &rg_codec_ctrl.base,
        .gpio_if = state.codec_gpio,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = RG_GPIO_SND_AMP_ENABLE,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = false,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = gain,
        .no_dac_ref = false,
    };
    state.codec = es8389_codec_new(&codec_cfg);
    if (!state.codec)
    {
        state.last_error = "ES8389 codec init failed";
        return false;
    }

    audio_codec_i2s_cfg_t i2s_cfg = {
        .tx_handle = state.tx_chan,
    };
    state.codec_data = audio_codec_new_i2s_data(&i2s_cfg);
    if (!state.codec_data)
    {
        state.last_error = "ES8389 I2S data interface failed";
        return false;
    }

    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = state.codec,
        .data_if = state.codec_data,
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
    };
    state.codec_dev = esp_codec_dev_new(&dev_cfg);
    if (!state.codec_dev)
    {
        state.last_error = "ES8389 codec device failed";
        return false;
    }

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = sample_rate,
        .channel = 2,
        .bits_per_sample = 16,
    };
    if (esp_codec_dev_open(state.codec_dev, &fs) != ESP_CODEC_DEV_OK)
    {
        state.last_error = "ES8389 open failed";
        return false;
    }
    if (esp_codec_dev_set_out_vol(state.codec_dev, RG_MIN(RG_MAX(state.volume, 0), 100)) != ESP_CODEC_DEV_OK)
    {
        state.last_error = "ES8389 volume setup failed";
        return false;
    }
    return true;
}
#endif

static bool driver_init(int device, int sample_rate)
{
    state.last_error = NULL;
    state.device = device;

    if (state.device == 0)
    {
    #if RG_AUDIO_USE_INT_DAC
    #if RG_AUDIO_USE_ES8389
        state.last_error = "This device does not support internal DAC mode!";
    #else
        esp_err_t ret = i2s_driver_install(I2S_NUM_0, &(i2s_config_t){
            .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
            .sample_rate = sample_rate,
            .bits_per_sample = 16,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_MSB,
            .intr_alloc_flags = 0, // ESP_INTR_FLAG_LEVEL1
            .dma_buf_count = DMA_BUFFER_COUNT,
            .dma_buf_len = DMA_BUFFER_LEN,
        }, 0, NULL);
        if (ret == ESP_OK)
            ret = i2s_set_dac_mode(RG_AUDIO_USE_INT_DAC);
        if (ret != ESP_OK)
            state.last_error = esp_err_to_name(ret);
    #endif
    #else
        state.last_error = "This device does not support internal DAC mode!";
    #endif
    }
    else if (state.device == 1)
    {
    #if RG_AUDIO_USE_EXT_DAC
    #if RG_AUDIO_USE_ES8389
        i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
        chan_cfg.auto_clear = true;
        esp_err_t ret = i2s_new_channel(&chan_cfg, &state.tx_chan, NULL);
        i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
        clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
        const i2s_std_config_t std_cfg = {
            .clk_cfg = clk_cfg,
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
            .gpio_cfg = {
                .mclk = RG_GPIO_SND_I2S_MCLK,
                .bclk = RG_GPIO_SND_I2S_BCK,
                .ws = RG_GPIO_SND_I2S_WS,
                .dout = RG_GPIO_SND_I2S_DATA,
                .din = RG_GPIO_SND_I2S_DATA_IN,
            },
        };
        if (ret == ESP_OK)
            ret = i2s_channel_init_std_mode(state.tx_chan, &std_cfg);
        if (ret == ESP_OK)
            ret = i2s_channel_enable(state.tx_chan);
        if (ret != ESP_OK)
            state.last_error = esp_err_to_name(ret);
        else if (!es8389_init(sample_rate))
            return false;
    #else
        esp_err_t ret = i2s_driver_install(I2S_NUM_0, &(i2s_config_t){
            .mode = I2S_MODE_MASTER | I2S_MODE_TX,
            .sample_rate = sample_rate,
            .bits_per_sample = 16,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = 0, // ESP_INTR_FLAG_LEVEL1
            .dma_buf_count = DMA_BUFFER_COUNT,
            .dma_buf_len = DMA_BUFFER_LEN,
        #if CONFIG_IDF_TARGET_ESP32
            .use_apll = true, // External DAC may care about accuracy
        #endif
        }, 0, NULL);
        if (ret == ESP_OK)
        {
            ret = i2s_set_pin(I2S_NUM_0, &(i2s_pin_config_t) {
            #ifdef RG_GPIO_SND_I2S_MCLK
                .mck_io_num = RG_GPIO_SND_I2S_MCLK,
            #else
                .mck_io_num = GPIO_NUM_NC,
            #endif
                .bck_io_num = RG_GPIO_SND_I2S_BCK,
                .ws_io_num = RG_GPIO_SND_I2S_WS,
                .data_out_num = RG_GPIO_SND_I2S_DATA,
                .data_in_num = GPIO_NUM_NC
            });
        }
        if (ret != ESP_OK)
            state.last_error = esp_err_to_name(ret);
    #endif
    #else
        state.last_error = "This device does not support external DAC mode!";
    #endif
    }
    #ifdef RG_GPIO_SND_AMP_ENABLE
        gpio_reset_pin(RG_GPIO_SND_AMP_ENABLE);
        gpio_set_level(RG_GPIO_SND_AMP_ENABLE, MUTE_ENABLE);
        gpio_set_direction(RG_GPIO_SND_AMP_ENABLE, GPIO_MODE_OUTPUT);
    #endif
    return state.last_error == NULL;
}

static bool driver_set_sample_rates(int sampleRate)
{
#if RG_AUDIO_USE_ES8389
    if (state.tx_chan)
    {
        i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate);
        clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
        esp_err_t ret = i2s_channel_disable(state.tx_chan);
        if (ret == ESP_OK)
            ret = i2s_channel_reconfig_std_clock(state.tx_chan, &clk_cfg);
        if (ret == ESP_OK)
            ret = i2s_channel_enable(state.tx_chan);
        if (ret != ESP_OK)
            return false;
    }
    if (state.codec && state.codec->set_fs)
    {
        esp_codec_dev_sample_info_t fs = {
            .sample_rate = sampleRate,
            .channel = 2,
            .bits_per_sample = 16,
        };
        return esp_codec_dev_open(state.codec_dev, &fs) == ESP_CODEC_DEV_OK;
    }
    return true;
#else
    return i2s_set_sample_rates(I2S_NUM_0, sampleRate) == ESP_OK;
#endif
}

static bool driver_deinit(void)
{
#if RG_AUDIO_USE_ES8389
    if (state.tx_chan)
    {
        i2s_channel_disable(state.tx_chan);
        i2s_del_channel(state.tx_chan);
        state.tx_chan = NULL;
    }
#else
    i2s_driver_uninstall(I2S_NUM_0);
#endif
    if (state.device == 0)
    {
    #if RG_AUDIO_USE_INT_DAC && !RG_AUDIO_USE_ES8389
        i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
    #endif
    }
    else if (state.device == 1)
    {
    #if RG_AUDIO_USE_ES8389
        if (state.codec_dev)
            esp_codec_dev_set_out_mute(state.codec_dev, true);
        #ifdef RG_GPIO_SND_AMP_ENABLE
        gpio_set_level(RG_GPIO_SND_AMP_ENABLE, MUTE_ENABLE);
        #endif
        state.codec_dev = NULL;
        state.codec = NULL;
        state.codec_data = NULL;
        state.codec_gpio = NULL;
        rg_codec_ctrl_close(&rg_codec_ctrl.base);
    #elif RG_AUDIO_USE_EXT_DAC
        gpio_reset_pin(RG_GPIO_SND_I2S_BCK);
        gpio_reset_pin(RG_GPIO_SND_I2S_DATA);
        gpio_reset_pin(RG_GPIO_SND_I2S_WS);
    #endif
    }
    #ifdef RG_GPIO_SND_AMP_ENABLE
    gpio_reset_pin(RG_GPIO_SND_AMP_ENABLE);
    #endif
    return true;
}

static bool driver_submit(const rg_audio_frame_t *frames, size_t count)
{
    float volume = state.muted ? 0.f : (state.volume * 0.01f);
    bool use_internal_dac = state.device == 0;
    rg_audio_frame_t buffer[DMA_BUFFER_LEN];
    size_t pos = 0;

    for (size_t i = 0; i < count; ++i)
    {
        int left = frames[i].left * volume;
        int right = frames[i].right * volume;

        if (use_internal_dac)
        {
        #if !RG_AUDIO_USE_ES8389 && RG_AUDIO_USE_INT_DAC == 1
            left = ((left + right) >> 1) + 0x8000; // the internal DAC expects unsigned data
            right = 0;
        #elif !RG_AUDIO_USE_ES8389 && RG_AUDIO_USE_INT_DAC == 2
            left = 0;
            right = ((left + right) >> 1) + 0x8000; // the internal DAC expects unsigned data
        #elif !RG_AUDIO_USE_ES8389 && RG_AUDIO_USE_INT_DAC == 3
            // In two channel mode we use left and right as a differential mono output to increase resolution.
            int sample = (left + right) >> 1;
            if (sample > 0x7F00)
            {
                left = 0x8000 + (sample - 0x7F00);
                right = -0x8000 + 0x7F00;
            }
            else if (sample < -0x7F00)
            {
                left = 0x8000 + (sample + 0x7F00);
                right = -0x8000 + -0x7F00;
            }
            else
            {
                left = 0x8000;
                right = -0x8000 + sample;
            }
        #endif
        }

        // Clipping   (not necessary, we have (int16 * vol) and volume is never more than 1.0)
        // if (left > 32767) left = 32767; else if (left < -32768) left = -32767;
        // if (right > 32767) right = 32767; else if (right < -32768) right = -32767;

        // Queue
        buffer[pos].left = left;
        buffer[pos].right = right;

        ++pos;
        if (i == count - 1 || pos == RG_COUNT(buffer))
        {
        #if RG_AUDIO_USE_ES8389
            if (esp_codec_dev_write(state.codec_dev, buffer, pos * 4) != ESP_CODEC_DEV_OK)
                RG_LOGW("I2S Submission error! Written: 0/%d\n", pos * 4);
        #else
            size_t written;
            if (i2s_write(I2S_NUM_0, (void *)buffer, pos * 4, &written, 1000) != ESP_OK)
                RG_LOGW("I2S Submission error! Written: %d/%d\n", written, pos * 4);
        #endif
            pos = 0;
        }
    }
    return true;
}

static bool driver_set_mute(bool mute)
{
#if !RG_AUDIO_USE_ES8389
    i2s_zero_dma_buffer(I2S_NUM_0);
#endif
    #if RG_AUDIO_USE_ES8389
    if (state.codec_dev)
        esp_codec_dev_set_out_mute(state.codec_dev, mute);
    #endif
    #ifdef RG_GPIO_SND_AMP_ENABLE
    gpio_set_level(RG_GPIO_SND_AMP_ENABLE, mute ? MUTE_ENABLE : MUTE_DISABLE);
    #endif
    state.muted = mute;
    return true;
}

static bool driver_set_volume(int volume)
{
    state.volume = volume;
    #if RG_AUDIO_USE_ES8389
    if (state.codec_dev)
    {
        esp_codec_dev_set_out_vol(state.codec_dev, RG_MIN(RG_MAX(volume, 0), 100));
        esp_codec_dev_set_out_mute(state.codec_dev, volume <= 0 || state.muted);
    }
    #ifdef RG_GPIO_SND_AMP_ENABLE
    gpio_set_level(RG_GPIO_SND_AMP_ENABLE, (volume <= 0 || state.muted) ? MUTE_ENABLE : MUTE_DISABLE);
    #endif
    #endif
    return true;
}

static const char *driver_get_error(void)
{
    return state.last_error;
}

const rg_audio_driver_t rg_audio_driver_i2s = {
    .name = "i2s",
    .init = driver_init,
    .deinit = driver_deinit,
    .submit = driver_submit,
    .set_mute = driver_set_mute,
    .set_volume = driver_set_volume,
    .set_sample_rate = driver_set_sample_rates,
    .get_error = driver_get_error,
};

#endif // RG_AUDIO_USE_INT_DAC || RG_AUDIO_USE_EXT_DAC
