// RGB LCD backend for ESP32-S31-Korvo-1.
//
// This is intentionally split into a scalar shadow-framebuffer path and
// capability-gated hardware paths. ESP32-S31 exposes new PIE/SIMD and PPA
// building blocks, but ESP-IDF master still marks parts of S31 LCD/PPA support
// as bring-up. The scalar path must always compile; hardware paths can take
// over when the matching SOC_* capability is enabled by IDF.

#pragma once

#include <stdint.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#include "soc/soc_caps.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#endif

#if defined(SOC_LCD_RGB_SUPPORTED) && SOC_LCD_RGB_SUPPORTED
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#endif

#if defined(SOC_PPA_SUPPORTED) && SOC_PPA_SUPPORTED
#include "driver/ppa.h"
#endif

#ifndef RG_ESP32S31_RGB_PCLK_HZ
#define RG_ESP32S31_RGB_PCLK_HZ (20 * 1000 * 1000)
#endif

#ifndef RG_GPIO_LCD_DISP
#define RG_GPIO_LCD_DISP GPIO_NUM_NC
#endif

#ifndef RG_RGB_LCD_PHYS_WIDTH
#define RG_RGB_LCD_PHYS_WIDTH RG_SCREEN_WIDTH
#endif

#ifndef RG_RGB_LCD_PHYS_HEIGHT
#define RG_RGB_LCD_PHYS_HEIGHT RG_SCREEN_HEIGHT
#endif

#define RGB_LCD_PIXEL_COUNT (RG_RGB_LCD_PHYS_WIDTH * RG_RGB_LCD_PHYS_HEIGHT)
#define RGB_LCD_FRAME_BYTES (RGB_LCD_PIXEL_COUNT * sizeof(uint16_t))

static uint16_t *lcd_framebuffer;
static uint16_t lcd_buffer[LCD_BUFFER_LENGTH] __attribute__((aligned(16)));
static int lcd_window_left;
static int lcd_window_top;
static int lcd_window_width;
static int lcd_window_height;
static int lcd_window_cursor;

#if defined(SOC_LCD_RGB_SUPPORTED) && SOC_LCD_RGB_SUPPORTED
static esp_lcd_panel_handle_t lcd_panel;
#endif

#if defined(SOC_PPA_SUPPORTED) && SOC_PPA_SUPPORTED
static ppa_client_handle_t lcd_ppa_client;
#endif

static const char *rgb_lcd_err_name(esp_err_t err)
{
#ifdef ESP_PLATFORM
    return esp_err_to_name(err);
#else
    return err == ESP_OK ? "ESP_OK" : "ESP_ERR";
#endif
}

static inline uint16_t rgb565_be_to_native(uint16_t pixel)
{
    return (uint16_t)((pixel << 8) | (pixel >> 8));
}

static inline void rgb565_copy_be_to_native(uint16_t *dst, const uint16_t *src, size_t count)
{
#if defined(CONFIG_IDF_TARGET_ESP32S31) && defined(SOC_CPU_HAS_PIE) && SOC_CPU_HAS_PIE
    // S31 has PIE/SIMD support, but ESP-IDF currently exposes it as GCC
    // assembly rather than stable C intrinsics. Keep this hot loop isolated so
    // a Core-1 PIE implementation can replace the scalar body after board-side
    // validation. Buffers are 16-byte aligned where retro-go owns them.
#endif
    for (size_t i = 0; i < count; ++i)
        dst[i] = rgb565_be_to_native(src[i]);
}

static void lcd_init(void)
{
    lcd_framebuffer = rg_alloc(RGB_LCD_FRAME_BYTES, MEM_SLOW);
    RG_ASSERT(lcd_framebuffer != NULL, "No RGB framebuffer");
    RG_LOGI("RGB LCD shadow framebuffer ready (%dx%d physical, %dx%d logical, %d bytes)",
        RG_RGB_LCD_PHYS_WIDTH, RG_RGB_LCD_PHYS_HEIGHT,
        RG_SCREEN_WIDTH, RG_SCREEN_HEIGHT, RGB_LCD_FRAME_BYTES);

#if defined(SOC_LCD_RGB_SUPPORTED) && SOC_LCD_RGB_SUPPORTED
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = RG_ESP32S31_RGB_PCLK_HZ,
            .h_res = RG_RGB_LCD_PHYS_WIDTH,
            .v_res = RG_RGB_LCD_PHYS_HEIGHT,
            .hsync_pulse_width = 1,
            .hsync_back_porch = 40,
            .hsync_front_porch = 20,
            .vsync_pulse_width = 1,
            .vsync_back_porch = 10,
            .vsync_front_porch = 5,
            .flags.pclk_active_neg = true,
        },
        .data_width = 16,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .out_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = 2,
        .dma_burst_size = 64,
        .hsync_gpio_num = RG_GPIO_LCD_HSYNC,
        .vsync_gpio_num = RG_GPIO_LCD_VSYNC,
        .de_gpio_num = RG_GPIO_LCD_DE,
        .pclk_gpio_num = RG_GPIO_LCD_PCLK,
        .disp_gpio_num = RG_GPIO_LCD_DISP,
        .data_gpio_nums = {
            RG_GPIO_LCD_D0, RG_GPIO_LCD_D1, RG_GPIO_LCD_D2, RG_GPIO_LCD_D3,
            RG_GPIO_LCD_D4, RG_GPIO_LCD_D5, RG_GPIO_LCD_D6, RG_GPIO_LCD_D7,
            RG_GPIO_LCD_D8, RG_GPIO_LCD_D9, RG_GPIO_LCD_D10, RG_GPIO_LCD_D11,
            RG_GPIO_LCD_D12, RG_GPIO_LCD_D13, RG_GPIO_LCD_D14, RG_GPIO_LCD_D15,
        },
        .flags.fb_in_psram = true,
    };

    RG_LOGI("RGB LCD config: pclk=%dHz h=%d/%d/%d v=%d/%d/%d disp=%d",
        RG_ESP32S31_RGB_PCLK_HZ,
        (int)panel_config.timings.hsync_pulse_width,
        (int)panel_config.timings.hsync_back_porch,
        (int)panel_config.timings.hsync_front_porch,
        (int)panel_config.timings.vsync_pulse_width,
        (int)panel_config.timings.vsync_back_porch,
        (int)panel_config.timings.vsync_front_porch,
        RG_GPIO_LCD_DISP);

    esp_err_t err = esp_lcd_new_rgb_panel(&panel_config, &lcd_panel);
    RG_LOGI("esp_lcd_new_rgb_panel: %s", rgb_lcd_err_name(err));

    if (err == ESP_OK)
    {
        err = esp_lcd_panel_reset(lcd_panel);
        RG_LOGI("esp_lcd_panel_reset: %s", rgb_lcd_err_name(err));
    }

    if (err == ESP_OK)
    {
        err = esp_lcd_panel_init(lcd_panel);
        RG_LOGI("esp_lcd_panel_init: %s", rgb_lcd_err_name(err));
    }

    if (err == ESP_OK && (int)RG_GPIO_LCD_DISP >= 0)
    {
        esp_err_t disp_err = esp_lcd_panel_disp_on_off(lcd_panel, true);
        RG_LOGI("esp_lcd_panel_disp_on_off: %s", rgb_lcd_err_name(disp_err));
        if (disp_err != ESP_OK && disp_err != ESP_ERR_NOT_SUPPORTED)
            err = disp_err;
    }
    else if (err == ESP_OK)
    {
        RG_LOGI("RGB LCD DISP GPIO is not configured; skipping display on/off call");
    }

    if (err == ESP_OK)
    {
        void *fb0 = NULL;
        void *fb1 = NULL;
        esp_err_t fb_err = esp_lcd_rgb_panel_get_frame_buffer(lcd_panel, 2, &fb0, &fb1);
        RG_LOGI("esp_lcd_rgb_panel_get_frame_buffer: %s ptr=%p", rgb_lcd_err_name(fb_err), fb0);
        RG_LOGI("esp_lcd_rgb_panel_get_frame_buffer second ptr=%p", fb1);
        RG_LOGI("RGB LCD panel initialized (%dx%d)", RG_RGB_LCD_PHYS_WIDTH, RG_RGB_LCD_PHYS_HEIGHT);
    }
    else
    {
        RG_LOGW("RGB LCD panel init failed (%s), keeping shadow framebuffer only", rgb_lcd_err_name(err));
        if (lcd_panel)
        {
            esp_lcd_panel_del(lcd_panel);
            lcd_panel = NULL;
        }
    }
#else
    RG_LOGW("SOC_LCD_RGB_SUPPORTED is disabled; keeping shadow framebuffer only");
#endif

#if defined(SOC_PPA_SUPPORTED) && SOC_PPA_SUPPORTED
    // PPA is reserved for future full-frame scale/rotate/fill. The current
    // S31 IDF capability macros may leave this disabled even when the register
    // block exists, so never rely on it for correctness.
    ppa_client_config_t ppa_config = {
        .oper_type = PPA_OPERATION_SRM,
    };
    if (ppa_register_client(&ppa_config, &lcd_ppa_client) != ESP_OK)
        lcd_ppa_client = NULL;
#endif
}

static void lcd_deinit(void)
{
#if defined(SOC_PPA_SUPPORTED) && SOC_PPA_SUPPORTED
    if (lcd_ppa_client)
    {
        ppa_unregister_client(lcd_ppa_client);
        lcd_ppa_client = NULL;
    }
#endif
#if defined(SOC_LCD_RGB_SUPPORTED) && SOC_LCD_RGB_SUPPORTED
    if (lcd_panel)
    {
        esp_lcd_panel_del(lcd_panel);
        lcd_panel = NULL;
    }
#endif
    lcd_framebuffer = NULL;
}

static void lcd_set_backlight(float percent)
{
#ifdef ESP_PLATFORM
#ifdef RG_GPIO_LCD_BCKL
    const gpio_num_t gpio = RG_GPIO_LCD_BCKL;
    if ((int)gpio >= 0)
    {
        gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
#ifdef RG_GPIO_LCD_BCKL_INVERT
        gpio_set_level(gpio, percent > 0.f ? 0 : 1);
#else
        gpio_set_level(gpio, percent > 0.f ? 1 : 0);
#endif
        RG_LOGI("RGB LCD backlight GPIO %d set to %d%%", gpio, (int)(percent * 100.f));
        return;
    }
#endif
#endif
    RG_LOGI("RGB LCD backlight GPIO is not configured");
}

static void lcd_set_window(int left, int top, int width, int height)
{
    lcd_window_left = left;
    lcd_window_top = top;
    lcd_window_width = width;
    lcd_window_height = height;
    lcd_window_cursor = 0;
}

static inline uint16_t *lcd_get_buffer(size_t length)
{
    (void)length;
    return lcd_buffer;
}

static inline void lcd_write_pixel(int x, int y, uint16_t pixel)
{
#if defined(RG_RGB_LCD_ROTATE_CW) && RG_RGB_LCD_ROTATE_CW
    const int phys_x = RG_RGB_LCD_PHYS_WIDTH - 1 - y;
    const int phys_y = x;
#elif defined(RG_RGB_LCD_ROTATE_CCW) && RG_RGB_LCD_ROTATE_CCW
    const int phys_x = y;
    const int phys_y = RG_RGB_LCD_PHYS_HEIGHT - 1 - x;
#else
    const int phys_x = x;
    const int phys_y = y;
#endif
    if ((unsigned)phys_x < RG_RGB_LCD_PHYS_WIDTH && (unsigned)phys_y < RG_RGB_LCD_PHYS_HEIGHT)
        lcd_framebuffer[phys_y * RG_RGB_LCD_PHYS_WIDTH + phys_x] = pixel;
}

static inline void lcd_fill_rect_logical(int left, int top, int width, int height, uint16_t color)
{
    int right = left + width;
    int bottom = top + height;
    if (left < 0)
        left = 0;
    if (top < 0)
        top = 0;
    if (right > RG_SCREEN_WIDTH)
        right = RG_SCREEN_WIDTH;
    if (bottom > RG_SCREEN_HEIGHT)
        bottom = RG_SCREEN_HEIGHT;

    for (int y = top; y < bottom; ++y)
        for (int x = left; x < right; ++x)
            lcd_write_pixel(x, y, color);
}

static inline void lcd_fill_circle_logical(int cx, int cy, int radius, uint16_t color)
{
    const int radius2 = radius * radius;
    for (int y = cy - radius; y <= cy + radius; ++y)
    {
        for (int x = cx - radius; x <= cx + radius; ++x)
        {
            const int dx = x - cx;
            const int dy = y - cy;
            if (dx * dx + dy * dy <= radius2)
                lcd_write_pixel(x, y, color);
        }
    }
}

static inline void lcd_draw_circle_ring_logical(int cx, int cy, int radius, int thickness, uint16_t color)
{
    const int outer2 = radius * radius;
    const int inner = radius - thickness;
    const int inner2 = inner * inner;
    for (int y = cy - radius; y <= cy + radius; ++y)
    {
        for (int x = cx - radius; x <= cx + radius; ++x)
        {
            const int dx = x - cx;
            const int dy = y - cy;
            const int distance2 = dx * dx + dy * dy;
            if (distance2 <= outer2 && distance2 >= inner2)
                lcd_write_pixel(x, y, color);
        }
    }
}

static inline uint8_t lcd_label_glyph(char c, int row)
{
    switch (c)
    {
    case 'A': { static const uint8_t g[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}; return g[row]; }
    case 'B': { static const uint8_t g[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}; return g[row]; }
    case 'C': { static const uint8_t g[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}; return g[row]; }
    case 'E': { static const uint8_t g[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}; return g[row]; }
    case 'L': { static const uint8_t g[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}; return g[row]; }
    case 'M': { static const uint8_t g[7] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}; return g[row]; }
    case 'N': { static const uint8_t g[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}; return g[row]; }
    case 'R': { static const uint8_t g[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}; return g[row]; }
    case 'S': { static const uint8_t g[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}; return g[row]; }
    case 'T': { static const uint8_t g[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}; return g[row]; }
    case 'U': { static const uint8_t g[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}; return g[row]; }
    case 'X': { static const uint8_t g[7] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}; return g[row]; }
    case 'Y': { static const uint8_t g[7] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}; return g[row]; }
    default: return 0;
    }
}

static inline void lcd_draw_label_char_logical(int left, int top, char c, int scale, uint16_t color)
{
    for (int row = 0; row < 7; ++row)
    {
        uint8_t bits = lcd_label_glyph(c, row);
        for (int col = 0; col < 5; ++col)
        {
            if (bits & (1 << (4 - col)))
                lcd_fill_rect_logical(left + col * scale, top + row * scale, scale, scale, color);
        }
    }
}

static inline void lcd_draw_label_logical(int cx, int cy, const char *text, int scale, uint16_t color)
{
    int chars = 0;
    for (const char *ptr = text; *ptr; ++ptr)
        chars++;

    const int char_width = 5 * scale;
    const int gap = scale;
    const int width = chars * char_width + (chars > 0 ? (chars - 1) * gap : 0);
    int x = cx - width / 2;
    const int y = cy - (7 * scale) / 2;

    for (const char *ptr = text; *ptr; ++ptr)
    {
        lcd_draw_label_char_logical(x, y, *ptr, scale, color);
        x += char_width + gap;
    }
}

static inline void lcd_draw_gba_touch_overlay(void)
{
#if defined(RG_TOUCH_OVERLAY_GBA) && RG_TOUCH_OVERLAY_GBA
#ifndef RG_TOUCH_OVERLAY_TOP
#define RG_TOUCH_OVERLAY_TOP (RG_SCREEN_HEIGHT / 2)
#endif
    const uint16_t panel = 0x1082;
    const uint16_t line = 0x39E7;
    const uint16_t control = 0x4208;
    const uint16_t control_dark = 0x2104;
    const uint16_t accent = 0x7BEF;
    const uint16_t label = 0xFFFF;
    const uint16_t a_color = 0xE8E4;
    const uint16_t b_color = 0xFD20;
    const uint16_t x_color = 0x04BF;
    const uint16_t y_color = 0x4D8A;

    lcd_fill_rect_logical(0, RG_TOUCH_OVERLAY_TOP, RG_SCREEN_WIDTH, RG_SCREEN_HEIGHT - RG_TOUCH_OVERLAY_TOP, panel);
    lcd_fill_rect_logical(0, RG_TOUCH_OVERLAY_TOP, RG_SCREEN_WIDTH, 2, line);

    lcd_fill_rect_logical(18, 330, 175, 48, control_dark);
    lcd_fill_rect_logical(287, 330, 175, 48, control_dark);
    lcd_fill_rect_logical(22, 334, 167, 40, control);
    lcd_fill_rect_logical(291, 334, 167, 40, control);
    lcd_draw_label_logical(105, 354, "L", 3, label);
    lcd_draw_label_logical(375, 354, "R", 3, label);

    lcd_fill_rect_logical(76, 456, 88, 88, control);
    lcd_fill_rect_logical(76, 620, 88, 88, control);
    lcd_fill_rect_logical(24, 544, 88, 88, control);
    lcd_fill_rect_logical(128, 544, 88, 88, control);
    lcd_fill_rect_logical(104, 552, 32, 72, panel);
    lcd_fill_rect_logical(84, 484, 72, 32, accent);
    lcd_fill_rect_logical(84, 648, 72, 32, accent);
    lcd_fill_rect_logical(52, 552, 32, 72, accent);
    lcd_fill_rect_logical(156, 552, 32, 72, accent);

    lcd_fill_circle_logical(330, 648, 50, control_dark);
    lcd_fill_circle_logical(433, 612, 50, control_dark);
    lcd_fill_circle_logical(330, 648, 42, b_color);
    lcd_fill_circle_logical(433, 612, 42, a_color);
    lcd_draw_label_logical(330, 648, "B", 4, label);
    lcd_draw_label_logical(433, 612, "A", 4, label);

    lcd_fill_circle_logical(330, 524, 45, control_dark);
    lcd_fill_circle_logical(433, 481, 45, control_dark);
    lcd_fill_circle_logical(330, 524, 37, y_color);
    lcd_fill_circle_logical(433, 481, 37, x_color);
    lcd_draw_label_logical(330, 524, "Y", 4, label);
    lcd_draw_label_logical(433, 481, "X", 4, label);

    lcd_fill_rect_logical(110, 724, 120, 54, control);
    lcd_fill_rect_logical(250, 724, 120, 54, control);
    lcd_draw_label_logical(170, 751, "SELECT", 2, label);
    lcd_draw_label_logical(310, 751, "START", 2, label);

    lcd_fill_rect_logical(190, 330, 100, 80, accent);
    lcd_draw_label_logical(240, 370, "MENU", 2, label);
#endif
}

static inline void lcd_send_buffer(uint16_t *buffer, size_t length)
{
    if (!length || !buffer || !lcd_framebuffer || lcd_window_width <= 0 || lcd_window_height <= 0)
        return;

    for (size_t done = 0; done < length;)
    {
        int rel_y = lcd_window_cursor / lcd_window_width;
        int rel_x = lcd_window_cursor - rel_y * lcd_window_width;
        int dst_x = lcd_window_left + rel_x;
        int dst_y = lcd_window_top + rel_y;
        int run = lcd_window_width - rel_x;

        if (run > (int)(length - done))
            run = (int)(length - done);

#if defined(RG_RGB_LCD_ROTATE_CW) && RG_RGB_LCD_ROTATE_CW
        for (int i = 0; i < run; ++i)
            lcd_write_pixel(dst_x + i, dst_y, rgb565_be_to_native(buffer[done + i]));
#elif defined(RG_RGB_LCD_ROTATE_CCW) && RG_RGB_LCD_ROTATE_CCW
        for (int i = 0; i < run; ++i)
            lcd_write_pixel(dst_x + i, dst_y, rgb565_be_to_native(buffer[done + i]));
#else
        if ((unsigned)dst_y < RG_SCREEN_HEIGHT && (unsigned)dst_x < RG_SCREEN_WIDTH)
        {
            int clipped = run;
            if (dst_x + clipped > RG_SCREEN_WIDTH)
                clipped = RG_SCREEN_WIDTH - dst_x;
            if (clipped > 0)
            {
                uint16_t *dst = &lcd_framebuffer[dst_y * RG_RGB_LCD_PHYS_WIDTH + dst_x];
                rgb565_copy_be_to_native(dst, &buffer[done], clipped);
            }
        }
#endif

        lcd_window_cursor += run;
        done += run;
    }
}

static void lcd_sync(void)
{
#if defined(SOC_LCD_RGB_SUPPORTED) && SOC_LCD_RGB_SUPPORTED
    if (lcd_framebuffer)
        lcd_draw_gba_touch_overlay();
    if (lcd_panel && lcd_framebuffer)
        esp_lcd_panel_draw_bitmap(lcd_panel, 0, 0, RG_RGB_LCD_PHYS_WIDTH, RG_RGB_LCD_PHYS_HEIGHT, lcd_framebuffer);
#endif
}

const rg_display_driver_t rg_display_driver_rgb_lcd = {
    .name = "rgb_lcd",
};
