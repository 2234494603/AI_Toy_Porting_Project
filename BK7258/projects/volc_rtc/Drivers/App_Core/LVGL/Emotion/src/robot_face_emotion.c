#include "robot_face_emotion.h"

#include <stdbool.h>

#include <components/log.h>
#include <os/os.h>

#define TAG "face_emotion"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)

#define ROBOT_FACE_EMOTION_RESTORE_CHECK_MS 200U

typedef struct
{
    robot_face_expression_t expression;
    uint32_t color;
} robot_face_emotion_style_t;

typedef struct
{
    robot_face_emotion_t current;
    robot_face_emotion_t base;
    lv_timer_t *restore_timer;
    uint32_t restore_tick;
    bool inited;
} robot_face_emotion_context_t;

static const robot_face_emotion_style_t s_emotion_styles[ROBOT_FACE_EMOTION_COUNT] =
{
    [ROBOT_FACE_EMOTION_BOOT]      = {ROBOT_FACE_NEUTRAL,   0x00F0F0U},
    [ROBOT_FACE_EMOTION_IDLE]      = {ROBOT_FACE_NEUTRAL,   0x00F0F0U},
    [ROBOT_FACE_EMOTION_LISTENING] = {ROBOT_FACE_FOCUSED,   0x00F0F0U},
    [ROBOT_FACE_EMOTION_THINKING]  = {ROBOT_FACE_SKEPTICAL, 0x8BE8FFU},
    [ROBOT_FACE_EMOTION_SPEAKING]  = {ROBOT_FACE_HAPPY,     0x00F0F0U},
    [ROBOT_FACE_EMOTION_HAPPY]     = {ROBOT_FACE_GLEE,      0x38FF9CU},
    [ROBOT_FACE_EMOTION_CONFUSED]  = {ROBOT_FACE_SKEPTICAL, 0xFFE66DU},
    [ROBOT_FACE_EMOTION_WORRIED]   = {ROBOT_FACE_WORRIED,   0xFFD166U},
    [ROBOT_FACE_EMOTION_SAD]       = {ROBOT_FACE_SAD,       0x6AA9FFU},
    [ROBOT_FACE_EMOTION_ANGRY]     = {ROBOT_FACE_ANGRY,     0xFF4040U},
    [ROBOT_FACE_EMOTION_SURPRISED] = {ROBOT_FACE_SURPRISED, 0xF7B2FFU},
    [ROBOT_FACE_EMOTION_SLEEPY]    = {ROBOT_FACE_SLEEPY,    0x7F8CFFU},
    [ROBOT_FACE_EMOTION_ERROR]     = {ROBOT_FACE_WORRIED,   0xFF4040U},
};

static const char *const s_emotion_names[ROBOT_FACE_EMOTION_COUNT] =
{
    [ROBOT_FACE_EMOTION_BOOT] = "boot",
    [ROBOT_FACE_EMOTION_IDLE] = "idle",
    [ROBOT_FACE_EMOTION_LISTENING] = "listening",
    [ROBOT_FACE_EMOTION_THINKING] = "thinking",
    [ROBOT_FACE_EMOTION_SPEAKING] = "speaking",
    [ROBOT_FACE_EMOTION_HAPPY] = "happy",
    [ROBOT_FACE_EMOTION_CONFUSED] = "confused",
    [ROBOT_FACE_EMOTION_WORRIED] = "worried",
    [ROBOT_FACE_EMOTION_SAD] = "sad",
    [ROBOT_FACE_EMOTION_ANGRY] = "angry",
    [ROBOT_FACE_EMOTION_SURPRISED] = "surprised",
    [ROBOT_FACE_EMOTION_SLEEPY] = "sleepy",
    [ROBOT_FACE_EMOTION_ERROR] = "error",
};

static robot_face_emotion_context_t s_emotion =
{
    .current = ROBOT_FACE_EMOTION_BOOT,
    .base = ROBOT_FACE_EMOTION_BOOT,
};

static bool robot_face_emotion_is_valid(robot_face_emotion_t emotion)
{
    return (emotion >= ROBOT_FACE_EMOTION_BOOT) && (emotion < ROBOT_FACE_EMOTION_COUNT);
}

static void robot_face_emotion_apply(robot_face_emotion_t emotion)
{
    const robot_face_emotion_style_t *style;

    if (!robot_face_emotion_is_valid(emotion))
    {
        emotion = ROBOT_FACE_EMOTION_IDLE;
    }

    style = &s_emotion_styles[emotion];
    robot_face_ui_stop_demo();
    robot_face_ui_set_expression(style->expression);
    robot_face_ui_set_color(style->color);
    s_emotion.current = emotion;
}

static void robot_face_emotion_restore_cb(lv_timer_t *timer)
{
    uint32_t now;

    (void)timer;

    if (s_emotion.restore_tick == 0)
    {
        return;
    }

    now = rtos_get_time();
    if ((int32_t)(now - s_emotion.restore_tick) >= 0)
    {
        s_emotion.restore_tick = 0;
        robot_face_emotion_apply(s_emotion.base);
    }
}

void robot_face_emotion_init(void)
{
    if (s_emotion.inited)
    {
        return;
    }

    s_emotion.current = ROBOT_FACE_EMOTION_BOOT;
    s_emotion.base = ROBOT_FACE_EMOTION_IDLE;
    s_emotion.restore_tick = 0;
    s_emotion.restore_timer = lv_timer_create(robot_face_emotion_restore_cb,
                                              ROBOT_FACE_EMOTION_RESTORE_CHECK_MS,
                                              NULL);
    s_emotion.inited = true;
    robot_face_emotion_apply(ROBOT_FACE_EMOTION_IDLE);
    LOGI("emotion init: %s\n", robot_face_emotion_name(s_emotion.current));
}

void robot_face_emotion_deinit(void)
{
    if (s_emotion.restore_timer != NULL)
    {
        lv_timer_del(s_emotion.restore_timer);
        s_emotion.restore_timer = NULL;
    }

    s_emotion.restore_tick = 0;
    s_emotion.inited = false;
}

void robot_face_emotion_set(robot_face_emotion_t emotion)
{
    if (!s_emotion.inited)
    {
        return;
    }

    if (!robot_face_emotion_is_valid(emotion))
    {
        emotion = ROBOT_FACE_EMOTION_IDLE;
    }

    s_emotion.restore_tick = 0;
    s_emotion.base = emotion;
    robot_face_emotion_apply(emotion);
    LOGI("emotion set: %s\n", robot_face_emotion_name(emotion));
}

void robot_face_emotion_show_for(robot_face_emotion_t emotion, uint32_t hold_ms)
{
    if (!s_emotion.inited)
    {
        return;
    }

    if (!robot_face_emotion_is_valid(emotion))
    {
        emotion = ROBOT_FACE_EMOTION_IDLE;
    }

    if (hold_ms == 0)
    {
        robot_face_emotion_set(emotion);
        return;
    }

    /* Continuous conversation returns to listening after the reply finishes. */
    if (emotion == ROBOT_FACE_EMOTION_SPEAKING)
    {
        s_emotion.base = ROBOT_FACE_EMOTION_LISTENING;
    }

    robot_face_emotion_apply(emotion);
    s_emotion.restore_tick = rtos_get_time() + hold_ms;
    LOGI("emotion show: %s hold=%u\n",
         robot_face_emotion_name(emotion),
         (unsigned int)hold_ms);
}

robot_face_emotion_t robot_face_emotion_get(void)
{
    return s_emotion.current;
}

const char *robot_face_emotion_name(robot_face_emotion_t emotion)
{
    if (!robot_face_emotion_is_valid(emotion))
    {
        return "unknown";
    }

    return s_emotion_names[emotion];
}
