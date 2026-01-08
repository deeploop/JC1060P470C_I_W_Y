/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file jd9165ba_test.c
 * @brief JD9165BA LCD Driver Test and Verification
 *
 * This file contains test functions to verify the JD9165BA LCD driver functionality.
 * It can be integrated into your main application for testing.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "driver/gpio.h"
#include "esp_lcd_jd9165ba.h"

static const char *TAG = "jd9165ba_test";

#define LCD_WIDTH   1024
#define LCD_HEIGHT  600
#define LCD_RST_GPIO   27
#define LCD_BL_GPIO    26

// Test pattern buffers
static uint8_t *test_buffer = NULL;

/**
 * @brief Generate color fill pattern
 */
static void fill_color_pattern(uint8_t *buffer, uint32_t width, uint32_t height, uint8_t r, uint8_t g, uint8_t b)
{
    for (uint32_t i = 0; i < width * height; i++) {
        buffer[i * 3 + 0] = r;  // Red
        buffer[i * 3 + 1] = g;  // Green
        buffer[i * 3 + 2] = b;  // Blue
    }
}

/**
 * @brief Generate gradient pattern
 */
static void fill_gradient_pattern(uint8_t *buffer, uint32_t width, uint32_t height)
{
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t idx = (y * width + x) * 3;
            buffer[idx + 0] = (x * 255) / width;   // Red gradient
            buffer[idx + 1] = (y * 255) / height;  // Green gradient
            buffer[idx + 2] = 128;                  // Blue constant
        }
    }
}

/**
 * @brief Generate checkerboard pattern
 */
static void fill_checkerboard_pattern(uint8_t *buffer, uint32_t width, uint32_t height, uint32_t square_size)
{
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t idx = (y * width + x) * 3;
            bool is_white = ((x / square_size) + (y / square_size)) % 2 == 0;
            uint8_t color = is_white ? 255 : 0;
            buffer[idx + 0] = color;
            buffer[idx + 1] = color;
            buffer[idx + 2] = color;
        }
    }
}

/**
 * @brief Test 1: Color Fill Test
 * Tests basic display functionality with solid colors
 */
esp_err_t jd9165ba_test_color_fill(esp_lcd_panel_handle_t panel)
{
    ESP_LOGI(TAG, "=== Test 1: Color Fill Test ===");

    if (!test_buffer) {
        test_buffer = malloc(LCD_WIDTH * LCD_HEIGHT * 3);
        if (!test_buffer) {
            ESP_LOGE(TAG, "Failed to allocate test buffer");
            return ESP_ERR_NO_MEM;
        }
    }

    const struct {
        const char *name;
        uint8_t r, g, b;
    } colors[] = {
        {"Red",     255, 0,   0  },
        {"Green",   0,   255, 0  },
        {"Blue",    0,   0,   255},
        {"White",   255, 255, 255},
        {"Black",   0,   0,   0  },
        {"Yellow",  255, 255, 0  },
        {"Cyan",    0,   255, 255},
        {"Magenta", 255, 0,   255},
    };

    for (int i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
        ESP_LOGI(TAG, "Filling screen with %s...", colors[i].name);
        fill_color_pattern(test_buffer, LCD_WIDTH, LCD_HEIGHT, colors[i].r, colors[i].g, colors[i].b);

        esp_err_t ret = esp_lcd_panel_draw_bitmap(panel, 0, 0, LCD_WIDTH, LCD_HEIGHT, test_buffer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to draw bitmap: %s", esp_err_to_name(ret));
            return ret;
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // Display for 1 second
    }

    ESP_LOGI(TAG, "Color fill test completed successfully!");
    return ESP_OK;
}

/**
 * @brief Test 2: Gradient Pattern Test
 * Tests color transition and smoothness
 */
esp_err_t jd9165ba_test_gradient(esp_lcd_panel_handle_t panel)
{
    ESP_LOGI(TAG, "=== Test 2: Gradient Pattern Test ===");

    if (!test_buffer) {
        test_buffer = malloc(LCD_WIDTH * LCD_HEIGHT * 3);
        if (!test_buffer) {
            ESP_LOGE(TAG, "Failed to allocate test buffer");
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_LOGI(TAG, "Drawing gradient pattern...");
    fill_gradient_pattern(test_buffer, LCD_WIDTH, LCD_HEIGHT);

    esp_err_t ret = esp_lcd_panel_draw_bitmap(panel, 0, 0, LCD_WIDTH, LCD_HEIGHT, test_buffer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to draw gradient: %s", esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "Gradient test completed successfully!");
    return ESP_OK;
}

/**
 * @brief Test 3: Checkerboard Pattern Test
 * Tests pixel accuracy and alignment
 */
esp_err_t jd9165ba_test_checkerboard(esp_lcd_panel_handle_t panel)
{
    ESP_LOGI(TAG, "=== Test 3: Checkerboard Pattern Test ===");

    if (!test_buffer) {
        test_buffer = malloc(LCD_WIDTH * LCD_HEIGHT * 3);
        if (!test_buffer) {
            ESP_LOGE(TAG, "Failed to allocate test buffer");
            return ESP_ERR_NO_MEM;
        }
    }

    uint32_t square_sizes[] = {64, 32, 16, 8};

    for (int i = 0; i < sizeof(square_sizes) / sizeof(square_sizes[0]); i++) {
        ESP_LOGI(TAG, "Drawing checkerboard with %lu pixel squares...", square_sizes[i]);
        fill_checkerboard_pattern(test_buffer, LCD_WIDTH, LCD_HEIGHT, square_sizes[i]);

        esp_err_t ret = esp_lcd_panel_draw_bitmap(panel, 0, 0, LCD_WIDTH, LCD_HEIGHT, test_buffer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to draw checkerboard: %s", esp_err_to_name(ret));
            return ret;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    ESP_LOGI(TAG, "Checkerboard test completed successfully!");
    return ESP_OK;
}

/**
 * @brief Test 4: Display Control Test
 * Tests display on/off and invert functions
 */
esp_err_t jd9165ba_test_display_control(esp_lcd_panel_handle_t panel)
{
    ESP_LOGI(TAG, "=== Test 4: Display Control Test ===");

    // Fill with white for visibility
    if (!test_buffer) {
        test_buffer = malloc(LCD_WIDTH * LCD_HEIGHT * 3);
        if (!test_buffer) {
            ESP_LOGE(TAG, "Failed to allocate test buffer");
            return ESP_ERR_NO_MEM;
        }
    }

    fill_color_pattern(test_buffer, LCD_WIDTH, LCD_HEIGHT, 255, 255, 255);
    esp_lcd_panel_draw_bitmap(panel, 0, 0, LCD_WIDTH, LCD_HEIGHT, test_buffer);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Test display on/off
    ESP_LOGI(TAG, "Testing display OFF...");
    esp_lcd_panel_disp_on_off(panel, false);
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "Testing display ON...");
    esp_lcd_panel_disp_on_off(panel, true);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Test color inversion
    ESP_LOGI(TAG, "Testing color inversion ON...");
    esp_lcd_panel_invert_color(panel, true);
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "Testing color inversion OFF...");
    esp_lcd_panel_invert_color(panel, false);
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Display control test completed successfully!");
    return ESP_OK;
}

/**
 * @brief Test 5: Partial Update Test
 * Tests updating specific regions of the display
 */
esp_err_t jd9165ba_test_partial_update(esp_lcd_panel_handle_t panel)
{
    ESP_LOGI(TAG, "=== Test 5: Partial Update Test ===");

    // Create smaller buffer for partial updates
    uint32_t box_width = 200;
    uint32_t box_height = 200;
    size_t box_size = box_width * box_height * 3;
    uint8_t *box_buffer = malloc(box_size);

    if (!box_buffer) {
        ESP_LOGE(TAG, "Failed to allocate box buffer");
        return ESP_ERR_NO_MEM;
    }

    // Fill background with black
    if (!test_buffer) {
        test_buffer = malloc(LCD_WIDTH * LCD_HEIGHT * 3);
    }
    fill_color_pattern(test_buffer, LCD_WIDTH, LCD_HEIGHT, 0, 0, 0);
    esp_lcd_panel_draw_bitmap(panel, 0, 0, LCD_WIDTH, LCD_HEIGHT, test_buffer);

    // Draw colored boxes at different positions
    const struct {
        int x, y;
        uint8_t r, g, b;
    } boxes[] = {
        {100,  100, 255, 0,   0  },  // Red
        {400,  100, 0,   255, 0  },  // Green
        {700,  100, 0,   0,   255},  // Blue
        {250,  300, 255, 255, 0  },  // Yellow
        {550,  300, 255, 0,   255},  // Magenta
    };

    for (int i = 0; i < sizeof(boxes) / sizeof(boxes[0]); i++) {
        ESP_LOGI(TAG, "Drawing box %d at (%d, %d)...", i + 1, boxes[i].x, boxes[i].y);
        fill_color_pattern(box_buffer, box_width, box_height, boxes[i].r, boxes[i].g, boxes[i].b);

        esp_err_t ret = esp_lcd_panel_draw_bitmap(panel, boxes[i].x, boxes[i].y,
                                                   boxes[i].x + box_width,
                                                   boxes[i].y + box_height,
                                                   box_buffer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to draw box: %s", esp_err_to_name(ret));
            free(box_buffer);
            return ret;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    free(box_buffer);
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Partial update test completed successfully!");
    return ESP_OK;
}

/**
 * @brief Run all JD9165BA driver tests
 */
esp_err_t jd9165ba_run_all_tests(esp_lcd_panel_handle_t panel)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  JD9165BA LCD Driver Test Suite");
    ESP_LOGI(TAG, "  Resolution: %dx%d", LCD_WIDTH, LCD_HEIGHT);
    ESP_LOGI(TAG, "========================================");

    esp_err_t ret;

    // Test 1: Color Fill
    ret = jd9165ba_test_color_fill(panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Test 1 failed!");
        goto cleanup;
    }

    // Test 2: Gradient
    ret = jd9165ba_test_gradient(panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Test 2 failed!");
        goto cleanup;
    }

    // Test 3: Checkerboard
    ret = jd9165ba_test_checkerboard(panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Test 3 failed!");
        goto cleanup;
    }

    // Test 4: Display Control
    ret = jd9165ba_test_display_control(panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Test 4 failed!");
        goto cleanup;
    }

    // Test 5: Partial Update
    ret = jd9165ba_test_partial_update(panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Test 5 failed!");
        goto cleanup;
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ALL TESTS PASSED!");
    ESP_LOGI(TAG, "========================================");

cleanup:
    if (test_buffer) {
        free(test_buffer);
        test_buffer = NULL;
    }

    return ret;
}

/**
 * @brief Print driver information
 */
void jd9165ba_print_info(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "JD9165BA LCD Driver Information:");
    ESP_LOGI(TAG, "  - Resolution: %dx%d", LCD_WIDTH, LCD_HEIGHT);
    ESP_LOGI(TAG, "  - Interface: MIPI-DSI 4-lane");
    ESP_LOGI(TAG, "  - Color Format: RGB888");
    ESP_LOGI(TAG, "  - Reset GPIO: %d", LCD_RST_GPIO);
    ESP_LOGI(TAG, "  - Backlight GPIO: %d", LCD_BL_GPIO);
    ESP_LOGI(TAG, "========================================");
}
