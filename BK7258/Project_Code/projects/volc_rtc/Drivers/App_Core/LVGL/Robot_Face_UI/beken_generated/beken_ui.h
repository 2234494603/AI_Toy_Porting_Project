/*
 * Copyright (c) 2025 BekenCorp. All rights reserved.
 * 
 * This software is proprietary and confidential. No part of this software may be
 * reproduced, distributed, or transmitted in any form or by any means, including
 * photocopying, recording, or other electronic or mechanical methods, without the
 * prior written permission of BekenCorp, except in the case of brief quotations
 * embodied in critical reviews and certain other noncommercial uses permitted
 * by copyright law.
 * 
 * For permission requests, write to BekenCorp at armino_support@bekencorp.com.

 * Author: Beken LVGL Designer Tool
*/
/**
 * @file beken_ui.c
 * @brief Beken UI implementation file
 * 
 * This file contains the implementation of the Beken UI system.
 * Customers can modify this file to customize their UI without
 * touching the main application code or build system.
 */

#ifndef __BEKEN_UI_H__
#define __BEKEN_UI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/* BEKEN_USER_CODE_BEGIN includes */
/* Add your custom code here. Content between BEGIN/END is preserved on re-export. */
/* BEKEN_USER_CODE_END includes */

/* Display configuration */
#define SCREEN_WIDTH    160
#define SCREEN_HEIGHT   160

typedef struct
{
    /* Page: 0 objects */
    lv_obj_t *neutral;
    lv_obj_t *neutral_left_eye_stroke;
    lv_obj_t *neutral_right_eye_stroke;
    /* Page: 1 objects */
    lv_obj_t *blink_high;
    lv_obj_t *blink_high_left_eye_stroke;
    lv_obj_t *blink_high_right_eye_stroke;
    /* Page: 2 objects */
    lv_obj_t *happy;
    lv_obj_t *happy_left_eye_stroke;
    lv_obj_t *happy_right_eye_stroke;
    /* Page: 3 objects */
    lv_obj_t *glee;
    lv_obj_t *glee_left_eye_stroke;
    lv_obj_t *glee_right_eye_stroke;
    /* Page: 4 objects */
    lv_obj_t *blink_low;
    lv_obj_t *blink_low_left_eye_stroke;
    lv_obj_t *blink_low_right_eye_stroke;
    /* Page: 5 objects */
    lv_obj_t *sad;
    lv_obj_t *sad_left_eye_stroke;
    lv_obj_t *sad_right_eye_stroke;
    /* Page: 6 objects */
    lv_obj_t *worried;
    lv_obj_t *worried_left_eye_stroke;
    lv_obj_t *worried_right_eye_stroke;
    /* Page: 7 objects */
    lv_obj_t *focused;
    lv_obj_t *focused_left_eye_stroke;
    lv_obj_t *focused_right_eye_stroke;
    /* Page: 8 objects */
    lv_obj_t *annoyed;
    lv_obj_t *annoyed_left_eye_stroke;
    lv_obj_t *annoyed_right_eye_stroke;
    /* Page: 9 objects */
    lv_obj_t *surprised;
    lv_obj_t *surprised_left_eye_stroke;
    lv_obj_t *surprised_right_eye_stroke;
    /* Page: 10 objects */
    lv_obj_t *skeptical;
    lv_obj_t *skeptical_left_eye_stroke;
    lv_obj_t *skeptical_right_eye_stroke;
    /* Page: 11 objects */
    lv_obj_t *sleepy;
    lv_obj_t *sleepy_left_eye_stroke;
    lv_obj_t *sleepy_right_eye_stroke;
    /* Page: 12 objects */
    lv_obj_t *angry;
    lv_obj_t *angry_left_eye_stroke;
    lv_obj_t *angry_right_eye_stroke;
    /* Page: 13 objects */
    lv_obj_t *scared;
    lv_obj_t *scared_image_1;
    lv_obj_t *scared_image_2;
} bk_lv_ui_t;

void init_page_neutral(bk_lv_ui_t *bk_ui);
void destroy_page_neutral(bk_lv_ui_t *bk_ui);
void init_page_blink_high(bk_lv_ui_t *bk_ui);
void destroy_page_blink_high(bk_lv_ui_t *bk_ui);
void init_page_happy(bk_lv_ui_t *bk_ui);
void destroy_page_happy(bk_lv_ui_t *bk_ui);
void init_page_glee(bk_lv_ui_t *bk_ui);
void destroy_page_glee(bk_lv_ui_t *bk_ui);
void init_page_blink_low(bk_lv_ui_t *bk_ui);
void destroy_page_blink_low(bk_lv_ui_t *bk_ui);
void init_page_sad(bk_lv_ui_t *bk_ui);
void destroy_page_sad(bk_lv_ui_t *bk_ui);
void init_page_worried(bk_lv_ui_t *bk_ui);
void destroy_page_worried(bk_lv_ui_t *bk_ui);
void init_page_focused(bk_lv_ui_t *bk_ui);
void destroy_page_focused(bk_lv_ui_t *bk_ui);
void init_page_annoyed(bk_lv_ui_t *bk_ui);
void destroy_page_annoyed(bk_lv_ui_t *bk_ui);
void init_page_surprised(bk_lv_ui_t *bk_ui);
void destroy_page_surprised(bk_lv_ui_t *bk_ui);
void init_page_skeptical(bk_lv_ui_t *bk_ui);
void destroy_page_skeptical(bk_lv_ui_t *bk_ui);
void init_page_sleepy(bk_lv_ui_t *bk_ui);
void destroy_page_sleepy(bk_lv_ui_t *bk_ui);
void init_page_angry(bk_lv_ui_t *bk_ui);
void destroy_page_angry(bk_lv_ui_t *bk_ui);
void init_page_scared(bk_lv_ui_t *bk_ui);
void destroy_page_scared(bk_lv_ui_t *bk_ui);

/* declare image */
LV_IMAGE_DECLARE(love_57x50_RGB565A8_NONE);

/* declare fonts */

/**
 * @brief Initialize the Beken UI system
 * 
 * This function initializes the UI components and creates the main interface.
 * Customers can modify this function to customize their UI layout.
 */
void beken_ui_init(void);

/**
 * @brief Get the configured screen width
 * @return Screen width in pixels
 */
int beken_get_screen_width(void);

/**
 * @brief Get the configured screen height
 * @return Screen height in pixels
 */
int beken_get_screen_height(void);

extern bk_lv_ui_t bk_lv_tool_ui;

/* Digital clock functions */
void lv_digital_clock_timer(lv_timer_t *timer);
void lv_digital_clock_register(lv_obj_t *label, int show_second, int use_ampm, int hour, int minute, int second);
void lv_digital_clock_unregister(lv_obj_t *label);
void lv_digital_clock_register(lv_obj_t *label, int show_second, int use_ampm, int hour, int minute, int second);
void lv_digital_clock_unregister(lv_obj_t *label);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* __BEKEN_UI_H__ */
