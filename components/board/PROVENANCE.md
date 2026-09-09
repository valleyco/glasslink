# Board HAL provenance

`include/hal_display.h` is the **shared** host/device contract for this repo.

`src/cyd_display_spi.c` is adapted from:

- `../esp32-invaders/components/board/src/cyd_display.c` (display path only)

Copied/adapted patterns:

- CYD pins: SCLK 14, MOSI 13, CS 15, DC 2, RST -1, BL 21
- ST7789 via `esp_lcd` (not ILI9341), landscape `swap_xy`, mirrors off
- RGB565 byteswap on TX
- Dual strip buffers + SPI color-done semaphore

**Not** copied: touch, 1bpp game present, screen-dirty/convert, machine deps.

Date: 2026-09-09 (Step 4).
