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
 * @file bk_lvgl_compat.h
 * @brief Compatibility helpers for supported LVGL 9.x versions.
 */

#ifndef __BK_LVGL_COMPAT_H__
#define __BK_LVGL_COMPAT_H__

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set a tab's displayed text using the public API of the linked LVGL version.
 */
static inline void bk_lvgl_tabview_set_tab_text(
    lv_obj_t * obj,
    uint32_t index,
    const char * text)
{
#if LV_VERSION_CHECK(9, 5, 0)
    lv_tabview_set_tab_text(obj, index, text);
#else
    lv_tabview_rename_tab(obj, index, text);
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* __BK_LVGL_COMPAT_H__ */
