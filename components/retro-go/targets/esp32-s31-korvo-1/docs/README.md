# ESP32-S31-Korvo-1 target

Experimental ESP32-S31-Korvo-1 bring-up target.

Hardware assumptions from Espressif public docs:

- ESP32-S31-WROOM-3 module
- 16 MB SPI flash + 16 MB PSRAM
- 800x480 RGB LCD accessory, GT1151 touch
- ES8389 codec over I2S + I2C
- microSD via SDMMC pins GPIO20..25
- four function buttons through ADC_BUTTON on GPIO42; thresholds need board calibration

## Default apps and cores

`env.py` sets the default app list to:

```bash
launcher
```

`retro-core` currently builds these emulation cores on this target:

- GB / GBC via `gnuboy`
- NES via `nofrendo`
- PC Engine / TurboGrafx-16 via `pce-go`
- Sega Master System / Game Gear / ColecoVision via `smsplus`
- Game & Watch via `gw-emulator`
- Atari Lynx via `handy`
- SNES via `snes9x`

Other upstream apps remain opt-in for now:

- `gwenesis` for Mega Drive / Genesis
- `fmsx` for MSX
- `prboom-go` for Doom

Build the default set:

```powershell
$env:IDF_TOOLS_PATH='D:\espressif\tools'
powershell -ExecutionPolicy Bypass -NoProfile -Command "& 'D:\espressif\master\esp-idf\export.ps1'; cd 'D:\Workspace\retro-go-s31'; python rg_tool.py --target esp32-s31-korvo-1 --no-networking build-img"
```

Build only one app during bring-up:

```powershell
python rg_tool.py --target esp32-s31-korvo-1 --no-networking build launcher
python rg_tool.py --target esp32-s31-korvo-1 --no-networking build screen-test
```

## Flashing

Set `PORT` to the board serial port first:

```powershell
$env:PORT='COM7'
```

For first flash, build and flash a full image. Add a FAT partition if you want internal flash storage; SD card storage is still the preferred path for ROMs.

```powershell
python rg_tool.py --target esp32-s31-korvo-1 --port $env:PORT --no-networking install
```

For faster iteration after the first full image flash, flash one app partition:

```powershell
python rg_tool.py --target esp32-s31-korvo-1 --port $env:PORT flash launcher
```

Monitor logs:

```powershell
python rg_tool.py --target esp32-s31-korvo-1 --port $env:PORT monitor launcher
```

## Board-test checklist

Board testing still required for:

- RGB timing and color order
- RGB frame pacing: current test firmware is about 6 FPS on camera preview, with visible tearing. Investigate VSYNC callbacks, real page flip, and avoiding full-frame CPU copies through `esp_lcd_panel_draw_bitmap()`.
- RGB update path: keep `screen-test` as the small repro app while tuning DMA/framebuffer behavior.
- GT1151 coordinate orientation and on-screen button zones
- ES8389 address/init and I2S routing
- ADC function-key thresholds
- SDMMC 4-bit wiring and mount behavior
