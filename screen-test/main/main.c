#include <rg_system.h>
#include <stdio.h>

#ifdef ESP_PLATFORM
#include <esp_system.h>
#endif

static const uint16_t test_colors[] = {
    C_RGB(255, 0, 0),
    C_RGB(0, 255, 0),
    C_RGB(0, 0, 255),
    C_RGB(255, 255, 255),
    C_RGB(0, 0, 0),
    C_RGB(255, 255, 0),
    C_RGB(0, 255, 255),
    C_RGB(255, 0, 255),
};

static uint16_t *frame;

static void draw_color_bars(int phase)
{
    const int width = rg_display_get_width();
    const int height = rg_display_get_height();
    const int count = (int)RG_COUNT(test_colors);
    const int bar_width = (width + count - 1) / count;
    const int block_left = (phase * 19) % width;
    const int block_top = height / 3;
    const int block_width = width / 8;
    const int block_height = height / 3;

    for (int y = 0; y < height; ++y)
    {
        uint16_t *row = frame + y * width;
        for (int x = 0; x < width; ++x)
        {
            int color_index = x / bar_width;
            if (color_index >= count)
                color_index = count - 1;
            row[x] = test_colors[(color_index + phase) % count];
        }
    }

    for (int y = block_top; y < block_top + block_height && y < height; ++y)
    {
        uint16_t *row = frame + y * width;
        for (int x = block_left; x < block_left + block_width && x < width; ++x)
            row[x] = C_RGB(255, 255, 255);
    }

    rg_display_write_rect(0, 0, width, height, width * sizeof(uint16_t), frame, 0);
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
#if RG_BUILD_RELEASE
        .isRelease = true,
#else
        .isRelease = false,
#endif
        .logLevel = RG_LOG_DEBUG,
        .initialized = true,
    };

    printf("\n========================================================\n");
    printf("%s %s (%s)\n", app->name, app->version, app->buildDate);
    printf(" built for: %s. type: %s\n", RG_TARGET_NAME, app->isRelease ? "release" : "dev");
    printf(" purpose: RGB LCD bring-up\n");
    printf("========================================================\n\n");

    rg_settings_init(true);
    rg_display_init();
    rg_display_set_backlight(100);
    frame = rg_alloc(rg_display_get_width() * rg_display_get_height() * sizeof(uint16_t), MEM_SLOW);
    RG_ASSERT(frame != NULL, "No test frame buffer");

    for (int phase = 0;; ++phase)
    {
        if ((phase % 30) == 0)
            RG_LOGI("Drawing RGB LCD test pattern phase %d", phase);
        draw_color_bars(phase);
        rg_task_delay(33);
    }
}
