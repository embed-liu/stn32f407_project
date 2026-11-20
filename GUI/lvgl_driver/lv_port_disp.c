/**
 * @file lv_port_disp.c
 * LVGL 显示移植层：把 LVGL 的刷屏回调接到本工程已有的 tftlcd 驱动上
 *
 * 目标屏：HX8357D(N) 3.5 寸 320*480，FSMC 驱动
 *
 * 注意：本文件不修改 tftlcd.c / tftlcd.h，只调用它已有的公有接口：
 *     LCD_Set_Window(x1, y1, x2, y2) —— 设定一次显示窗口
 *     TFTLCD->LCD_DATA               —— 窗口内像素自增写入
 * 其中 HX8357DN 是 8 位总线时序，一个像素要写两次（先高字节后低字节），
 * 与 tftlcd.c 里 LCD_WriteData_Color() 的处理完全一致；这里做成内联直写，
 * 省掉每个像素的函数调用开销（整屏 153,600 像素，差别很明显）。
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include "tftlcd.h"

/*********************
 *      DEFINES
 *********************/
#define DISP_BUF_SIZE (LV_HOR_RES_MAX * LV_PORT_DISP_BUF_LINES)

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 注册 LVGL 显示设备
 * @note  必须在 TFTLCD_Init() 之后调用（需要 tftlcd_data 里的宽高）
 */
void lv_port_disp_init(void)
{
    /*-------------------------
     * 初始化显示设备
     * -----------------------*/
    disp_init();

    /*-----------------------------
     * 创建绘制缓冲区
     * 采用单缓冲 + 部分刷新：缓冲区放 40 行像素
     *----------------------------*/
    static lv_disp_draw_buf_t draw_buf_dsc;
    static lv_color_t buf_1[DISP_BUF_SIZE];

    lv_disp_draw_buf_init(&draw_buf_dsc, buf_1, NULL, DISP_BUF_SIZE);

    /*-----------------------------------
     * 在 LVGL 中注册显示设备
     *----------------------------------*/
    static lv_disp_drv_t disp_drv;

    lv_disp_drv_init(&disp_drv);

    /* 分辨率直接取 tftlcd 驱动初始化后的实际值
     * HX8357DN 竖屏(TFTLCD_DIR=0)为 320*480 */
    disp_drv.hor_res = tftlcd_data.width;
    disp_drv.ver_res = tftlcd_data.height;

    /* 把缓冲区内容搬到屏上的回调 */
    disp_drv.flush_cb = disp_flush;

    /* 绑定绘制缓冲区 */
    disp_drv.draw_buf = &draw_buf_dsc;

    /* 不做软件旋转，直接用屏的当前方向 */
    disp_drv.sw_rotate = 0;

    /* 最后注册驱动 */
    lv_disp_drv_register(&disp_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief 显示设备底层初始化
 * @note  LCD 的 GPIO/FSMC 初始化已在 main() 里由 TFTLCD_Init() 完成，
 *        这里不需要重复初始化，留空即可
 */
static void disp_init(void)
{
    /* 屏幕已在 TFTLCD_Init() 中初始化完毕 */
}

/**
 * @brief 把 LVGL 缓冲区的一段矩形区域刷到屏上
 * @param area    要刷新的区域（含两端坐标）
 * @param color_p 该区域的像素数据，RGB565，逐行排列
 */
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);
    uint32_t total = w * h;
    const uint16_t * p = (const uint16_t *)color_p;

    /* 整个区域只设一次窗口，后续像素在窗口内自动自增 */
    LCD_Set_Window(area->x1, area->y1, area->x2, area->y2);

    while(total--)
    {
        uint16_t c = *p++;
        /* HX8357DN：8 位总线，先写高字节再写低字节 */
        TFTLCD->LCD_DATA = c >> 8;
        TFTLCD->LCD_DATA = c & 0xFF;
    }

    /* 通知 LVGL 本次刷新完成 */
    lv_disp_flush_ready(disp_drv);
}
