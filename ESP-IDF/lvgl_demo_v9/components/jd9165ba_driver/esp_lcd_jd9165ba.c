/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <sys/cdefs.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_jd9165ba.h"

static const char *TAG = "lcd.jd9165ba";

// JD9165BA specific commands
#define JD9165BA_CMD_PAGE_SELECT    0x30

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    uint16_t width;
    uint16_t height;
    uint8_t madctl_val; // Current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // Current value of LCD_CMD_COLMOD register
} jd9165ba_panel_t;

// JD9165BA initialization sequence for 1024x600 MIPI-DSI 4-lane
static const uint8_t vendor_specific_init[] = {
    // Page 0
    0x02, JD9165BA_CMD_PAGE_SELECT, 0x00,
    0x05, 0xF7, 0x49, 0x61, 0x02, 0x00,

    // Page 1 - MIPI Configuration
    0x02, JD9165BA_CMD_PAGE_SELECT, 0x01,
    0x02, 0x04, 0x0C,
    0x02, 0x05, 0x08,
    0x02, 0x20, 0x04,  // r_lansel_sel_reg
    0x02, 0x0B, 0x13,  // 4-lane configuration
    0x02, 0x1F, 0x05,  // mipi_hs_settle
    0x02, 0x23, 0x38,
    0x02, 0x28, 0x18,
    0x02, 0x29, 0x29,
    0x02, 0x2A, 0x01,
    0x02, 0x2B, 0x29,
    0x02, 0x2C, 0x01,

    // Page 2 - Display Configuration
    0x02, JD9165BA_CMD_PAGE_SELECT, 0x02,
    0x02, 0x00, 0x05,
    0x02, 0x01, 0x22,
    0x02, 0x02, 0x08,
    0x02, 0x03, 0x12,
    0x02, 0x04, 0x16,
    0x02, 0x05, 0x64,
    0x02, 0x06, 0x00,
    0x02, 0x07, 0x00,
    0x02, 0x08, 0x78,
    0x02, 0x09, 0x00,
    0x02, 0x0A, 0x04,

    // Page 6 - Gamma Settings
    0x02, JD9165BA_CMD_PAGE_SELECT, 0x06,
    0x0F, 0x12, 0x3F, 0x26, 0x27, 0x35, 0x2D, 0x34, 0x3F, 0x3F, 0x3F, 0x35, 0x2A, 0x20, 0x16, 0x08,
    0x0F, 0x13, 0x3F, 0x26, 0x28, 0x35, 0x27, 0x29, 0x29, 0x2F, 0x35, 0x2F, 0x26, 0x20, 0x16, 0x08,

    // Page 0x0A
    0x02, JD9165BA_CMD_PAGE_SELECT, 0x0A,
    0x02, 0x02, 0x4F,
    0x02, 0x0B, 0x40,

    // Page 0x0D - MIPI Power
    0x02, JD9165BA_CMD_PAGE_SELECT, 0x0D,
    0x02, 0x0D, 0x04,
    0x02, 0x10, 0x0C,
    0x02, 0x11, 0x0C,
    0x02, 0x12, 0x0C,
    0x02, 0x13, 0x0C,

    // Page 7
    0x02, JD9165BA_CMD_PAGE_SELECT, 0x07,
    0x02, 0x0D, 0x01,

    // Back to Page 0
    0x02, JD9165BA_CMD_PAGE_SELECT, 0x00,

    // Sleep Out
    0x01, LCD_CMD_SLPOUT,
    0xFF, 120,  // Delay 120ms

    // Display On
    0x01, LCD_CMD_DISPON,
    0xFF, 20,   // Delay 20ms

    0x00  // End of sequence
};

static esp_err_t panel_jd9165ba_del(esp_lcd_panel_t *panel);
static esp_err_t panel_jd9165ba_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_jd9165ba_init(esp_lcd_panel_t *panel);
static esp_err_t panel_jd9165ba_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_jd9165ba_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_jd9165ba_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_jd9165ba_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_jd9165ba_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_jd9165ba_disp_on_off(esp_lcd_panel_t *panel, bool on_off);

esp_err_t esp_lcd_new_panel_jd9165ba(const esp_lcd_panel_io_handle_t io,
                                       const esp_lcd_panel_dev_config_t *panel_dev_config,
                                       esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");

    jd9165ba_panel_t *jd9165ba = calloc(1, sizeof(jd9165ba_panel_t));
    ESP_RETURN_ON_FALSE(jd9165ba, ESP_ERR_NO_MEM, TAG, "no mem for jd9165ba panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    jd9165ba->io = io;
    jd9165ba->reset_gpio_num = panel_dev_config->reset_gpio_num;
    jd9165ba->reset_level = panel_dev_config->flags.reset_active_high;
    jd9165ba->width = panel_dev_config->width ? panel_dev_config->width : 1024;
    jd9165ba->height = panel_dev_config->height ? panel_dev_config->height : 600;
    jd9165ba->madctl_val = 0;
    jd9165ba->colmod_val = 0x77; // RGB888

    jd9165ba->base.del = panel_jd9165ba_del;
    jd9165ba->base.reset = panel_jd9165ba_reset;
    jd9165ba->base.init = panel_jd9165ba_init;
    jd9165ba->base.draw_bitmap = panel_jd9165ba_draw_bitmap;
    jd9165ba->base.invert_color = panel_jd9165ba_invert_color;
    jd9165ba->base.set_gap = panel_jd9165ba_set_gap;
    jd9165ba->base.mirror = panel_jd9165ba_mirror;
    jd9165ba->base.swap_xy = panel_jd9165ba_swap_xy;
    jd9165ba->base.disp_on_off = panel_jd9165ba_disp_on_off;

    *ret_panel = &(jd9165ba->base);
    ESP_LOGI(TAG, "new jd9165ba panel @%p, resolution: %dx%d", jd9165ba, jd9165ba->width, jd9165ba->height);

    return ESP_OK;

err:
    if (jd9165ba) {
        free(jd9165ba);
    }
    return ESP_FAIL;
}

static esp_err_t panel_jd9165ba_del(esp_lcd_panel_t *panel)
{
    jd9165ba_panel_t *jd9165ba = __containerof(panel, jd9165ba_panel_t, base);

    if (jd9165ba->reset_gpio_num >= 0) {
        gpio_reset_pin(jd9165ba->reset_gpio_num);
    }

    ESP_LOGI(TAG, "del jd9165ba panel @%p", jd9165ba);
    free(jd9165ba);
    return ESP_OK;
}

static esp_err_t panel_jd9165ba_reset(esp_lcd_panel_t *panel)
{
    jd9165ba_panel_t *jd9165ba = __containerof(panel, jd9165ba_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9165ba->io;

    // Perform hardware reset
    if (jd9165ba->reset_gpio_num >= 0) {
        gpio_set_level(jd9165ba->reset_gpio_num, jd9165ba->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(jd9165ba->reset_gpio_num, !jd9165ba->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
    } else {
        // Software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    return ESP_OK;
}

static esp_err_t panel_jd9165ba_init(esp_lcd_panel_t *panel)
{
    jd9165ba_panel_t *jd9165ba = __containerof(panel, jd9165ba_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9165ba->io;

    ESP_LOGI(TAG, "initializing jd9165ba panel...");

    // Send initialization sequence
    const uint8_t *cmd_ptr = vendor_specific_init;
    while (*cmd_ptr != 0x00) {
        uint8_t len = *cmd_ptr++;

        if (len == 0xFF) {
            // Delay command
            uint8_t delay_ms = *cmd_ptr++;
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        } else if (len == 0x01) {
            // Single byte command (no parameters)
            uint8_t cmd = *cmd_ptr++;
            ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, cmd, NULL, 0), TAG, "send command failed");
        } else {
            // Command with parameters
            uint8_t cmd = *cmd_ptr++;
            ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, cmd, cmd_ptr, len - 1), TAG, "send command failed");
            cmd_ptr += (len - 1);
        }
    }

    ESP_LOGI(TAG, "jd9165ba panel initialized");
    return ESP_OK;
}

static esp_err_t panel_jd9165ba_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    jd9165ba_panel_t *jd9165ba = __containerof(panel, jd9165ba_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9165ba->io;

    ESP_RETURN_ON_FALSE(x_start < x_end, ESP_ERR_INVALID_ARG, TAG, "start x must be < end x");
    ESP_RETURN_ON_FALSE(y_start < y_end, ESP_ERR_INVALID_ARG, TAG, "start y must be < end y");

    // Column address set
    uint8_t col_data[] = {
        (x_start >> 8) & 0xFF, x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF, (x_end - 1) & 0xFF
    };
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_CASET, col_data, sizeof(col_data)), TAG, "send command failed");

    // Row address set
    uint8_t row_data[] = {
        (y_start >> 8) & 0xFF, y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF, (y_end - 1) & 0xFF
    };
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_RASET, row_data, sizeof(row_data)), TAG, "send command failed");

    // Write pixel data
    size_t len = (x_end - x_start) * (y_end - y_start) * 3; // RGB888
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len), TAG, "send color failed");

    return ESP_OK;
}

static esp_err_t panel_jd9165ba_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    jd9165ba_panel_t *jd9165ba = __containerof(panel, jd9165ba_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9165ba->io;
    uint8_t command = invert_color_data ? LCD_CMD_INVON : LCD_CMD_INVOFF;

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_jd9165ba_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    jd9165ba_panel_t *jd9165ba = __containerof(panel, jd9165ba_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9165ba->io;

    if (mirror_x) {
        jd9165ba->madctl_val |= LCD_CMD_MX_BIT;
    } else {
        jd9165ba->madctl_val &= ~LCD_CMD_MX_BIT;
    }

    if (mirror_y) {
        jd9165ba->madctl_val |= LCD_CMD_MY_BIT;
    } else {
        jd9165ba->madctl_val &= ~LCD_CMD_MY_BIT;
    }

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, &jd9165ba->madctl_val, 1), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_jd9165ba_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    jd9165ba_panel_t *jd9165ba = __containerof(panel, jd9165ba_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9165ba->io;

    if (swap_axes) {
        jd9165ba->madctl_val |= LCD_CMD_MV_BIT;
    } else {
        jd9165ba->madctl_val &= ~LCD_CMD_MV_BIT;
    }

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, &jd9165ba->madctl_val, 1), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_jd9165ba_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    // JD9165BA doesn't support gap setting
    return ESP_OK;
}

static esp_err_t panel_jd9165ba_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    jd9165ba_panel_t *jd9165ba = __containerof(panel, jd9165ba_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9165ba->io;
    uint8_t command = on_off ? LCD_CMD_DISPON : LCD_CMD_DISPOFF;

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}

esp_err_t esp_lcd_jd9165ba_set_config(esp_lcd_panel_handle_t panel,
                                        const esp_lcd_jd9165ba_config_t *jd9165ba_config)
{
    ESP_RETURN_ON_FALSE(panel && jd9165ba_config, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    jd9165ba_panel_t *jd9165ba = __containerof(panel, jd9165ba_panel_t, base);

    // Apply color space configuration
    if (jd9165ba_config->flags.color_space_bgr) {
        jd9165ba->madctl_val |= LCD_CMD_BGR_BIT;
    } else {
        jd9165ba->madctl_val &= ~LCD_CMD_BGR_BIT;
    }

    // Apply mirroring
    if (jd9165ba_config->flags.mirror_horizontal) {
        jd9165ba->madctl_val |= LCD_CMD_MX_BIT;
    }
    if (jd9165ba_config->flags.mirror_vertical) {
        jd9165ba->madctl_val |= LCD_CMD_MY_BIT;
    }

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(jd9165ba->io, LCD_CMD_MADCTL, &jd9165ba->madctl_val, 1),
                        TAG, "send command failed");

    return ESP_OK;
}
