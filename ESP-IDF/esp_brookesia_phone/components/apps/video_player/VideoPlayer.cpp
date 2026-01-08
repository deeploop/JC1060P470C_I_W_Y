/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <fcntl.h>
#include <dirent.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "VideoPlayer.hpp"
#include "bsp/esp-bsp.h"

#define APP_MJPEG_PATH              "/sdcard/mjpeg"
#define APP_SUPPORT_VIDEO_FILE_EXT  ".mjpeg"
#define APP_BGM_DIR   BSP_SPIFFS_MOUNT_POINT "/music"
#define APP_MAX_VIDEO_NUM           (15)
#define APP_VIDEO_FRAME_BUF_SIZE    (720 * 1280 * BSP_LCD_BITS_PER_PIXEL / 8)
#define APP_CACHE_BUF_SIZE          (64 * 1024)
#define APP_BREAKING_NEWS_TEXT      "This example demonstrates the JPEG decoding capability of the ESP32-P4"

using namespace std;

static jpeg_decode_cfg_t decode_cfg_rgb = {
    .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
    .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
};


static jpeg_decode_memory_alloc_cfg_t rx_mem_cfg = {
    .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
};

static const char *TAG = "AppVideoPlayer";

LV_IMG_DECLARE(breaking_news);
LV_IMG_DECLARE(img_app_video_player);

AppVideoPlayer::AppVideoPlayer():
    ESP_Brookesia_PhoneApp("Video Player", &img_app_video_player, true), // auto_resize_visual_area
    _file_iterator(NULL)
{

}

AppVideoPlayer::~AppVideoPlayer()
{
}

bool AppVideoPlayer::run(void)
{
    bsp_display_lock(0);
    video_background = lv_obj_create(NULL);
    lv_obj_set_size(video_background,1024,600);
    lv_obj_clear_flag( video_background, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
    lv_obj_set_flex_flow(video_background,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(video_background, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(video_background, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
    lv_obj_set_style_bg_opa(video_background, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(video_background, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(video_background, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(video_background, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(video_background, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(video_background, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(video_background, 0, LV_PART_MAIN| LV_STATE_DEFAULT);

    video_canvas = lv_canvas_create(video_background);
    lv_obj_set_size(video_canvas,1024,600);
    lv_obj_set_align( video_canvas, LV_ALIGN_CENTER );
    lv_obj_add_flag( video_canvas, LV_OBJ_FLAG_ADV_HITTEST );   /// Flags
    lv_obj_clear_flag( video_canvas, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

    lv_scr_load(video_background);
    bsp_display_unlock();

    jpeg_data_buffer[0] = (uint8_t*)jpeg_alloc_decoder_mem(1024 * 608 *2 , &rx_mem_cfg, &jpeg_buf_size[0]);
    if(!jpeg_data_buffer[0])
        ESP_LOGE(TAG,"NO mem Jpeg_data_buffer[0]");

    jpeg_data_buffer[1] = (uint8_t*)jpeg_alloc_decoder_mem(1024 * 608 *2 , &rx_mem_cfg, &jpeg_buf_size[1]);
    if(!jpeg_data_buffer[1])
        ESP_LOGE(TAG,"NO mem Jpeg_data_buffer[1]");

    playing = true;
    xTaskCreatePinnedToCore((TaskFunction_t)play_avi_task, "avi_play_task", 4096, this, 4, NULL, 1);
    xSemaphoreGive(semph_event);

    return true;
}

bool AppVideoPlayer::pause(void)
{
    ESP_LOGI(TAG,"video plater pause");
    avi_player_play_stop(avi_handle);

    return true;
}

bool AppVideoPlayer::resume(void)
{
    ESP_LOGI(TAG,"video plater re");
//    avi_player_play_from_file(avi_handle,_video_path);
    return true;
}

bool AppVideoPlayer::back(void)
{
    return notifyCoreClosed();
}

bool AppVideoPlayer::close(void)
{
    playing = false;
    vTaskDelay(pdMS_TO_TICKS(5));
    avi_player_play_stop(avi_handle);
    vTaskDelay(pdMS_TO_TICKS(5));
    xSemaphoreGive(semph_event);
    vTaskDelay(pdMS_TO_TICKS(5));
    

    // lv_obj_del(video_canvas);
    // lv_obj_clean(lv_scr_act());

    if(jpeg_data_buffer[0])
    {
        free(jpeg_data_buffer[0]);
        jpeg_buf_size[0] = 0;
    }

    if(jpeg_data_buffer[1])
    {
        free(jpeg_data_buffer[1]);
        jpeg_buf_size[1] = 0;
    }

    return true;
}

bool AppVideoPlayer::init(void)
{
    if (bsp_extra_player_init() != ESP_OK) {
        ESP_LOGE(TAG, "Play init with SPIFFS failed");
        return false;
    }

    if (bsp_extra_file_instance_init(APP_MJPEG_PATH, &_file_iterator) != ESP_OK) {
        ESP_LOGE(TAG, "bsp_extra_file_instance_init failed");
        return false;
    }

    avi_cnt = file_iterator_get_count(_file_iterator);

    // end_play = false;
    avi_player_config_t config = {
        .buffer_size = 80 * 1024,
        .video_cb = video_write,
        .audio_cb = audio_write,
        .audio_set_clock_cb = audio_set_clock,
        .avi_play_end_cb = avi_play_end,
        .priority = 4,
        .coreID = 0,
        .user_data = this,
        .stack_size = 4096,
        // It must not be set to `true` when reading data from flash.
        // .stack_in_psram = true,
    };
    avi_player_init(config, &avi_handle);
    ESP_LOGI(TAG,"avi player init success");

    semph_event = xSemaphoreCreateBinary();

    jpeg_decode_engine_cfg_t decode_eng_cfg = {
        .timeout_ms = 40,
    };

    ESP_ERROR_CHECK(jpeg_new_decoder_engine(&decode_eng_cfg, &avi_jpgd_handle));
    ESP_LOGI(TAG,"jpeg decoder init success");

    return true;
}

void AppVideoPlayer::audio_set_clock(uint32_t rate, uint32_t bits_cfg, uint32_t ch, void *arg)
{
    ESP_LOGI(TAG, "Audio set clock, rate %" PRIu32 ", bits %" PRIu32 ", ch %" PRIu32, rate, bits_cfg, ch);
    // bsp_extra_codec_set_fs_play(rate,bits_cfg,I2S_SLOT_MODE_STEREO);
    bsp_extra_codec_mute_set(false);
    ESP_LOGI(TAG,"set clock success");
}

void AppVideoPlayer::audio_write(frame_data_t *data, void *arg)
{
    AppVideoPlayer *app = (AppVideoPlayer *)arg;
    // ESP_LOGI(TAG, "Audio write: %d", data->data_bytes);
    if(data->data_bytes != 0 && app->playing)
    {
        size_t write_bytes;
        bsp_extra_i2s_write(data->data,data->data_bytes,&write_bytes,0);
    }
    
}

void AppVideoPlayer::video_write(frame_data_t *data, void *arg)
{
    AppVideoPlayer *app = (AppVideoPlayer *)arg;
    // ESP_LOGI(TAG, "Video write: %d", data->data_bytes);
    // ESP_LOGI(TAG,"video write");
    if(data->data_bytes != 0 )
    {
        if(!app->playing)
        {
            return;
        }
       
        if(app->index == 0)
        {
            app->index = 1;
        }
        else if(app->index == 1)
        {
            app->index = 0;
        }

        uint32_t out_size_image = 0;
        
        esp_err_t ret = jpeg_decoder_process(app->avi_jpgd_handle, &decode_cfg_rgb, data->data, data->data_bytes, app->jpeg_data_buffer[app->index], app->jpeg_buf_size[app->index], &out_size_image);
        
        if(ret != ESP_OK)
        {
            return;
        }

        bsp_display_lock(0);
        lv_canvas_set_buffer(app->video_canvas,app->jpeg_data_buffer[app->index],1024,600,LV_IMG_CF_TRUE_COLOR);
        bsp_display_unlock();
    }
    
    return;
}

void AppVideoPlayer::avi_play_end(void *arg)
{
    AppVideoPlayer *app = (AppVideoPlayer *)arg;
    xSemaphoreGive(app->semph_event);
}

void AppVideoPlayer::play_avi_task(void *arg)
{
    AppVideoPlayer *app = (AppVideoPlayer *)arg;
    while(app->playing)
    {
        xSemaphoreTake(app->semph_event,portMAX_DELAY);
        if(!app->playing)
            break;
        
        file_iterator_get_full_path_from_index(app->_file_iterator,app->cnt,app->_video_path,256);
        ESP_LOGI(TAG,"play %s",app->_video_path);
        avi_player_play_from_file(app->avi_handle,app->_video_path);
        app->cnt ++;
        if(app->cnt > app->avi_cnt -1)
            app->cnt = 0;
    }

     
    ESP_LOGI(TAG, "Video player  task exit");
    // avi_player_play_stop(app->avi_handle);
    vTaskDelete(NULL);

}
