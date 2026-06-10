#include <rg_system.h>
#include <rg_input.h>
#include <stdio.h>
#include <string.h>

static uint16_t *frame;
static uint32_t last_nonzero_state;

#define PANEL_WIDTH  480
#define PANEL_HEIGHT 400

static void append_key_names(uint32_t state, char *buffer, size_t buffer_size)
{
    buffer[0] = 0;
    for (int i = 0; i < RG_KEY_COUNT; ++i)
    {
        const rg_key_t key = (rg_key_t)(1u << i);
        if (!(state & key))
            continue;
        if (buffer[0])
            strlcat(buffer, "|", buffer_size);
        strlcat(buffer, rg_input_get_key_name(key), buffer_size);
    }
    if (!buffer[0])
        strlcat(buffer, "None", buffer_size);
}

static uint8_t glyph_rows(char ch, int row)
{
    static const uint8_t digits[10][7] = {
        {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
        {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
        {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
        {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E},
        {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
        {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E},
        {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
        {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
        {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
        {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    };
    static const uint8_t letters[26][7] = {
        {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
        {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
        {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
        {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
        {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
        {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
        {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
        {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
        {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
        {0x01,0x01,0x01,0x01,0x11,0x11,0x0E},
        {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
        {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
        {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
        {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
        {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
        {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
        {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
        {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
        {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
        {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
        {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
        {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
        {0x11,0x11,0x11,0x15,0x15,0x15,0x0A},
        {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
        {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
        {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    };

    if (ch >= 'a' && ch <= 'z')
        ch = ch - 'a' + 'A';
    if (ch >= '0' && ch <= '9')
        return digits[ch - '0'][row];
    if (ch >= 'A' && ch <= 'Z')
        return letters[ch - 'A'][row];
    switch (ch)
    {
    case ':': return row == 2 || row == 4 ? 0x04 : 0x00;
    case '|': return 0x04;
    case '-': return row == 3 ? 0x1F : 0x00;
    case '=': return row == 2 || row == 4 ? 0x1F : 0x00;
    default: return 0x00;
    }
}

static void fill_rect(int left, int top, int width, int height, uint16_t color)
{
    int right = RG_MIN(left + width, PANEL_WIDTH);
    int bottom = RG_MIN(top + height, PANEL_HEIGHT);
    left = RG_MAX(left, 0);
    top = RG_MAX(top, 0);

    for (int y = top; y < bottom; ++y)
        for (int x = left; x < right; ++x)
            frame[y * PANEL_WIDTH + x] = color;
}

static void draw_text(int x, int y, const char *text, int scale, uint16_t color)
{
    int cx = x;
    for (const char *ptr = text; *ptr; ++ptr)
    {
        if (*ptr == '\n')
        {
            cx = x;
            y += 8 * scale;
            continue;
        }
        for (int row = 0; row < 7; ++row)
        {
            uint8_t bits = glyph_rows(*ptr, row);
            for (int col = 0; col < 5; ++col)
            {
                if (bits & (1 << (4 - col)))
                    fill_rect(cx + col * scale, y + row * scale, scale, scale, color);
            }
        }
        cx += 6 * scale;
    }
}

static void draw_status(uint32_t state)
{
    if (!frame)
        return;

    const uint16_t bg = C_RGB(8, 10, 15);
    const uint16_t panel = C_RGB(26, 32, 42);
    const uint16_t active = C_RGB(52, 190, 120);
    const uint16_t inactive = C_RGB(46, 52, 62);
    const uint16_t white = C_RGB(245, 245, 245);
    const uint16_t yellow = C_RGB(255, 220, 70);
    const uint16_t orange = C_RGB(255, 150, 40);
    const uint16_t dim = C_RGB(180, 190, 205);

    for (int i = 0; i < PANEL_WIDTH * PANEL_HEIGHT; ++i)
        frame[i] = bg;

    char current_names[160];
    char last_names[160];
    char line[192];
    append_key_names(state, current_names, sizeof(current_names));
    append_key_names(last_nonzero_state, last_names, sizeof(last_names));

    fill_rect(0, 0, PANEL_WIDTH, 150, panel);
    draw_text(16, 12, "INPUT TEST", 3, yellow);
    snprintf(line, sizeof(line), "mask=0x%04X", (unsigned)(state & 0xFFFF));
    draw_text(16, 52, line, 2, white);
    snprintf(line, sizeof(line), "CURRENT: %s", current_names);
    draw_text(16, 82, line, 2, white);
    snprintf(line, sizeof(line), "LAST: %s", last_names);
    draw_text(16, 116, line, 2, orange);

    for (int i = 0; i < RG_KEY_COUNT; ++i)
    {
        const int x = 20 + (i % 2) * 220;
        const int y = 164 + (i / 2) * 32;
        const int w = 190;
        const int h = 26;
        const uint16_t color = (state & (1u << i)) ? active : inactive;
        fill_rect(x, y, w, h, color);
        draw_text(x + 8, y + 7, rg_input_get_key_name((rg_key_t)(1u << i)), 1,
            (state & (1u << i)) ? C_RGB(0, 0, 0) : dim);
    }

    rg_display_write_rect(0, 0, PANEL_WIDTH, PANEL_HEIGHT, PANEL_WIDTH * sizeof(uint16_t), frame, 0);
}

static void log_state(uint32_t state)
{
    char names[160];
    append_key_names(state, names, sizeof(names));
    RG_LOGI("key mask=0x%04X bits=" PRINTF_BINARY_16 " names=%s",
        (unsigned)(state & 0xFFFF), PRINTF_BINVAL_16(state & 0xFFFF), names);
}

void app_main(void)
{
    rg_app_t *app = rg_system_get_app();
    *app = (rg_app_t){
        .name = RG_PROJECT_APP,
        .version = RG_PROJECT_VER,
        .buildDate = RG_BUILD_DATE,
        .buildInfo = RG_BUILD_INFO,
        .configNs = RG_PROJECT_APP,
        .bootArgs = "",
        .romPath = "",
        .speed = 1.f,
        .sampleRate = 0,
        .tickRate = 60,
        .frameTime = 1000000 / 60,
        .frameskip = 1,
        .tickTimeout = 3000000,
        .enWatchdog = false,
        .isColdBoot = true,
        .isLauncher = false,
        .isRelease = false,
        .logLevel = RG_LOG_DEBUG,
        .initialized = true,
    };

    printf("\n========================================================\n");
    printf("%s %s (%s)\n", app->name, app->version, app->buildDate);
    printf(" built for: %s\n", RG_TARGET_NAME);
    printf(" purpose: input key mask test\n");
    printf("========================================================\n\n");

    rg_settings_init(true);
    rg_display_init();
    rg_display_set_backlight(100);
    rg_input_init();

    frame = rg_alloc(PANEL_WIDTH * PANEL_HEIGHT * sizeof(uint16_t), MEM_SLOW);
    if (!frame)
        RG_LOGW("No framebuffer for input-test display");

    for (int i = 0; i < RG_KEY_COUNT; ++i)
    {
        const rg_key_t key = (rg_key_t)(1u << i);
        RG_LOGI("key bit %02d mask=0x%04X name=%s", i, (unsigned)key, rg_input_get_key_name(key));
    }

    uint32_t last_state = UINT32_MAX;
    for (;;)
    {
        const uint32_t state = rg_input_read_gamepad() & 0xFFFF;
        if (state != last_state)
        {
            if (state)
                last_nonzero_state = state;
            log_state(state);
            draw_status(state);
            last_state = state;
        }
        rg_task_delay(20);
    }
}
