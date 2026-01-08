# JD9165BA LCD Driver Testing Guide

## Overview

This guide explains how to test and verify the JD9165BA LCD driver in your ESP32-P4 project.

## Test Environment Setup

### Hardware Required

1. **ESP32-P4 Function EV Board**
2. **JD9165BA 1024x600 LCD Panel** with MIPI-DSI connector
3. **USB-C Cable** for power and programming
4. **Connections:**
   ```
   ESP32-P4          JD9165BA LCD
   ========================================
   GPIO 27     →     LCD_RST (Reset)
   GPIO 26     →     LCD_BL  (Backlight)
   MIPI_DSI    →     MIPI Connector
                     (4 data lanes + CLK)
   5V          →     VDD
   GND         →     GND
   ```

### Software Required

- ESP-IDF v5.3 or later
- Python 3.11+
- Build tools (cmake, ninja)

## Integration into lvgl_demo_v9

### Step 1: Verify Component Structure

```bash
cd ESP-IDF/lvgl_demo_v9
ls components/jd9165ba_driver/
```

You should see:
```
├── esp_lcd_jd9165ba.h
├── esp_lcd_jd9165ba.c
├── jd9165ba_test.c
├── CMakeLists.txt
├── idf_component.yml
├── README.md
└── TESTING_GUIDE.md
```

### Step 2: Modify main.c to Use JD9165BA Driver

Edit `main/main.c` to include the test code:

```c
#include "esp_lcd_jd9165ba.h"
#include "esp_lcd_mipi_dsi.h"
#include "bsp/esp-bsp.h"

// Declare test function
extern esp_err_t jd9165ba_run_all_tests(esp_lcd_panel_handle_t panel);
extern void jd9165ba_print_info(void);

void app_main(void)
{
    ESP_LOGI("main", "Starting JD9165BA driver test...");

    // Print driver information
    jd9165ba_print_info();

    // Initialize BSP (this sets up MIPI-DSI bus)
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = 1024 * 100,
        .double_buffer = true,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = false,
        }
    };

    bsp_display_start_with_config(&cfg);

    // Get panel handle (you may need to expose this from BSP)
    // For testing, we can create panel directly

    // Run comprehensive tests
    esp_lcd_panel_handle_t panel = NULL; // Get from BSP
    esp_err_t ret = jd9165ba_run_all_tests(panel);

    if (ret == ESP_OK) {
        ESP_LOGI("main", "All tests passed!");
    } else {
        ESP_LOGE("main", "Tests failed with error: %s", esp_err_to_name(ret));
    }

    // Continue with normal LVGL demo
    bsp_display_lock(0);
    lv_demo_widgets();
    bsp_display_unlock();
}
```

### Step 3: Build and Flash

```bash
# Set target
idf.py set-target esp32p4

# Configure if needed
idf.py menuconfig

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

## Test Suite Description

The driver includes 5 comprehensive tests:

### Test 1: Color Fill Test

**Purpose:** Verify basic display functionality

**What it does:**
- Fills entire screen with solid colors
- Tests: Red, Green, Blue, White, Black, Yellow, Cyan, Magenta
- Each color displayed for 1 second

**Expected Results:**
- ✅ Screen fills completely with each color
- ✅ No dead pixels
- ✅ No color bleeding

**Failure Indicators:**
- ❌ Incomplete fill
- ❌ Wrong colors
- ❌ Screen remains blank

### Test 2: Gradient Pattern Test

**Purpose:** Verify color transition smoothness

**What it does:**
- Draws horizontal red gradient (0→255)
- Draws vertical green gradient (0→255)
- Constant blue level (128)

**Expected Results:**
- ✅ Smooth color transitions
- ✅ No banding
- ✅ Gradient covers full screen

**Failure Indicators:**
- ❌ Color banding
- ❌ Discontinuities
- ❌ Incorrect gradient direction

### Test 3: Checkerboard Pattern Test

**Purpose:** Verify pixel accuracy and alignment

**What it does:**
- Draws checkerboard patterns with different square sizes
- Tests: 64px, 32px, 16px, 8px squares

**Expected Results:**
- ✅ Sharp square edges
- ✅ Perfect alternating pattern
- ✅ No misalignment

**Failure Indicators:**
- ❌ Blurry edges
- ❌ Misaligned squares
- ❌ Pattern irregularities

### Test 4: Display Control Test

**Purpose:** Verify display control commands

**What it does:**
- Tests display ON/OFF
- Tests color inversion

**Expected Results:**
- ✅ Display turns completely off (backlight may stay on)
- ✅ Display turns back on
- ✅ Colors invert correctly

**Failure Indicators:**
- ❌ Display doesn't respond to commands
- ❌ Partial on/off
- ❌ Inversion doesn't work

### Test 5: Partial Update Test

**Purpose:** Verify region-specific updates

**What it does:**
- Draws 200x200 colored boxes at different positions
- Tests: Red, Green, Blue, Yellow, Magenta boxes

**Expected Results:**
- ✅ Boxes appear at correct positions
- ✅ Background remains unchanged
- ✅ No tearing

**Failure Indicators:**
- ❌ Boxes at wrong positions
- ❌ Background corruption
- ❌ Tearing artifacts

## Manual Verification Steps

### Step 1: Power-On Verification

1. Connect hardware
2. Flash firmware
3. Observe boot sequence

**Expected:**
```
I (XXX) lcd.jd9165ba: new jd9165ba panel @0xXXXXXXXX, resolution: 1024x600
I (XXX) lcd.jd9165ba: initializing jd9165ba panel...
I (XXX) lcd.jd9165ba: jd9165ba panel initialized
```

### Step 2: Visual Inspection

1. **Backlight Check:**
   - LCD should illuminate
   - Adjust brightness if adjustable

2. **Color Accuracy:**
   - Pure red should have no green/blue components
   - Pure green should have no red/blue components
   - Pure blue should have no red/green components

3. **Uniformity:**
   - White should be uniform across entire screen
   - Black should be truly black (no light leakage)

### Step 3: Touch Test (if applicable)

1. Run LVGL widgets demo
2. Test touch responsiveness
3. Verify coordinate accuracy

### Step 4: Performance Test

1. Monitor frame rate in serial output
2. Check for tearing during animations
3. Verify smooth scrolling

## Debugging

### Problem: No Display Output

**Checklist:**
- [ ] Is power supply adequate? (Check 5V rail)
- [ ] Is reset pin connected correctly?
- [ ] Is MIPI-DSI connector fully seated?
- [ ] Are data lanes correctly connected?
- [ ] Is ESP32-P4 configured for correct LCD type?

**Debug Steps:**
1. Check serial output for initialization errors
2. Verify GPIO levels with multimeter
3. Check MIPI clock signal with oscilloscope
4. Enable verbose logging:
   ```c
   esp_log_level_set("lcd.jd9165ba", ESP_LOG_DEBUG);
   ```

### Problem: Wrong Colors

**Possible Causes:**
- RGB/BGR order mismatch
- Incorrect color depth configuration
- Endianness issues

**Solution:**
```c
esp_lcd_jd9165ba_config_t config = {
    .flags = {
        .color_space_bgr = 1,  // Toggle this
    }
};
esp_lcd_jd9165ba_set_config(panel, &config);
```

### Problem: Display Artifacts

**Possible Causes:**
- MIPI timing issues
- Signal integrity problems
- Insufficient buffer size

**Solutions:**
1. Adjust MIPI clock frequency
2. Check cable quality
3. Increase buffer size
4. Enable double buffering

### Problem: Slow Performance

**Optimizations:**
1. Use DMA buffers
2. Increase buffer size
3. Enable frame buffer in SPIRAM
4. Optimize pixel format

## Expected Output (Serial Monitor)

```
I (1234) main: Starting JD9165BA driver test...
I (1235) jd9165ba_test: ========================================
I (1236) jd9165ba_test: JD9165BA LCD Driver Information:
I (1237) jd9165ba_test:   - Resolution: 1024x600
I (1238) jd9165ba_test:   - Interface: MIPI-DSI 4-lane
I (1239) jd9165ba_test:   - Color Format: RGB888
I (1240) jd9165ba_test:   - Reset GPIO: 27
I (1241) jd9165ba_test:   - Backlight GPIO: 26
I (1242) jd9165ba_test: ========================================
I (1243) lcd.jd9165ba: new jd9165ba panel @0x3fca1234, resolution: 1024x600
I (1244) lcd.jd9165ba: initializing jd9165ba panel...
I (1489) lcd.jd9165ba: jd9165ba panel initialized
I (1490) jd9165ba_test: ========================================
I (1491) jd9165ba_test:   JD9165BA LCD Driver Test Suite
I (1492) jd9165ba_test:   Resolution: 1024x600
I (1493) jd9165ba_test: ========================================
I (1494) jd9165ba_test: === Test 1: Color Fill Test ===
I (1495) jd9165ba_test: Filling screen with Red...
I (2496) jd9165ba_test: Filling screen with Green...
I (3497) jd9165ba_test: Filling screen with Blue...
...
I (15000) jd9165ba_test: === Test 5: Partial Update Test ===
I (15001) jd9165ba_test: Drawing box 1 at (100, 100)...
...
I (20000) jd9165ba_test: ========================================
I (20001) jd9165ba_test:   ALL TESTS PASSED!
I (20002) jd9165ba_test: ========================================
I (20003) main: All tests passed!
```

## Performance Benchmarks

### Expected Frame Rates

| Configuration | FPS | Notes |
|--------------|-----|-------|
| Single buffer | ~30 | Basic config |
| Double buffer | ~60 | Recommended |
| Triple buffer | ~60 | Minimal improvement |

### Memory Usage

| Item | Size | Location |
|------|------|----------|
| Frame buffer (single) | ~1.8MB | SPIRAM |
| Frame buffer (double) | ~3.6MB | SPIRAM |
| Driver code | ~20KB | Flash |
| Test code | ~15KB | Flash |

## Next Steps

After successful testing:

1. **Integrate with LVGL:**
   - Use `bsp_display_start_with_config()`
   - Run LVGL demos
   - Test touch input

2. **Optimize Performance:**
   - Tune buffer sizes
   - Enable hardware acceleration
   - Profile critical paths

3. **MicroPython Integration:**
   - Create MicroPython bindings
   - Test with MicroPython LVGL
   - Document API

## Support

For issues or questions:
- Check ESP-IDF documentation
- Review LVGL documentation
- Check JD9165BA datasheet
- Post in ESP32 forums

## References

- [ESP-IDF LCD Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/lcd.html)
- [LVGL Documentation](https://docs.lvgl.io/)
- [ESP32-P4 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-p4_datasheet_en.pdf)
