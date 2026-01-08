# JD9165BA Driver Integration Example

## Quick Start - Standalone Test

### Option 1: Add to Existing main.c

Add this to your `main/main.c`:

```c
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "bsp/esp-bsp.h"

// Include JD9165BA driver test functions
extern esp_err_t jd9165ba_run_all_tests(esp_lcd_panel_handle_t panel);
extern void jd9165ba_print_info(void);

void app_main(void)
{
    // Print driver info
    jd9165ba_print_info();

    // Initialize display using BSP
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = 1024 * 100,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = false,
        }
    };

    esp_lcd_panel_handle_t panel_handle = NULL;
    bsp_display_start_with_config(&cfg);

    // Note: You'll need to get panel handle from BSP
    // For now, skip tests if panel handle not available

    if (panel_handle) {
        // Run all tests
        ESP_ERROR_CHECK(jd9165ba_run_all_tests(panel_handle));
    }

    // Start LVGL demo
    bsp_display_lock(0);
    lv_demo_widgets();
    bsp_display_unlock();
}
```

### Option 2: Create Dedicated Test Main

Create `main/main_test.c`:

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "driver/gpio.h"
#include "esp_lcd_jd9165ba.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "jd9165ba_main";

// Test functions
extern esp_err_t jd9165ba_run_all_tests(esp_lcd_panel_handle_t panel);
extern void jd9165ba_print_info(void);

// Backlight control
static void init_backlight(void)
{
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << BSP_LCD_BACKLIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(BSP_LCD_BACKLIGHT, 1);
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "JD9165BA LCD Driver Standalone Test");
    ESP_LOGI(TAG, "========================================");

    // Initialize backlight
    init_backlight();

    // Print driver information
    jd9165ba_print_info();

    // Create MIPI DSI bus
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = 4,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = 1000,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    ESP_LOGI(TAG, "MIPI DSI bus created");

    // Create panel IO
    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io_handle));

    ESP_LOGI(TAG, "Panel IO created");

    // Create JD9165BA panel
    esp_lcd_panel_handle_t panel_handle;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 24,
        .flags = {
            .reset_active_high = 0,
        },
        .vendor_config = NULL,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_jd9165ba(io_handle, &panel_config, &panel_handle));
    ESP_LOGI(TAG, "JD9165BA panel created");

    // Reset and initialize
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Panel initialized and turned on");

    // Run comprehensive tests
    ESP_ERROR_CHECK(jd9165ba_run_all_tests(panel_handle));

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Test complete. Panel ready for use.");
    ESP_LOGI(TAG, "========================================");

    // Keep running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## Integration with LVGL

### Step 1: Register Custom Driver

In `components/bsp_extra/src/bsp_board_extra.c`, add:

```c
#include "esp_lcd_jd9165ba.h"

esp_err_t bsp_display_new_with_custom_driver(...)
{
    // ... existing MIPI-DSI bus creation code ...

    // Instead of using default driver, use JD9165BA
    ESP_ERROR_CHECK(esp_lcd_new_panel_jd9165ba(io_handle, &panel_config, &panel_handle));

    // ... rest of initialization ...
}
```

### Step 2: Use with LVGL Port

```c
#include "esp_lvgl_port.h"
#include "esp_lcd_jd9165ba.h"

void setup_lvgl_with_jd9165ba(void)
{
    // Create panel (as shown above)
    esp_lcd_panel_handle_t panel_handle;
    // ... panel creation code ...

    // Configure LVGL port
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = 1024 * 600 * sizeof(lv_color_t) / 10,
        .double_buffer = true,
        .hres = 1024,
        .vres = 600,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram = true,
        }
    };

    lvgl_port_display_handle_t disp = lvgl_port_add_disp(&disp_cfg);

    // Run LVGL demo
    lv_demo_widgets();
}
```

## CMakeLists.txt Integration

Add to your main component's `CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_lcd
        jd9165ba_driver  # Add this line
        lvgl
        esp_lvgl_port
)
```

## sdkconfig Configuration

Add to `sdkconfig.defaults`:

```ini
# JD9165BA LCD Configuration
CONFIG_BSP_LCD_TYPE_1024_600=y
CONFIG_BSP_LCD_COLOR_FORMAT_RGB888=y

# LVGL Configuration
CONFIG_LV_COLOR_DEPTH_24=y
CONFIG_LV_COLOR_16_SWAP=n

# SPIRAM Configuration
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_200M=y
```

## Build Commands

```bash
# Clean previous build
idf.py fullclean

# Set target
idf.py set-target esp32p4

# Configure
idf.py menuconfig
# Navigate to: Component config → Board Support Package
# Select: LCD Type → 1024x600

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

## Verify Installation

After flashing, you should see:

```
I (XXX) jd9165ba_main: ========================================
I (XXX) jd9165ba_main: JD9165BA LCD Driver Standalone Test
I (XXX) jd9165ba_main: ========================================
I (XXX) jd9165ba_test: JD9165BA LCD Driver Information:
I (XXX) jd9165ba_test:   - Resolution: 1024x600
I (XXX) jd9165ba_test:   - Interface: MIPI-DSI 4-lane
I (XXX) jd9165ba_test:   - Color Format: RGB888
I (XXX) lcd.jd9165ba: new jd9165ba panel @0xXXXXXXXX
I (XXX) lcd.jd9165ba: initializing jd9165ba panel...
I (XXX) lcd.jd9165ba: jd9165ba panel initialized
I (XXX) jd9165ba_test: === Test 1: Color Fill Test ===
...
I (XXX) jd9165ba_test:   ALL TESTS PASSED!
```

## Troubleshooting

### Build Errors

**Error:** `fatal error: esp_lcd_jd9165ba.h: No such file or directory`

**Solution:** Add component to REQUIRES in CMakeLists.txt

**Error:** `undefined reference to esp_lcd_new_panel_jd9165ba`

**Solution:** Make sure jd9165ba_driver component is in components/ directory

### Runtime Errors

**Error:** `esp_lcd_panel_reset failed: ESP_FAIL`

**Solution:** Check GPIO connections and pin numbers

**Error:** `No display output`

**Solution:**
1. Verify power supply
2. Check MIPI connections
3. Enable debug logging
4. Test with simple color fill first

## Next Steps

1. Test with LVGL demos
2. Integrate touch input
3. Optimize performance
4. Add MicroPython bindings
5. Create custom UI applications

## Example Project Structure

```
ESP-IDF/lvgl_demo_v9/
├── components/
│   ├── jd9165ba_driver/          ← Custom driver
│   │   ├── esp_lcd_jd9165ba.h
│   │   ├── esp_lcd_jd9165ba.c
│   │   ├── jd9165ba_test.c
│   │   ├── CMakeLists.txt
│   │   └── idf_component.yml
│   ├── bsp_extra/
│   └── espressif__esp32_p4_function_ev_board/
├── main/
│   ├── main.c                     ← Your application
│   └── CMakeLists.txt
├── CMakeLists.txt
└── sdkconfig.defaults
```

## Performance Tips

1. **Use Double Buffering:**
   ```c
   .double_buffer = true,
   ```

2. **Optimize Buffer Size:**
   ```c
   .buffer_size = 1024 * 100,  // 100 lines
   ```

3. **Enable DMA:**
   ```c
   .buff_dma = true,
   ```

4. **Use SPIRAM:**
   ```c
   .buff_spiram = true,
   ```

## Support

- Driver issues: Check logs and verify hardware connections
- LVGL issues: Refer to LVGL documentation
- ESP-IDF issues: Check ESP-IDF documentation
