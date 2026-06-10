# S31 Portrait Touch GBA Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a first-pass portrait Retro-Go layout for ESP32-S31-Korvo-1 with a GBA-like touch control area below the game image.

**Architecture:** Keep the RGB panel running at its physical 800x480 timing while exposing a 480x800 logical framebuffer to Retro-Go. Rotate writes in the RGB display backend and map Goodix touch zones against the same logical coordinate space.

**Tech Stack:** ESP-IDF, Retro-Go display/input abstractions, RGB LCD framebuffer backend, Goodix touch input.

---

### Task 1: Portrait Logical Screen

**Files:**
- Modify: `components/retro-go/targets/esp32-s31-korvo-1/config.h`
- Modify: `components/retro-go/drivers/display/rgb_lcd.h`

- [ ] **Step 1: Add target macros**

Set `RG_SCREEN_WIDTH` to `480`, `RG_SCREEN_HEIGHT` to `800`, and add an RGB backend rotation macro for clockwise output to the physical 800x480 panel.

- [ ] **Step 2: Rotate RGB backend writes**

Update `lcd_write()` so logical `(x, y)` maps to physical `(799 - y, x)`. Keep the panel initialization at physical 800x480 by using separate physical width and height constants.

- [ ] **Step 3: Full-frame sync**

Keep syncing the full physical framebuffer with `esp_lcd_panel_draw_bitmap(lcd_panel, 0, 0, 800, 480, lcd_framebuffer)`.

### Task 2: GBA-Like Touch Zones

**Files:**
- Modify: `components/retro-go/targets/esp32-s31-korvo-1/config.h`

- [ ] **Step 1: Reserve top area for gameplay**

Use `RG_SCREEN_VISIBLE_AREA {0, 0, 0, 400}` so emulator content fits in the top half of the logical 480x800 screen.

- [ ] **Step 2: Map lower controls**

Map D-pad on lower left, A/B on lower right, L/R near the shoulder row, and START/SELECT/MENU near the lower center.

### Task 3: Build And Device Check

**Files:**
- Read: `build-full-launcher.log`

- [ ] **Step 1: Build full launcher image**

Run `python rg_tool.py --target esp32-s31-korvo-1 --no-networking build-img`.

- [ ] **Step 2: Flash COM7**

Flash `retro-go_1.46-8-g4ced1-dirty_esp32-s31-korvo-1.img` with esptool on COM7.

- [ ] **Step 3: Capture serial**

Confirm storage, Goodix, RGB LCD initialization, and reported logical screen size.
