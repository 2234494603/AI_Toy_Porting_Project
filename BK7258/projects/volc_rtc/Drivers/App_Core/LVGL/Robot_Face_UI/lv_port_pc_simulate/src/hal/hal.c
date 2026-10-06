#include "hal.h"
#include LV_SDL_INCLUDE_PATH
#include "../lvgl/src/drivers/sdl/lv_sdl_window.h"

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <Windows.h>
#endif

lv_display_t * sdl_hal_init(int32_t w, int32_t h)
{
  lv_group_set_default(lv_group_create());

  /* 创建 LVGL 的 SDL 窗口，逻辑分辨率仍然是 w×h（仿真分辨率） */
  lv_display_t * disp = lv_sdl_window_create(w, h);
  if(disp) {
    lv_display_set_antialiasing(disp, true);
  }

  /* 根据物理屏幕尺寸自动计算缩放系数，保证窗口不会超出屏幕，且等比缩放 */
  SDL_DisplayMode dm;
  if(SDL_GetDesktopDisplayMode(0, &dm) == 0) {
    int screen_w = dm.w;
    int screen_h = dm.h;

#ifdef _WIN32
    /* 在 Windows 下，使用“工作区”大小，自动扣掉任务栏等区域，避免窗口被遮挡 */
    RECT rc;
    if(SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0)) {
      screen_w = (int)(rc.right - rc.left);
      screen_h = (int)(rc.bottom - rc.top);
    }
#endif

    float scale = 1.0f;

    if(screen_w < w || screen_h < h) {
      float scale_w = (float)screen_w / (float)w;
      float scale_h = (float)screen_h / (float)h;
      scale = scale_w < scale_h ? scale_w : scale_h;

      /* 只在需要缩小时再额外乘 0.9，留一点边距 */
      if(scale < 1.0f) {
        scale *= 0.9f;
      }
    }

    /* 只做缩小，不放大；异常情况下回退为 1.0 */
    if(scale > 1.0f) scale = 1.0f;
    if(scale <= 0.0f) scale = 1.0f;

    lv_sdl_window_set_zoom(disp, scale);
    /* 缩放后把窗口居中，避免多屏/虚拟屏导致窗口偏到负坐标区域 */
    lv_sdl_window_center(disp);
  }

  lv_indev_t * mouse = lv_sdl_mouse_create();
  lv_indev_set_group(mouse, lv_group_get_default());
  lv_indev_set_display(mouse, disp);
  lv_display_set_default(disp);
  /*Declare the image file.*/
  // LV_IMAGE_DECLARE(mouse_cursor_icon); 
  // lv_obj_t * cursor_obj;
  /*Create an image object for the cursor */
  // cursor_obj = lv_image_create(lv_screen_active()); 
  /*Set the image source*/
  // lv_image_set_src(cursor_obj, &mouse_cursor_icon);           
  /*Connect the image  object to the driver*/
  // lv_indev_set_cursor(mouse, cursor_obj);             

  lv_indev_t * mousewheel = lv_sdl_mousewheel_create();
  lv_indev_set_display(mousewheel, disp);
  lv_indev_set_group(mousewheel, lv_group_get_default());

  lv_indev_t * kb = lv_sdl_keyboard_create();
  lv_indev_set_display(kb, disp);
  lv_indev_set_group(kb, lv_group_get_default());

  return disp;
}
