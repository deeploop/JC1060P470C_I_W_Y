# JD9165BA MIPI-DSI LCD Driver for ESP32-P4

## Overview

This is a custom LCD driver for the JD9165BA MIPI-DSI display controller (1024x600 resolution), designed for ESP32-P4 with LVGL and MicroPython support.

## Features

- **Resolution:** 1024x600
- **Interface:** MIPI-DSI (4-lane)
- **Color Format:** RGB888
- **Compatible with:**
  - ESP-IDF v5.3+
  - LVGL v9.x
  - MicroPython (future support)

## Hardware Requirements

- **ESP32-P4 Function EV Board**
- **JD9165BA 1024x600 LCD Panel** with MIPI-DSI interface
- **Connections:**
  - GPIO 27: LCD_RST (Reset pin)
  - GPIO 26: LCD_BL (Backlight control via PWM)
  - MIPI-DSI: 4 data lanes + CLK

## Usage

### In ESP-IDF Project

```c
#include "esp_lcd_jd9165ba.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"

// Create MIPI-DSI bus
esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
esp_lcd_dsi_bus_config_t bus_config = {
    // ...configure bus...
};
esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus);

// Create panel IO
esp_lcd_panel_io_handle_t io_handle;
esp_lcd_dbi_io_config_t io_config = {
    // ...configure IO...
};
esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &io_config, &io_handle);

// Create JD9165BA panel
esp_lcd_panel_handle_t panel_handle;
esp_lcd_panel_dev_config_t panel_config = {
    .reset_gpio_num = 27,
    .color_space = ESP_LCD_COLOR_SPACE_RGB,
    .bits_per_pixel = 24,
    .width = 1024,
    .height = 600,
};
esp_lcd_new_panel_jd9165ba(io_handle, &panel_config, &panel_handle);

// Reset and initialize
esp_lcd_panel_reset(panel_handle);
esp_lcd_panel_init(panel_handle);
esp_lcd_panel_disp_on_off(panel_handle, true);
```

### With LVGL

```c
#include "esp_lvgl_port.h"

lvgl_port_display_cfg_t disp_cfg = {
    .io_handle = io_handle,
    .panel_handle = panel_handle,
    .buffer_size = 1024 * 100,
    .double_buffer = true,
    // ...other config...
};
lvgl_port_add_disp(&disp_cfg);
```

## Testing and Verification

### 1. Basic Driver Test

Use the test example in `main/jd9165ba_test.c`:

```bash
cd ESP-IDF/lvgl_demo_v9
idf.py set-target esp32p4
idf.py build flash monitor
```

Expected output:
```
I (XXX) lcd.jd9165ba: new jd9165ba panel @0xXXXXXXXX, resolution: 1024x600
I (XXX) lcd.jd9165ba: initializing jd9165ba panel...
I (XXX) lcd.jd9165ba: jd9165ba panel initialized
I (XXX) jd9165ba_test: Display initialized successfully!
```

### 2. Verification Steps

1. **Power On Test:**
   - LCD should turn on after initialization
   - Backlight should be controllable

2. **Color Fill Test:**
   - Fill screen with solid colors (Red, Green, Blue, White, Black)
   - Verify color accuracy

3. **LVGL Integration Test:**
   - Run LVGL widgets demo
   - Check touch response (if touch panel connected)
   - Verify smooth animations

4. **Performance Test:**
   - Measure frame rate
   - Check for tearing or artifacts
   - Test with different buffer sizes

## Configuration

### sdkconfig Options

Add these to your `sdkconfig.defaults`:

```ini
CONFIG_BSP_LCD_TYPE_1024_600=y
CONFIG_BSP_LCD_COLOR_FORMAT_RGB888=y
CONFIG_LV_COLOR_DEPTH_24=y
```

## Initialization Sequence

The driver sends the following initialization sequence:
1. Hardware reset (GPIO toggle)
2. Page selection commands
3. MIPI-DSI lane configuration (4-lane)
4. Display timing parameters
5. Gamma correction settings
6. Power settings
7. Sleep out + Display on

## Troubleshooting

### No Display

- Check power supply (IOVCC, AVDD, VGH, VGL)
- Verify reset pin connection
- Check MIPI-DSI lane connections

### Wrong Colors

- Verify color format configuration (RGB vs BGR)
- Check `color_space_bgr` flag in config

### Display Artifacts

- Adjust MIPI timing parameters
- Check clock frequency (default: 312MHz)
- Verify signal integrity

## API Reference

### Main Functions

- `esp_lcd_new_panel_jd9165ba()` - Create panel instance
- `esp_lcd_jd9165ba_set_config()` - Apply vendor-specific configuration
- `esp_lcd_panel_reset()` - Reset the panel
- `esp_lcd_panel_init()` - Initialize the panel
- `esp_lcd_panel_draw_bitmap()` - Draw bitmap data
- `esp_lcd_panel_disp_on_off()` - Control display on/off

## License

Apache License 2.0

## Author

Created for ESP32-P4 + JD9165BA LCD panel integration
