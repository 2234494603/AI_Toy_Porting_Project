#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    ROBOT_FACE_NEUTRAL = 0,
    ROBOT_FACE_BLINK_HIGH,
    ROBOT_FACE_HAPPY,
    ROBOT_FACE_GLEE,
    ROBOT_FACE_BLINK_LOW,
    ROBOT_FACE_SAD,
    ROBOT_FACE_WORRIED,
    ROBOT_FACE_FOCUSED,
    ROBOT_FACE_ANNOYED,
    ROBOT_FACE_SURPRISED,
    ROBOT_FACE_SKEPTICAL,
    ROBOT_FACE_SLEEPY,
    ROBOT_FACE_ANGRY,
    ROBOT_FACE_SCARED,
    ROBOT_FACE_EXPRESSION_COUNT
} robot_face_expression_t;

/*
 * The two physical 160 x 160 displays are exposed as one 160 x 320 LVGL
 * canvas. The screen module loads one Designer page and maps its left/right
 * eye objects to the two physical display regions.
 *
 * Call these functions while the LVGL display lock is held. Timer callbacks
 * created by robot_face_ui_start_demo() already run in the LVGL context.
 */
void robot_face_ui_create(lv_obj_t *parent);
void robot_face_ui_destroy(void);

void robot_face_ui_set_expression(robot_face_expression_t expression);
robot_face_expression_t robot_face_ui_get_expression(void);
const char *robot_face_ui_expression_name(robot_face_expression_t expression);

void robot_face_ui_next_expression(void);
void robot_face_ui_start_demo(uint32_t period_ms);
void robot_face_ui_stop_demo(void);

void robot_face_ui_set_color(uint32_t rgb888);
void robot_face_ui_set_swap_displays(bool swap);

#ifdef __cplusplus
}
#endif
