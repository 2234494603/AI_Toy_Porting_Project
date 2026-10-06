#pragma once

#include <stdint.h>

#include "media_app.h"
#include "robot_face_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef media_robot_face_emotion_t robot_face_emotion_t;

#define ROBOT_FACE_EMOTION_BOOT       MEDIA_ROBOT_FACE_EMOTION_BOOT
#define ROBOT_FACE_EMOTION_IDLE       MEDIA_ROBOT_FACE_EMOTION_IDLE
#define ROBOT_FACE_EMOTION_LISTENING  MEDIA_ROBOT_FACE_EMOTION_LISTENING
#define ROBOT_FACE_EMOTION_THINKING   MEDIA_ROBOT_FACE_EMOTION_THINKING
#define ROBOT_FACE_EMOTION_SPEAKING   MEDIA_ROBOT_FACE_EMOTION_SPEAKING
#define ROBOT_FACE_EMOTION_HAPPY      MEDIA_ROBOT_FACE_EMOTION_HAPPY
#define ROBOT_FACE_EMOTION_CONFUSED   MEDIA_ROBOT_FACE_EMOTION_CONFUSED
#define ROBOT_FACE_EMOTION_WORRIED    MEDIA_ROBOT_FACE_EMOTION_WORRIED
#define ROBOT_FACE_EMOTION_SAD        MEDIA_ROBOT_FACE_EMOTION_SAD
#define ROBOT_FACE_EMOTION_ANGRY      MEDIA_ROBOT_FACE_EMOTION_ANGRY
#define ROBOT_FACE_EMOTION_SURPRISED  MEDIA_ROBOT_FACE_EMOTION_SURPRISED
#define ROBOT_FACE_EMOTION_SLEEPY     MEDIA_ROBOT_FACE_EMOTION_SLEEPY
#define ROBOT_FACE_EMOTION_ERROR      MEDIA_ROBOT_FACE_EMOTION_ERROR
#define ROBOT_FACE_EMOTION_COUNT      MEDIA_ROBOT_FACE_EMOTION_COUNT

void robot_face_emotion_init(void);
void robot_face_emotion_deinit(void);

void robot_face_emotion_set(robot_face_emotion_t emotion);
void robot_face_emotion_show_for(robot_face_emotion_t emotion, uint32_t hold_ms);
robot_face_emotion_t robot_face_emotion_get(void);
const char *robot_face_emotion_name(robot_face_emotion_t emotion);

#ifdef __cplusplus
}
#endif
