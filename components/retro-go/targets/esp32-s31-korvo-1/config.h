// Target definition
#define RG_TARGET_NAME             "ESP32-S31-KORVO-1"

// Storage: S31-Korvo-1 exposes a 4-bit SDIO microSD slot on GPIO20-25.
// ESP32-S31 uses SDMMC slot 0 for these default SDIO pins.
#define RG_STORAGE_ROOT            "/sd"
#define RG_STORAGE_SDMMC_HOST      SDMMC_HOST_SLOT_0
#define RG_STORAGE_SDMMC_SPEED     SDMMC_FREQ_DEFAULT
#define RG_STORAGE_SDMMC_4BIT      1
#define RG_STORAGE_FLASH_PARTITION "vfs"

// Audio: the board codec is ES8389 on I2S + I2C. The legacy retro-go I2S path
// can emit I2S samples after the codec is initialized; full codec control is a
// board-test task because the ES8389 address and analog routing should be
// verified with an I2C scan/schematic.
#define RG_AUDIO_USE_INT_DAC       0
#define RG_AUDIO_USE_EXT_DAC       1
#define RG_AUDIO_USE_ES8389        1
#define RG_AUDIO_SAMPLE_RATE       48000
#define RG_GBA_AUDIO_SAMPLE_RATE   32768

// Video: native panel is an 800x480 RGB LCD (GT1151 touch on the accessory).
// RG_SCREEN_DRIVER=2 is the new compile-safe RGB/LCD framebuffer backend.
#define RG_SCREEN_DRIVER           2
#define RG_SCREEN_BACKLIGHT        1
#define RG_RGB_LCD_PHYS_WIDTH      800
#define RG_RGB_LCD_PHYS_HEIGHT     480
#define RG_RGB_LCD_ROTATE_CW       1
#define RG_GBA_VIEWPORT_HEIGHT     320
#define RG_TOUCH_OVERLAY_GBA       1
#define RG_TOUCH_OVERLAY_TOP       RG_GBA_VIEWPORT_HEIGHT
#define RG_SCREEN_WIDTH            480
#define RG_SCREEN_HEIGHT           800
#define RG_SCREEN_VISIBLE_AREA     {0, 0, 0, 480}
#define RG_SCREEN_SAFE_AREA        {0, 0, 0, 0}
#define RG_SCREEN_PARTIAL_UPDATES  1

// Input: use the GT1151/Goodix touch controller as an on-screen gamepad.
// The physical 800x480 touch coordinates are rotated into the 480x800 logical
// portrait layout before the GBA-like zones below are applied.
#define RG_BATTERY_DRIVER          0
#define RG_GAMEPAD_TOUCH_ADDR      0x5D
#define RG_GAMEPAD_TOUCH_ALT_ADDR  0x14
#define RG_GAMEPAD_TOUCH_MAX_POINTS 5
#define RG_GAMEPAD_TOUCH_PHYS_WIDTH 800
#define RG_GAMEPAD_TOUCH_ROTATE_CW  1
#define RG_GAMEPAD_TOUCH_SWAP_XY    0
#define RG_GAMEPAD_TOUCH_INVERT_X   0
#define RG_GAMEPAD_TOUCH_INVERT_Y   0
#define RG_GAMEPAD_DEBOUNCE_PRESS   1
#define RG_GAMEPAD_DEBOUNCE_RELEASE 1
#define RG_GAMEPAD_TOUCH_HOLD_MS    50
// On-board PLAY/SET/VOL-/VOL+ keys share GPIO42 through an ADC resistor ladder.
// The schematic marks VOL+ at 0.38V, VOL- at 0.82V, and idle near 2V.
#define RG_VOLUME_BUTTON_ADC_UNIT   ADC_UNIT_1
#define RG_VOLUME_BUTTON_ADC_CHANNEL ADC_CHANNEL_0
#define RG_VOLUME_BUTTON_ADC_ATTEN  ADC_ATTEN_DB_0
#define RG_VOLUME_BUTTON_ADC_REPEAT_MS 180
#define RG_VOLUME_BUTTON_ADC_MAP { \
    {  10, 1600, 2300}, \
    { -10, 1100, 1550}, \
}
#define RG_GAMEPAD_TOUCH_MAP { \
    {RG_KEY_L,        0, 210, 320, 440}, \
    {RG_KEY_R,      270, 480, 320, 440}, \
    {RG_KEY_UP,      76, 164, 456, 544}, \
    {RG_KEY_DOWN,    76, 164, 620, 708}, \
    {RG_KEY_LEFT,    24, 112, 544, 632}, \
    {RG_KEY_RIGHT,  128, 216, 544, 632}, \
    {RG_KEY_B,      284, 376, 596, 700}, \
    {RG_KEY_A,      388, 478, 560, 664}, \
    {RG_KEY_Y,      284, 376, 472, 574}, \
    {RG_KEY_X,      388, 478, 430, 532}, \
    {RG_KEY_SELECT, 110, 230, 724, 792}, \
    {RG_KEY_START,  250, 370, 724, 792}, \
    {RG_KEY_MENU,   190, 290, 330, 410}, \
}

// I2C bus: ES8389 codec and GT1151 touch.
#define RG_GPIO_I2C_SDA            GPIO_NUM_0
#define RG_GPIO_I2C_SCL            GPIO_NUM_1

// I2S pins for ES8389 codec.
#define RG_GPIO_SND_I2S_MCLK       GPIO_NUM_2
#define RG_GPIO_SND_I2S_BCK        GPIO_NUM_3
#define RG_GPIO_SND_I2S_WS         GPIO_NUM_4
#define RG_GPIO_SND_I2S_DATA_IN    GPIO_NUM_6
#define RG_GPIO_SND_I2S_DATA       GPIO_NUM_5
#define RG_GPIO_SND_AMP_ENABLE     GPIO_NUM_7

// microSD SDMMC pins. Existing rg_storage.c names these with the SDSPI prefix
// even when RG_STORAGE_SDMMC_HOST is selected.
#define RG_GPIO_SDSPI_D0           GPIO_NUM_20
#define RG_GPIO_SDSPI_D1           GPIO_NUM_21
#define RG_GPIO_SDSPI_D2           GPIO_NUM_22
#define RG_GPIO_SDSPI_D3           GPIO_NUM_23
#define RG_GPIO_SDSPI_CLK          GPIO_NUM_24
#define RG_GPIO_SDSPI_CMD          GPIO_NUM_25

// RGB LCD data pins, in DB0..DB15 order.
#define RG_GPIO_LCD_D0             GPIO_NUM_8
#define RG_GPIO_LCD_D1             GPIO_NUM_9
#define RG_GPIO_LCD_D2             GPIO_NUM_10
#define RG_GPIO_LCD_D3             GPIO_NUM_11
#define RG_GPIO_LCD_D4             GPIO_NUM_12
#define RG_GPIO_LCD_D5             GPIO_NUM_13
#define RG_GPIO_LCD_D6             GPIO_NUM_14
#define RG_GPIO_LCD_D7             GPIO_NUM_15
#define RG_GPIO_LCD_D8             GPIO_NUM_16
#define RG_GPIO_LCD_D9             GPIO_NUM_17
#define RG_GPIO_LCD_D10            GPIO_NUM_18
#define RG_GPIO_LCD_D11            GPIO_NUM_19
#define RG_GPIO_LCD_D12            GPIO_NUM_33
#define RG_GPIO_LCD_D13            GPIO_NUM_34
#define RG_GPIO_LCD_D14            GPIO_NUM_35
#define RG_GPIO_LCD_D15            GPIO_NUM_36
#define RG_GPIO_LCD_CS             GPIO_NUM_38
#define RG_GPIO_LCD_PCLK           GPIO_NUM_40
#define RG_GPIO_LCD_DE             GPIO_NUM_43
#define RG_GPIO_LCD_HSYNC          GPIO_NUM_44
#define RG_GPIO_LCD_VSYNC          GPIO_NUM_45
#define RG_GPIO_LCD_MOSI           GPIO_NUM_60
#define RG_GPIO_LCD_CLK            GPIO_NUM_61
#define RG_GPIO_LCD_BCKL           GPIO_NUM_NC
#define RG_GPIO_LCD_RST            GPIO_NUM_NC

// WS2812 pin according to the S31-Korvo-1 pin table. The component description
// has a conflicting GPIO8 note, but GPIO8 is DB0 in the same table.
#define RG_GPIO_LED                GPIO_NUM_37

// Compile-time acceleration policy. S31 PIE/SIMD is exposed by SOC_CPU_HAS_PIE;
// PPA/RGB LCD are capability-gated because ESP-IDF master still marks some S31
// peripheral support as bring-up/TODO.
#define RG_ESP32S31_ACCEL          1
#define RG_ESP32S31_PIE_PREFERRED  1
#define RG_ESP32S31_PPA_PREFERRED  1
