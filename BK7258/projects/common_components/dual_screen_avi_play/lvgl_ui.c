#include <os/os.h>
#include "lcd_act.h"
#include "media_app.h"
#include "lv_vendor.h"
#include "lvgl.h"
#include "driver/drv_tp.h"
#include "yuv_encode.h"
#include "media_evt.h"
#include "bk_avi_play.h"

#if CONFIG_ROBOT_FACE_UI
#include "robot_face_emotion.h"
#include "robot_face_ui.h"
#endif

#define TAG "lvgl_ui"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

static lv_vnd_config_t lv_vnd_config = {0};
#if !CONFIG_ROBOT_FACE_UI
static lv_obj_t *label1 = NULL;
static lv_obj_t *label2 = NULL;
#endif
static uint8_t lvgl_ui_index = LVGL_UI_DISP_IN_INIT;

LV_FONT_DECLARE(lv_font);

bk_err_t lvgl_event_close_handle(media_mailbox_msg_t *msg)
{
    LOGI("%s \r\n", __func__);

    lv_vendor_disp_lock();

#if CONFIG_ROBOT_FACE_UI
    robot_face_emotion_deinit();
    robot_face_ui_destroy();
#else
    if (lvgl_ui_index == LVGL_UI_DISP_IN_TEXT) {
        bk_avi_play_stop();
    } else {
        if (label1) {
            lv_obj_del(label1);
            label1 = NULL;
        }

        if (label2) {
            lv_obj_del(label2);
            label2 = NULL;
        }
    }
#endif

    lv_vendor_disp_unlock();

    lv_vendor_stop();

    lcd_display_close();

#if !CONFIG_ROBOT_FACE_UI
    if (lvgl_ui_index == LVGL_UI_DISP_IN_TEXT) {
        bk_avi_play_close();
    }
#endif

    return BK_OK;
}

bk_err_t lvgl_event_open_handle(media_mailbox_msg_t *msg)
{
#if !CONFIG_ROBOT_FACE_UI
    bk_err_t ret = BK_FAIL;
#endif

    LOGI("%s \n", __func__);

    lcd_open_t *lcd_open = (lcd_open_t *)msg->param;

    if (lv_vnd_config.draw_pixel_size == 0) {
#ifdef CONFIG_LVGL_USE_PSRAM
#define PSRAM_DRAW_BUFFER ((0x60000000UL) + 5 * 1024 * 1024)
        lv_vnd_config.draw_pixel_size = ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi);
        lv_vnd_config.draw_buf_2_1 = (lv_color_t *)PSRAM_DRAW_BUFFER;
        lv_vnd_config.draw_buf_2_2 = (lv_color_t *)(PSRAM_DRAW_BUFFER + lv_vnd_config.draw_pixel_size * sizeof(lv_color_t));
#else
#define PSRAM_FRAME_BUFFER ((0x60000000UL) + 5 * 1024 * 1024)
        lv_vnd_config.draw_pixel_size = ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) / 10;
        lv_vnd_config.draw_buf_2_1 = LV_MEM_CUSTOM_ALLOC(lv_vnd_config.draw_pixel_size * sizeof(lv_color_t));
        lv_vnd_config.draw_buf_2_2 = NULL;
        lv_vnd_config.frame_buf_1 = (lv_color_t *)PSRAM_FRAME_BUFFER;
        lv_vnd_config.frame_buf_2 = NULL;//(lv_color_t *)(PSRAM_FRAME_BUFFER + ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) * sizeof(lv_color_t));
#endif
#if (CONFIG_LCD_SPI_DEVICE_NUM > 1 || CONFIG_LCD_QSPI_DEVICE_NUM > 1)
        lv_vnd_config.lcd_hor_res = ppi_to_pixel_x(lcd_open->device_ppi);
        lv_vnd_config.lcd_ver_res = ppi_to_pixel_y(lcd_open->device_ppi) * 2;
#else
        lv_vnd_config.lcd_hor_res = ppi_to_pixel_x(lcd_open->device_ppi);
        lv_vnd_config.lcd_ver_res = ppi_to_pixel_y(lcd_open->device_ppi);
#endif
        lv_vnd_config.rotation = ROTATE_NONE;

        lv_vendor_init(&lv_vnd_config);
    }

    lcd_display_open(lcd_open);

#if (CONFIG_TP)
    drv_tp_open(ppi_to_pixel_x(lcd_open->device_ppi), ppi_to_pixel_y(lcd_open->device_ppi), TP_MIRROR_NONE);
#endif

#if CONFIG_ROBOT_FACE_UI
    lv_vendor_disp_lock();

    robot_face_ui_create(lv_scr_act());
    robot_face_emotion_init();
    lvgl_ui_index = LVGL_UI_DISP_IN_TEXT_AND_IMAGE;

    lv_vendor_disp_unlock();
#else
    if (lvgl_ui_index == LVGL_UI_DISP_IN_TEXT_AND_IMAGE) {
        lv_vendor_disp_lock();

        label1 = lv_label_create(lv_scr_act());
        lv_label_set_text(label1, "图 像");
        lv_obj_set_style_text_font(label1, &lv_font, 0);
        lv_obj_align(label1, LV_ALIGN_TOP_MID, 0, 60);

        label2 = lv_label_create(lv_scr_act());
        lv_label_set_text(label2, "识 别");
        lv_obj_set_style_text_font(label2, &lv_font, 0);
        lv_obj_align(label2, LV_ALIGN_BOTTOM_MID, 0, -60);

        lv_vendor_disp_unlock();
    } else {
        ret = bk_avi_play_open("/genie_eye.avi", true, true);
        if (ret != BK_OK) {
            LOGE("%s bk_avi_play_open failed\r\n", __func__);
            lcd_display_close();
            return ret;
        }

        lv_vendor_disp_lock();

        bk_avi_play_start();

        lvgl_ui_index = LVGL_UI_DISP_IN_TEXT;

        lv_vendor_disp_unlock();
    }
#endif

    lv_vendor_start();

    return BK_OK;
}

bk_err_t lvgl_event_switch_ui_handle(media_mailbox_msg_t *msg)
{
    LOGI("%s \r\n", __func__);

    lvgl_ui_index_t ui_index = msg->param;

#if CONFIG_ROBOT_FACE_UI
    lv_vendor_disp_lock();

    if (ui_index == LVGL_UI_DISP_IN_TEXT) {
        robot_face_emotion_set(ROBOT_FACE_EMOTION_LISTENING);
    } else if ((ui_index == LVGL_UI_DISP_IN_TEXT_AND_IMAGE) ||
               (ui_index == LVGL_UI_DISP_IN_INIT)) {
        robot_face_emotion_set(ROBOT_FACE_EMOTION_IDLE);
    } else {
        lv_vendor_disp_unlock();
        LOGE("%s index is invalid\r\n", __func__);
        return BK_ERR_PARAM;
    }

    lvgl_ui_index = ui_index;
    lv_vendor_disp_unlock();
#else
    if (ui_index == LVGL_UI_DISP_IN_TEXT) {
        if (lvgl_ui_index == LVGL_UI_DISP_IN_TEXT_AND_IMAGE) {
            lv_vendor_disp_lock();
            if (label1) {
                lv_obj_del(label1);
                label1 = NULL;
            }

            if (label2) {
                lv_obj_del(label2);
                label2 = NULL;
            }

            bk_avi_play_start();
            lvgl_ui_index = LVGL_UI_DISP_IN_TEXT;
            lv_vendor_disp_unlock();
        }
    } else if (ui_index == LVGL_UI_DISP_IN_TEXT_AND_IMAGE) {
        if (lvgl_ui_index == LVGL_UI_DISP_IN_TEXT) {
            lv_vendor_disp_lock();
            bk_avi_play_stop();

            label1 = lv_label_create(lv_scr_act());
            lv_label_set_text(label1, "图 像");
            lv_obj_set_style_text_font(label1, &lv_font, 0);
            lv_obj_align(label1, LV_ALIGN_TOP_MID, 0, 60);

            label2 = lv_label_create(lv_scr_act());
            lv_label_set_text(label2, "识 别");
            lv_obj_set_style_text_font(label2, &lv_font, 0);
            lv_obj_align(label2, LV_ALIGN_BOTTOM_MID, 0, -60);

            lvgl_ui_index = LVGL_UI_DISP_IN_TEXT_AND_IMAGE;
            lv_vendor_disp_unlock();
        }
    } else if (ui_index == LVGL_UI_DISP_IN_INIT) {
        lvgl_ui_index = LVGL_UI_DISP_IN_INIT;
    } else {
        LOGE("%s index is invalid\r\n", __func__);
    }
#endif

    return BK_OK;
}

#if CONFIG_ROBOT_FACE_UI
bk_err_t lvgl_event_robot_face_emotion_handle(media_mailbox_msg_t *msg)
{
    robot_face_emotion_t emotion = (robot_face_emotion_t)(msg->param & 0xFFU);
    uint32_t hold_ms = ((msg->param >> 8) & 0x00FFFFFFU) * 100U;

    if (emotion >= ROBOT_FACE_EMOTION_COUNT)
    {
        LOGE("%s emotion is invalid: %u\r\n",
             __func__, (unsigned int)emotion);
        return BK_ERR_PARAM;
    }

    lv_vendor_disp_lock();
    if (hold_ms > 0)
    {
        robot_face_emotion_show_for(emotion, hold_ms);
    }
    else
    {
        robot_face_emotion_set(emotion);
    }
    lv_vendor_disp_unlock();

    return BK_OK;
}
#endif

void lvgl_event_handle(media_mailbox_msg_t *msg)
{
    bk_err_t ret = BK_FAIL;

    switch (msg->event)
    {
        case EVENT_LVGL_OPEN_IND:
            ret = lvgl_event_open_handle(msg);
            break;

        case EVENT_LVGL_CLOSE_IND:
            ret = lvgl_event_close_handle(msg);
            break;

        case EVENT_LVGL_SWITCH_UI_IND:
            ret = lvgl_event_switch_ui_handle(msg);
            break;

#if CONFIG_ROBOT_FACE_UI
        case EVENT_LVGL_ROBOT_FACE_EMOTION_IND:
            ret = lvgl_event_robot_face_emotion_handle(msg);
            break;
#endif

        default:
            break;
    }

    msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
}

