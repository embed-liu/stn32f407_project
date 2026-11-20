/**
 * @file lv_conf.h
 * LVGL 8.3.11 配置文件（STM32F407ZGT6 + HX8357D 3.5寸 320*480 FSMC 屏）
 *
 * 本文件只写出与默认值不同的配置项，其余选项由 lvgl/src/lv_conf_internal.h 给出默认值。
 */

/* clang-format off */
#if 1 /* 置 1 表示启用本配置文件 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/* 16 位色，RGB565，与 tftlcd 驱动的 u16 颜色格式一致 */
#define LV_COLOR_DEPTH 16

/* 高低字节不交换：disp_flush 里按 HX8357DN 的顺序先写高字节再写低字节 */
#define LV_COLOR_16_SWAP 0

/*=========================
   MEMORY SETTINGS
 *=========================*/

/* 不使用 malloc，改用 LVGL 内部静态内存池，避免和 FreeRTOS heap_4(36KB) 抢堆 */
#define LV_MEM_CUSTOM 0

#if LV_MEM_CUSTOM == 0
    /* 静态内存池大小，控件多时可加大，占用的是 SRAM（本工程余量约 80KB） */
    #define LV_MEM_SIZE (32U * 1024U)
    #define LV_MEM_ADR 0
#if LV_MEM_ADR == 0
    #undef LV_MEM_POOL_INCLUDE
    #undef LV_MEM_POOL_ALLOC
#endif
#endif

/* 绘制过程中临时缓冲块的最大数量 */
#define LV_MEM_BUF_MAX_NUM 16

/*=====================
   HAL SETTINGS
 *=====================*/

/* 界面刷新周期(ms) */
#define LV_DISP_DEF_REFR_PERIOD 30

/*=========================
   RENDERING
 *=========================*/

/* 复杂绘制（阴影、圆角、渐变、透明度）需要开启 */
#define LV_DRAW_COMPLEX 1

/* 阴影缓存，省 SRAM 关掉 */
#define LV_SHADOW_CACHE_SIZE 0

/* 图片缓存，不用图片，关掉 */
#define LV_IMG_CACHE_DEF_SIZE 0

/*=========================
   DEVICE DRIVER
 *=========================*/

/* 屏幕最大分辨率：绘制缓冲区按它定静态数组，实际分辨率取 tftlcd_data 的宽高 */
#define LV_HOR_RES_MAX 320
#define LV_VER_RES_MAX 480

/* 由 gui.c 里的 Task_GUI 调用 lv_tick_inc() 提供时基 */
#define LV_TICK_CUSTOM 0

/* 每英寸像素，按 3.5 寸 320*480 估算 */
#define LV_DPI_DEF 120

/*==================
   LOG / ASSERTS
 *==================*/

#define LV_USE_LOG 0

#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ 0

#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0
#define LV_USE_REFR_DEBUG 0

/*==================
   LAYOUT
 *==================*/

/* 界面卡片用 flex 排版 */
#define LV_USE_FLEX 1
#define LV_USE_GRID 0

/*==================
   FONT
 *==================*/

#define LV_FONT_MONTSERRAT_8 0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0
#define LV_FONT_MONTSERRAT_12_SUBPX 0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK 0
#define LV_FONT_UNSCII_8 0
#define LV_FONT_UNSCII_16 0
#define LV_FONT_CUSTOM_DECLARE

/* 默认字体 */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* 字体数据不按 32 位对齐，省 Flash */
#define LV_FONT_FMT_TXT_LARGE 0

#define LV_USE_FONT_COMPRESSED 0
#define LV_USE_FONT_SUBPX 0
#define LV_USE_FONT_PLACEHOLDER 1

/*==================
   TEXT
 *==================*/

#define LV_TXT_ENC LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_COLOR_CMD "#"

#define LV_USE_BIDI 0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*===================
   WIDGETS
 *====================*/
/* 控件全部使用 LVGL 默认开关（默认全开）。
 * 注意：LVGL 8.3 的 keyboard 依赖 btnmatrix、calendar 依赖 dropdown，
 * 单独关闭这些控件会导致 lvgl 内部 #error，需要按依赖关系整体开关。 */

/*===================
   EXAMPLES / DEMOS
 *====================*/
/* 本工程只用监控界面，示例和演示程序全部不编译 */
#define LV_BUILD_EXAMPLES 0

/*====================
   COMPILER SETTINGS
 *====================*/
/* 坐标不用 32 位，Cortex-M4 上更省 RAM */
#define LV_USE_LARGE_COORD 0

#endif /* LV_CONF_H */
#endif /* 内容启用 */
/* clang-format on */
