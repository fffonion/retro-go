#include <rg_system.h>
#include <rg_i2c.h>
#include <stdio.h>
#include <string.h>

#define GOODIX_REG_PRODUCT_ID 0x8140
#define GOODIX_REG_STATUS     0x814E
#define GOODIX_REG_POINTS     0x8150
#define GOODIX_STATUS_READY   0x80
#define GOODIX_MAX_POINTS     5

typedef struct
{
    const char *name;
    int x_min;
    int x_max;
    int y_min;
    int y_max;
    uint16_t color;
} zone_t;

typedef struct
{
    int raw_x;
    int raw_y;
    int x;
    int y;
} point_t;

static const zone_t zones[] = {
    {"L",      0, 210, 400, 500, C_RGB(70, 90, 130)},
    {"R",    270, 479, 400, 500, C_RGB(70, 90, 130)},
    {"UP",    80, 160, 505, 595, C_RGB(80, 80, 120)},
    {"DOWN",  80, 160, 675, 770, C_RGB(80, 80, 120)},
    {"LEFT",  15,  95, 595, 675, C_RGB(80, 80, 120)},
    {"RIGHT",145, 225, 595, 675, C_RGB(80, 80, 120)},
    {"B",    285, 390, 590, 720, C_RGB(130, 70, 90)},
    {"A",    365, 479, 555, 685, C_RGB(150, 80, 100)},
    {"Y",    280, 375, 505, 610, C_RGB(90, 70, 120)},
    {"X",    380, 479, 485, 595, C_RGB(90, 70, 120)},
    {"SEL",  145, 230, 725, 795, C_RGB(90, 90, 90)},
    {"START",250, 335, 725, 795, C_RGB(90, 90, 90)},
    {"MENU", 190, 290, 405, 475, C_RGB(140, 130, 80)},
};

static uint8_t touch_addr;
static uint16_t *frame;

static int local_abs(int value)
{
    return value < 0 ? -value : value;
}

static bool touch_i2c_read(uint8_t addr, uint16_t reg, void *read_data, size_t read_len)
{
    const uint8_t reg_data[2] = {reg >> 8, reg & 0xFF};
    return rg_i2c_write(addr, -1, reg_data, sizeof(reg_data)) &&
        rg_i2c_read(addr, -1, read_data, read_len);
}

static bool touch_i2c_write_byte(uint8_t addr, uint16_t reg, uint8_t value)
{
    const uint8_t data[3] = {reg >> 8, reg & 0xFF, value};
    return rg_i2c_write(addr, -1, data, sizeof(data));
}

static bool touch_probe(uint8_t addr)
{
    uint8_t product_id[4] = {0};
    if (!touch_i2c_read(addr, GOODIX_REG_PRODUCT_ID, product_id, sizeof(product_id)))
        return false;

    touch_addr = addr;
    RG_LOGI("Goodix probe ok addr=0x%02X id=%c%c%c%c",
        addr,
        product_id[0] ? product_id[0] : '?',
        product_id[1] ? product_id[1] : '?',
        product_id[2] ? product_id[2] : '?',
        product_id[3] ? product_id[3] : '?');
    return true;
}

static void transform_point(point_t *point)
{
    const int raw_x = point->raw_x;
    const int raw_y = point->raw_y;
#if defined(RG_GAMEPAD_TOUCH_ROTATE_CW) && RG_GAMEPAD_TOUCH_ROTATE_CW
    point->x = raw_y;
    point->y = RG_GAMEPAD_TOUCH_PHYS_WIDTH - 1 - raw_x;
#elif defined(RG_GAMEPAD_TOUCH_ROTATE_CCW) && RG_GAMEPAD_TOUCH_ROTATE_CCW
    point->x = RG_GAMEPAD_TOUCH_PHYS_HEIGHT - 1 - raw_y;
    point->y = raw_x;
#else
    point->x = raw_x;
    point->y = raw_y;
#endif
#if RG_GAMEPAD_TOUCH_SWAP_XY
    int t = point->x;
    point->x = point->y;
    point->y = t;
#endif
#if RG_GAMEPAD_TOUCH_INVERT_X
    point->x = RG_SCREEN_WIDTH - 1 - point->x;
#endif
#if RG_GAMEPAD_TOUCH_INVERT_Y
    point->y = RG_SCREEN_HEIGHT - 1 - point->y;
#endif
}

static int read_points(point_t *points)
{
    uint8_t status = 0;
    if (!touch_addr || !touch_i2c_read(touch_addr, GOODIX_REG_STATUS, &status, 1))
        return -1;

    if (!(status & GOODIX_STATUS_READY))
        return 0;

    const int count = RG_MIN(status & 0x0F, GOODIX_MAX_POINTS);
    uint8_t data[GOODIX_MAX_POINTS * 8] = {0};

    if (count && touch_i2c_read(touch_addr, GOODIX_REG_POINTS, data, count * 8))
    {
        for (int i = 0; i < count; ++i)
        {
            const uint8_t *point_data = &data[i * 8];
            points[i].raw_x = point_data[0] | (point_data[1] << 8);
            points[i].raw_y = point_data[2] | (point_data[3] << 8);
            transform_point(&points[i]);
        }
    }

    touch_i2c_write_byte(touch_addr, GOODIX_REG_STATUS, 0);
    return count;
}

static void fill_rect(int left, int top, int width, int height, uint16_t color)
{
    int right = RG_MIN(left + width, rg_display_get_width());
    int bottom = RG_MIN(top + height, rg_display_get_height());
    left = RG_MAX(left, 0);
    top = RG_MAX(top, 0);

    for (int y = top; y < bottom; ++y)
        for (int x = left; x < right; ++x)
            frame[y * rg_display_get_width() + x] = color;
}

static void fill_circle(int cx, int cy, int radius, uint16_t color)
{
    const int radius2 = radius * radius;
    for (int y = cy - radius; y <= cy + radius; ++y)
    {
        for (int x = cx - radius; x <= cx + radius; ++x)
        {
            if ((unsigned)x >= (unsigned)rg_display_get_width() ||
                (unsigned)y >= (unsigned)rg_display_get_height())
                continue;
            const int dx = x - cx;
            const int dy = y - cy;
            if (dx * dx + dy * dy <= radius2)
                frame[y * rg_display_get_width() + x] = color;
        }
    }
}

static void draw_frame(const point_t *points, int count)
{
    const int width = rg_display_get_width();
    const int height = rg_display_get_height();
    const uint16_t bg = C_RGB(14, 17, 22);
    const uint16_t play = C_RGB(60, 140, 130);
    const uint16_t white = C_RGB(255, 255, 255);

    for (int i = 0; i < width * height; ++i)
        frame[i] = bg;

    fill_rect(0, 0, width, 400, play);
    fill_rect(0, 398, width, 4, C_RGB(220, 220, 220));

    for (size_t i = 0; i < RG_COUNT(zones); ++i)
    {
        const zone_t *zone = &zones[i];
        fill_rect(zone->x_min, zone->y_min,
            zone->x_max - zone->x_min + 1,
            zone->y_max - zone->y_min + 1,
            zone->color);
    }

    for (int i = 0; i < count; ++i)
    {
        fill_circle(points[i].x, points[i].y, 18, white);
        fill_circle(points[i].x, points[i].y, 8, C_RGB(255, 80, 40));
    }

    rg_display_write_rect(0, 0, width, height, width * sizeof(uint16_t), frame, 0);
}

static void log_points(const point_t *points, int count)
{
    char keys[96] = {0};
    for (int p = 0; p < count; ++p)
    {
        for (size_t i = 0; i < RG_COUNT(zones); ++i)
        {
            const zone_t *zone = &zones[i];
            if (points[p].x >= zone->x_min && points[p].x <= zone->x_max &&
                points[p].y >= zone->y_min && points[p].y <= zone->y_max)
            {
                if (keys[0])
                    strlcat(keys, "|", sizeof(keys));
                strlcat(keys, zone->name, sizeof(keys));
            }
        }
    }

    if (count <= 0)
    {
        RG_LOGI("touch count=%d", count);
        return;
    }

    RG_LOGI("touch count=%d raw0=(%d,%d) logical0=(%d,%d) keys=%s",
        count, points[0].raw_x, points[0].raw_y, points[0].x, points[0].y,
        keys[0] ? keys : "-");
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
    printf(" purpose: Goodix raw touch test\n");
    printf("========================================================\n\n");

    rg_settings_init(true);
    rg_display_init();
    rg_display_set_backlight(100);

    frame = rg_alloc(rg_display_get_width() * rg_display_get_height() * sizeof(uint16_t), MEM_SLOW);
    RG_ASSERT(frame != NULL, "No touch test frame buffer");

    if (rg_i2c_init())
    {
        if (!touch_probe(RG_GAMEPAD_TOUCH_ADDR))
            touch_probe(RG_GAMEPAD_TOUCH_ALT_ADDR);
    }

    if (!touch_addr)
        RG_LOGE("Goodix not found");

    point_t points[GOODIX_MAX_POINTS] = {0};
    int last_count = -99;
    int last_x = -9999;
    int last_y = -9999;

    for (;;)
    {
        int count = read_points(points);
        draw_frame(points, count > 0 ? count : 0);

        if (count != last_count ||
            (count > 0 && (local_abs(points[0].x - last_x) > 8 || local_abs(points[0].y - last_y) > 8)))
        {
            log_points(points, count);
            last_count = count;
            if (count > 0)
            {
                last_x = points[0].x;
                last_y = points[0].y;
            }
        }

        rg_task_delay(33);
    }
}
