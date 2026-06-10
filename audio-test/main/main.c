#include <math.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "audio_codec_data_if.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_if.h"
#include "audio_codec_gpio_if.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

#define TAG "audio-test"

#define I2C_PORT I2C_NUM_0
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 48000
#define CHANNELS 2
#define BITS_PER_SAMPLE 16

#define GPIO_I2C_SDA GPIO_NUM_0
#define GPIO_I2C_SCL GPIO_NUM_1
#define GPIO_I2S_MCLK GPIO_NUM_2
#define GPIO_I2S_BCLK GPIO_NUM_3
#define GPIO_I2S_WS GPIO_NUM_4
#define GPIO_I2S_DOUT GPIO_NUM_5
#define GPIO_I2S_DIN GPIO_NUM_6
#define GPIO_PA_ENABLE GPIO_NUM_7
#define GPIO_BUTTON_ADC GPIO_NUM_42
#define BUTTON_ADC_UNIT ADC_UNIT_1
#define BUTTON_ADC_CHANNEL ADC_CHANNEL_0
#define BUTTON_ADC_ATTEN ADC_ATTEN_DB_0

#define VOL_PLUS_RAW_MIN 1500
#define VOL_PLUS_RAW_MAX 4096
#define VOL_MINUS_RAW_MIN 400
#define VOL_MINUS_RAW_MAX 1500

static i2s_chan_handle_t tx_chan;
static i2c_master_bus_handle_t i2c_bus;
static esp_codec_dev_handle_t codec_dev;
static adc_oneshot_unit_handle_t button_adc_unit;
static adc_oneshot_unit_handle_t adc2_unit;
static int output_volume = 20;

static esp_err_t i2c_init(void)
{
    const i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_PORT,
        .sda_io_num = GPIO_I2C_SDA,
        .scl_io_num = GPIO_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&cfg, &i2c_bus), TAG, "i2c bus failed");
    ESP_LOGW(TAG, "I2C ready sda=%d scl=%d", GPIO_I2C_SDA, GPIO_I2C_SCL);
    return ESP_OK;
}

static void i2c_scan(void)
{
    ESP_LOGW(TAG, "I2C scan start");
    for (uint8_t addr = 0x08; addr < 0x78; ++addr)
    {
        esp_err_t err = i2c_master_probe(i2c_bus, addr, 50);
        if (err == ESP_OK)
            ESP_LOGW(TAG, "I2C device found at 0x%02x", addr);
    }
    ESP_LOGW(TAG, "I2C scan end");
}

static esp_err_t i2s_init(void)
{
    const i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORT, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &tx_chan, NULL), TAG, "i2s new channel failed");

    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE);
    clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    const i2s_std_config_t std_cfg = {
        .clk_cfg = clk_cfg,
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = GPIO_I2S_MCLK,
            .bclk = GPIO_I2S_BCLK,
            .ws = GPIO_I2S_WS,
            .dout = GPIO_I2S_DOUT,
            .din = GPIO_I2S_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(tx_chan, &std_cfg), TAG, "i2s std init failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(tx_chan), TAG, "i2s tx enable failed");
    ESP_LOGW(TAG, "I2S ready mclk=%d bclk=%d ws=%d dout=%d din=%d", GPIO_I2S_MCLK, GPIO_I2S_BCLK, GPIO_I2S_WS, GPIO_I2S_DOUT, GPIO_I2S_DIN);
    return ESP_OK;
}

static esp_codec_dev_handle_t codec_init(void)
{
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_PORT,
        .addr = ES8389_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_bus,
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    ESP_RETURN_ON_FALSE(ctrl_if, NULL, TAG, "codec i2c ctrl failed");

    audio_codec_i2s_cfg_t i2s_cfg = {
        .tx_handle = tx_chan,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);
    ESP_RETURN_ON_FALSE(data_if, NULL, TAG, "codec i2s data failed");

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    ESP_RETURN_ON_FALSE(gpio_if, NULL, TAG, "codec gpio failed");

    es8389_codec_cfg_t es8389_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
        .pa_pin = GPIO_PA_ENABLE,
        .pa_reverted = false,
        .use_mclk = false,
    };
    const audio_codec_if_t *codec_if = es8389_codec_new(&es8389_cfg);
    ESP_RETURN_ON_FALSE(codec_if, NULL, TAG, "es8389 codec failed");

    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = codec_if,
        .data_if = data_if,
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
    };
    esp_codec_dev_handle_t dev = esp_codec_dev_new(&dev_cfg);
    ESP_RETURN_ON_FALSE(dev, NULL, TAG, "codec dev failed");

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = SAMPLE_RATE,
        .channel = CHANNELS,
        .bits_per_sample = BITS_PER_SAMPLE,
    };
    esp_err_t err = esp_codec_dev_open(dev, &fs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "codec open failed: %s", esp_err_to_name(err));
        return NULL;
    }
    err = esp_codec_dev_set_out_vol(dev, output_volume);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "codec volume failed: %s", esp_err_to_name(err));
        return NULL;
    }
    ESP_LOGW(TAG, "ES8389 open ok addr=0x%02x volume=%d", ES8389_CODEC_DEFAULT_ADDR, output_volume);
    return dev;
}

static void button_adc_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = BUTTON_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &button_adc_unit));
    adc_oneshot_unit_init_cfg_t unit2_cfg = {
        .unit_id = ADC_UNIT_2,
    };
    esp_err_t unit2_err = adc_oneshot_new_unit(&unit2_cfg, &adc2_unit);
    ESP_LOGW(TAG, "adc2 init err=%s", esp_err_to_name(unit2_err));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = BUTTON_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    for (int ch = 0; ch < 8; ++ch)
    {
        esp_err_t err1 = adc_oneshot_config_channel(button_adc_unit, ch, &chan_cfg);
        esp_err_t err2 = adc2_unit ? adc_oneshot_config_channel(adc2_unit, ch, &chan_cfg) : ESP_ERR_INVALID_STATE;
        ESP_LOGW(TAG, "adc config ch%d adc1=%s adc2=%s", ch, esp_err_to_name(err1), esp_err_to_name(err2));
    }
    ESP_LOGW(TAG, "button ADC ready gpio=%d adc1_ch=%d", GPIO_BUTTON_ADC, BUTTON_ADC_CHANNEL);
}

static int read_button_adc_raw(void)
{
    int raw = 0;
    if (!button_adc_unit || adc_oneshot_read(button_adc_unit, BUTTON_ADC_CHANNEL, &raw) != ESP_OK)
        return -1;
    return raw;
}

static void log_adc_snapshot(void)
{
    char adc1[96] = {0};
    char adc2[96] = {0};
    size_t off1 = 0;
    size_t off2 = 0;
    for (int ch = 0; ch < 8; ++ch)
    {
        int raw1 = -1;
        int raw2 = -1;
        esp_err_t err1 = button_adc_unit ? adc_oneshot_read(button_adc_unit, ch, &raw1) : ESP_ERR_INVALID_STATE;
        esp_err_t err2 = adc2_unit ? adc_oneshot_read(adc2_unit, ch, &raw2) : ESP_ERR_INVALID_STATE;
        off1 += snprintf(adc1 + off1, sizeof(adc1) - off1, "%s%d:%d", ch ? " " : "", ch, err1 == ESP_OK ? raw1 : -1);
        off2 += snprintf(adc2 + off2, sizeof(adc2) - off2, "%s%d:%d", ch ? " " : "", ch, err2 == ESP_OK ? raw2 : -1);
    }
    ESP_LOGW(TAG, "adc1[%s] adc2[%s]", adc1, adc2);
}

static void volume_buttons_update(void)
{
    static int64_t next_repeat = 0;
    static int64_t press_start = 0;
    static int last_raw = -1;
    static int max_raw = 0;
    static bool fired = false;

    int raw = read_button_adc_raw();
    if (raw != last_raw && raw > 0)
    {
        ESP_LOGW(TAG, "button adc raw changed %d -> %d", last_raw, raw);
        last_raw = raw;
    }
    if (raw <= 0)
    {
        press_start = 0;
        max_raw = 0;
        fired = false;
        return;
    }

    int64_t now = esp_timer_get_time();
    if (press_start == 0)
    {
        press_start = now;
        max_raw = raw;
    }
    if (raw > max_raw)
        max_raw = raw;

    if (!fired && now - press_start < 60000)
        return;
    if (fired && now < next_repeat)
        return;

    int sample = max_raw;
    int bucket = 0;
    int delta = 0;
    if (sample >= VOL_PLUS_RAW_MIN && sample < VOL_PLUS_RAW_MAX)
    {
        bucket = 1;
        delta = 10;
    }
    else if (sample >= VOL_MINUS_RAW_MIN && sample < VOL_MINUS_RAW_MAX)
    {
        bucket = -1;
        delta = -10;
    }

    if (bucket == 0)
    {
        return;
    }

    output_volume += delta;
    if (output_volume < 0)
        output_volume = 0;
    if (output_volume > 100)
        output_volume = 100;
    esp_err_t err = codec_dev ? esp_codec_dev_set_out_vol(codec_dev, output_volume) : ESP_ERR_INVALID_STATE;
    ESP_LOGW(TAG, "button raw=%d max=%d key=%s vol=%d err=%s", raw, sample, delta > 0 ? "VOL+" : "VOL-", output_volume, esp_err_to_name(err));
    fired = true;
    next_repeat = now + 180000;
}

static void fill_sine(int16_t *buffer, size_t frames, int *phase)
{
    const int amplitude = 4000;
    const int period = SAMPLE_RATE / 440;
    for (size_t i = 0; i < frames; ++i)
    {
        float value = sinf((float)(*phase) * 2.0f * (float)M_PI / (float)period);
        int16_t sample = (int16_t)(value * amplitude);
        buffer[i * 2 + 0] = sample;
        buffer[i * 2 + 1] = sample;
        *phase = (*phase + 1) % period;
    }
}

void app_main(void)
{
    ESP_LOGW(TAG, "S31 Korvo ES8389 sine test");
    ESP_ERROR_CHECK(i2c_init());
    i2c_scan();
    ESP_ERROR_CHECK(i2s_init());
    button_adc_init();

    codec_dev = codec_init();
    if (!codec_dev)
    {
        ESP_LOGE(TAG, "Audio codec init failed");
        return;
    }

    ESP_LOGW(TAG, "PA_CTRL after codec open gpio=%d level=%d", GPIO_PA_ENABLE, gpio_get_level(GPIO_PA_ENABLE));
    ESP_LOGW(TAG, "playing 440Hz tone; press VOL+/VOL- to change volume");

    int phase = 0;
    uint32_t loops = 0;
    int16_t samples[512 * 2];
    while (true)
    {
        fill_sine(samples, 512, &phase);
        esp_err_t err = esp_codec_dev_write(codec_dev, samples, sizeof(samples));
        if (err != ESP_OK)
            ESP_LOGE(TAG, "write failed: %s", esp_err_to_name(err));
        volume_buttons_update();
        if ((++loops % 94) == 0)
        {
            int raw = read_button_adc_raw();
            ESP_LOGW(TAG, "tone alive loops=%" PRIu32 " adc_raw=%d idle_vol=%d", loops, raw, output_volume);
            log_adc_snapshot();
        }
    }
}
