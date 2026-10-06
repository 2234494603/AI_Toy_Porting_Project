# Robot_Face_UI integration for the BK7258 LVGL 8.3 firmware.
#
# Drop the complete Designer project into the sibling Robot_Face_UI directory.
# This fragment is included by dual_screen_avi_play and appends the generated
# sources and include paths to that component without hard-coded host paths.

set(robot_face_ui_dir ${CMAKE_CURRENT_LIST_DIR}/Robot_Face_UI)

if (NOT EXISTS ${robot_face_ui_dir}/beken_generated/neutral_init.c)
    message(FATAL_ERROR
        "Robot_Face_UI export not found at ${robot_face_ui_dir}. "
        "Place the complete Designer project in Drivers/App_Core/LVGL/Robot_Face_UI."
    )
endif()

file(GLOB robot_face_generated_sources
    ${robot_face_ui_dir}/beken_generated/*.c
    ${robot_face_ui_dir}/beken_generated/custom/*.c
)

# Designer exports LVGL 9 image descriptors, while BK7258 uses LVGL 8.3.
# Adapt image sources into the component build directory so the Designer export
# remains untouched and a dropped-in Robot_Face_UI project is still buildable.
file(GLOB robot_face_image_sources
    ${robot_face_ui_dir}/beken_generated/image/*.c
)
set(robot_face_lvgl8_image_sources)
set(robot_face_lvgl8_image_dir ${CMAKE_CURRENT_BINARY_DIR}/robot_face_lvgl8_images)
file(MAKE_DIRECTORY ${robot_face_lvgl8_image_dir})

foreach(robot_face_image_source IN LISTS robot_face_image_sources)
    get_filename_component(robot_face_image_name ${robot_face_image_source} NAME)
    set(robot_face_lvgl8_image_source ${robot_face_lvgl8_image_dir}/${robot_face_image_name})
    file(READ ${robot_face_image_source} robot_face_image_contents)
    string(REPLACE "lv_image_dsc_t" "lv_img_dsc_t" robot_face_image_contents "${robot_face_image_contents}")
    string(REPLACE "LV_COLOR_FORMAT_RGB565A8" "LV_IMG_CF_RGB565A8" robot_face_image_contents "${robot_face_image_contents}")
    string(REPLACE ".header.magic = LV_IMAGE_HEADER_MAGIC," ".header.always_zero = 0," robot_face_image_contents "${robot_face_image_contents}")
    string(REPLACE ".reserved = NULL," "" robot_face_image_contents "${robot_face_image_contents}")
    file(WRITE ${robot_face_lvgl8_image_source} "${robot_face_image_contents}")
    list(APPEND robot_face_lvgl8_image_sources ${robot_face_lvgl8_image_source})
endforeach()

list(APPEND srcs
    ${CMAKE_CURRENT_LIST_DIR}/src/robot_face_ui.c
    ${CMAKE_CURRENT_LIST_DIR}/Emotion/src/robot_face_emotion.c
    ${robot_face_generated_sources}
    ${robot_face_lvgl8_image_sources}
)
list(APPEND incs
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/Emotion/include
    ${robot_face_ui_dir}/beken_generated
    ${robot_face_ui_dir}/beken_generated/custom
)

set(robot_face_compat_definitions
    lv_point_precise_t=lv_point_t
    lv_obj_delete=lv_obj_del
    lv_obj_remove_flag=lv_obj_clear_flag
    lv_screen_load=lv_scr_load
    lv_screen_load_anim=lv_scr_load_anim
    lv_screen_load_anim_t=lv_scr_load_anim_t
    lv_obj_set_style_transform_rotation=lv_obj_set_style_transform_angle
    lv_obj_set_style_shadow_offset_x=lv_obj_set_style_shadow_ofs_x
    lv_obj_set_style_shadow_offset_y=lv_obj_set_style_shadow_ofs_y
    LV_IMAGE_DECLARE=LV_IMG_DECLARE
    lv_image_create=lv_img_create
    lv_image_set_src=lv_img_set_src
    lv_image_set_pivot=lv_img_set_pivot
    lv_image_set_rotation=lv_img_set_angle
    lv_obj_set_style_image_opa=lv_obj_set_style_img_opa
    lv_obj_set_style_image_recolor=lv_obj_set_style_img_recolor
    lv_obj_set_style_image_recolor_opa=lv_obj_set_style_img_recolor_opa
    lv_strcpy=__builtin_strcpy
)
