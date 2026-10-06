#include "robot_face_ui.h"

#include <stddef.h>

#include "beken_ui.h"

#define ROBOT_FACE_SCREEN_WIDTH       160
#define ROBOT_FACE_SCREEN_HEIGHT      160
#define ROBOT_FACE_EYE_OBJECT_WIDTH   70
#define ROBOT_FACE_EYE_X              ((ROBOT_FACE_SCREEN_WIDTH - ROBOT_FACE_EYE_OBJECT_WIDTH) / 2)
#define ROBOT_FACE_DEFAULT_COLOR      0x00F0F0U
#define ROBOT_FACE_MIN_DEMO_PERIOD_MS 500U

typedef void (*robot_face_page_fn_t)(bk_lv_ui_t *ui);

typedef struct
{
    robot_face_page_fn_t init;
    robot_face_page_fn_t destroy;
    lv_obj_t **page;
    lv_obj_t **left_eye;
    lv_obj_t **right_eye;
    bool image_eyes;
} robot_face_page_t;

typedef struct
{
    lv_obj_t *parent;
    lv_obj_t *eye[2];
    lv_timer_t *demo_timer;
    robot_face_expression_t expression;
    uint32_t color;
    bool swap_displays;
} robot_face_context_t;

static bk_lv_ui_t s_generated_ui;

#define ROBOT_FACE_LINE_PAGE(name) \
    { \
        init_page_##name, \
        destroy_page_##name, \
        &s_generated_ui.name, \
        &s_generated_ui.name##_left_eye_stroke, \
        &s_generated_ui.name##_right_eye_stroke, \
        false \
    }

#define ROBOT_FACE_IMAGE_PAGE(name, left_image, right_image) \
    { \
        init_page_##name, \
        destroy_page_##name, \
        &s_generated_ui.name, \
        &s_generated_ui.left_image, \
        &s_generated_ui.right_image, \
        true \
    }

static const robot_face_page_t s_pages[ROBOT_FACE_EXPRESSION_COUNT] =
{
    [ROBOT_FACE_NEUTRAL] = ROBOT_FACE_LINE_PAGE(neutral),
    [ROBOT_FACE_BLINK_HIGH] = ROBOT_FACE_LINE_PAGE(blink_high),
    [ROBOT_FACE_HAPPY] = ROBOT_FACE_LINE_PAGE(happy),
    [ROBOT_FACE_GLEE] = ROBOT_FACE_LINE_PAGE(glee),
    [ROBOT_FACE_BLINK_LOW] = ROBOT_FACE_LINE_PAGE(blink_low),
    [ROBOT_FACE_SAD] = ROBOT_FACE_LINE_PAGE(sad),
    [ROBOT_FACE_WORRIED] = ROBOT_FACE_LINE_PAGE(worried),
    [ROBOT_FACE_FOCUSED] = ROBOT_FACE_LINE_PAGE(focused),
    [ROBOT_FACE_ANNOYED] = ROBOT_FACE_LINE_PAGE(annoyed),
    [ROBOT_FACE_SURPRISED] = ROBOT_FACE_LINE_PAGE(surprised),
    [ROBOT_FACE_SKEPTICAL] = ROBOT_FACE_LINE_PAGE(skeptical),
    [ROBOT_FACE_SLEEPY] = ROBOT_FACE_LINE_PAGE(sleepy),
    [ROBOT_FACE_ANGRY] = ROBOT_FACE_LINE_PAGE(angry),
    [ROBOT_FACE_SCARED] = ROBOT_FACE_IMAGE_PAGE(scared, scared_image_1, scared_image_2),
};

static const char *const s_expression_names[ROBOT_FACE_EXPRESSION_COUNT] =
{
    [ROBOT_FACE_NEUTRAL] = "neutral",
    [ROBOT_FACE_BLINK_HIGH] = "blink_high",
    [ROBOT_FACE_HAPPY] = "happy",
    [ROBOT_FACE_GLEE] = "glee",
    [ROBOT_FACE_BLINK_LOW] = "blink_low",
    [ROBOT_FACE_SAD] = "sad",
    [ROBOT_FACE_WORRIED] = "worried",
    [ROBOT_FACE_FOCUSED] = "focused",
    [ROBOT_FACE_ANNOYED] = "annoyed",
    [ROBOT_FACE_SURPRISED] = "surprised",
    [ROBOT_FACE_SKEPTICAL] = "skeptical",
    [ROBOT_FACE_SLEEPY] = "sleepy",
    [ROBOT_FACE_ANGRY] = "angry",
    [ROBOT_FACE_SCARED] = "scared",
};

static robot_face_context_t s_face =
{
    .expression = ROBOT_FACE_NEUTRAL,
    .color = ROBOT_FACE_DEFAULT_COLOR,
};

static bool robot_face_obj_is_valid(const lv_obj_t *obj)
{
    return (obj != NULL) && lv_obj_is_valid(obj);
}

static void robot_face_clear_eyes(void)
{
    uint8_t i;

    for (i = 0; i < 2U; ++i)
    {
        if (robot_face_obj_is_valid(s_face.eye[i]))
        {
            lv_obj_del(s_face.eye[i]);
        }
        s_face.eye[i] = NULL;
    }
}

static bool robot_face_load_generated_page(robot_face_expression_t expression)
{
    const robot_face_page_t *page = &s_pages[expression];
    lv_obj_t *left_eye;
    lv_obj_t *right_eye;
    lv_coord_t left_y;
    lv_coord_t right_y;

    page->init(&s_generated_ui);

    left_eye = *page->left_eye;
    right_eye = *page->right_eye;
    if (!robot_face_obj_is_valid(*page->page) ||
        !robot_face_obj_is_valid(left_eye) ||
        !robot_face_obj_is_valid(right_eye))
    {
        page->destroy(&s_generated_ui);
        return false;
    }

    /* Move the generated eyes from the temporary page to the two displays. */
    lv_obj_set_parent(left_eye, s_face.parent);
    lv_obj_set_parent(right_eye, s_face.parent);

    left_y = s_face.swap_displays ? ROBOT_FACE_SCREEN_HEIGHT : 0;
    right_y = s_face.swap_displays ? 0 : ROBOT_FACE_SCREEN_HEIGHT;

    lv_obj_set_pos(left_eye, ROBOT_FACE_EYE_X, left_y);
    lv_obj_set_pos(right_eye, ROBOT_FACE_EYE_X, right_y);
    if (page->image_eyes)
    {
        lv_obj_set_style_img_recolor(left_eye, lv_color_hex(s_face.color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor_opa(left_eye, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(right_eye, lv_color_hex(s_face.color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor_opa(right_eye, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    else
    {
        lv_obj_set_style_line_color(left_eye, lv_color_hex(s_face.color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_line_color(right_eye, lv_color_hex(s_face.color), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    s_face.eye[0] = left_eye;
    s_face.eye[1] = right_eye;

    *page->left_eye = NULL;
    *page->right_eye = NULL;
    page->destroy(&s_generated_ui);
    return true;
}

static void robot_face_demo_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    robot_face_ui_next_expression();
}

void robot_face_ui_create(lv_obj_t *parent)
{
    if (parent == NULL)
    {
        return;
    }

    robot_face_ui_destroy();

    s_face.parent = parent;
    s_face.expression = ROBOT_FACE_NEUTRAL;
    s_face.color = ROBOT_FACE_DEFAULT_COLOR;
    s_face.swap_displays = false;

    lv_obj_set_style_bg_color(parent, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    robot_face_ui_set_expression(ROBOT_FACE_NEUTRAL);
}

void robot_face_ui_destroy(void)
{
    robot_face_ui_stop_demo();
    robot_face_clear_eyes();
    s_face.parent = NULL;
}

void robot_face_ui_set_expression(robot_face_expression_t expression)
{
    if ((expression < ROBOT_FACE_NEUTRAL) || (expression >= ROBOT_FACE_EXPRESSION_COUNT))
    {
        expression = ROBOT_FACE_NEUTRAL;
    }

    if (s_face.parent == NULL)
    {
        return;
    }

    robot_face_clear_eyes();
    if (robot_face_load_generated_page(expression))
    {
        s_face.expression = expression;
    }
}

robot_face_expression_t robot_face_ui_get_expression(void)
{
    return s_face.expression;
}

const char *robot_face_ui_expression_name(robot_face_expression_t expression)
{
    if ((expression < ROBOT_FACE_NEUTRAL) || (expression >= ROBOT_FACE_EXPRESSION_COUNT))
    {
        return "unknown";
    }

    return s_expression_names[expression];
}

void robot_face_ui_next_expression(void)
{
    robot_face_expression_t next = (robot_face_expression_t)(s_face.expression + 1);

    if (next >= ROBOT_FACE_EXPRESSION_COUNT)
    {
        next = ROBOT_FACE_NEUTRAL;
    }

    robot_face_ui_set_expression(next);
}

void robot_face_ui_start_demo(uint32_t period_ms)
{
    robot_face_ui_stop_demo();

    if (period_ms < ROBOT_FACE_MIN_DEMO_PERIOD_MS)
    {
        period_ms = ROBOT_FACE_MIN_DEMO_PERIOD_MS;
    }

    s_face.demo_timer = lv_timer_create(robot_face_demo_timer_cb, period_ms, NULL);
}

void robot_face_ui_stop_demo(void)
{
    if (s_face.demo_timer != NULL)
    {
        lv_timer_del(s_face.demo_timer);
        s_face.demo_timer = NULL;
    }
}

void robot_face_ui_set_color(uint32_t rgb888)
{
    const robot_face_page_t *page = &s_pages[s_face.expression];

    s_face.color = rgb888 & 0x00FFFFFFU;

    if (robot_face_obj_is_valid(s_face.eye[0]))
    {
        if (page->image_eyes)
        {
            lv_obj_set_style_img_recolor(s_face.eye[0], lv_color_hex(s_face.color), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor_opa(s_face.eye[0], LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        else
        {
            lv_obj_set_style_line_color(s_face.eye[0], lv_color_hex(s_face.color), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
    if (robot_face_obj_is_valid(s_face.eye[1]))
    {
        if (page->image_eyes)
        {
            lv_obj_set_style_img_recolor(s_face.eye[1], lv_color_hex(s_face.color), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor_opa(s_face.eye[1], LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        else
        {
            lv_obj_set_style_line_color(s_face.eye[1], lv_color_hex(s_face.color), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

void robot_face_ui_set_swap_displays(bool swap)
{
    if (s_face.swap_displays == swap)
    {
        return;
    }

    s_face.swap_displays = swap;
    robot_face_ui_set_expression(s_face.expression);
}
