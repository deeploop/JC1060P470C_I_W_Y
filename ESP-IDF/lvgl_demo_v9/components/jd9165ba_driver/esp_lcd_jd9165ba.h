/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_lcd_jd9165ba.h
 * @brief ESP LCD JD9165BA MIPI-DSI LCD Panel Driver
 *
 * This driver supports JD9165BA 1024x600 MIPI-DSI LCD panel for ESP32-P4.
 * Compatible with LVGL and MicroPython.
 */

#pragma once

#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_mipi_dsi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LCD panel configuration for JD9165BA
 */
typedef struct {
    /**
     * @brief Reset GPIO number, set to -1 if not used
     */
    int reset_gpio_num;

    /**
     * @brief Color space setting
     * Set to 1 for RGB color order, 0 for BGR
     */
    struct {
        unsigned int reset_level: 1;    /*!< Level of reset signal in reset stage */
        unsigned int color_space_bgr: 1; /*!< Whether to use BGR color space */
        unsigned int mirror_horizontal: 1; /*!< Whether to mirror horizontally */
        unsigned int mirror_vertical: 1;   /*!< Whether to mirror vertically */
    } flags;
} esp_lcd_jd9165ba_config_t;

/**
 * @brief Create LCD panel for model JD9165BA
 *
 * @param[in] io LCD panel IO handle
 * @param[in] panel_dev_config General panel device configuration
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid argument
 *      - ESP_ERR_NO_MEM: Out of memory
 *      - ESP_FAIL: Fail to create panel
 */
esp_err_t esp_lcd_new_panel_jd9165ba(const esp_lcd_panel_io_handle_t io,
                                       const esp_lcd_panel_dev_config_t *panel_dev_config,
                                       esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief JD9165BA vendor specific configuration
 *
 * @param[in] panel LCD panel handle
 * @param[in] jd9165ba_config JD9165BA specific configuration
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid argument
 */
esp_err_t esp_lcd_jd9165ba_set_config(esp_lcd_panel_handle_t panel,
                                        const esp_lcd_jd9165ba_config_t *jd9165ba_config);

#ifdef __cplusplus
}
#endif
