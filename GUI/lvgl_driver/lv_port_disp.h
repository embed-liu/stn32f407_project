/**
 * @file lv_port_disp.h
 * LVGL 显示移植层（HX8357D / 3.5寸 320*480 / FSMC）
 */

#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"

/*********************
 *      DEFINES
 *********************/
/* 绘制缓冲区行数：每行 320 像素，40 行约 25KB SRAM
 * 数值越小越省 RAM，但整屏刷新会被拆成更多次；40 行是本工程的经验值 */
#define LV_PORT_DISP_BUF_LINES 40

/**********************
 *      GLOBAL PROTOTYPES
 **********************/
void lv_port_disp_init(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_PORT_DISP_H */
