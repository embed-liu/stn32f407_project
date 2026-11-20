/**
 * @file lv_app.c
 * 监控界面（LVGL 8.3）：三个页面，KEY0 循环切页
 *
 *   页1 电机状态与 ADC 采集：M1/M2 剩余步数、速度 + 4 路模拟量 + 电流趋势
 *   页2 故障记录：最近 6 条保护触发记录（无记录时显示"暂无故障记录"）
 *   页3 参数设置：电流/电压阈值（按键改，也可 Modbus 写）+ 操作说明
 *
 * 顶部标题栏（含 Modbus 在线/离线状态与页码）和底部提示条三页共用。
 */

#include "lv_app.h"
#include "lvgl.h"
#include "mb_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(font_cn_16)
LV_FONT_DECLARE(font_cn_24)

#define PAGE_NUM        3
#define FAULT_MAX       6
#define MB_TIMEOUT_MS   3000

/* 配色：深色工业风 */
#define COLOR_SCR        lv_color_hex(0x0B1319)
#define COLOR_HEAD       lv_color_hex(0x10303C)
#define COLOR_CARD       lv_color_hex(0x16262F)
#define COLOR_CARD_BD    lv_color_hex(0x25454F)
#define COLOR_TEXT       lv_color_hex(0xE8F4F7)
#define COLOR_DIM        lv_color_hex(0x8AA3AE)
#define COLOR_ACCENT     lv_color_hex(0x00D0FF)
#define COLOR_OK         lv_color_hex(0x2FE39B)
#define COLOR_WARN       lv_color_hex(0xFFB020)
#define COLOR_ERR        lv_color_hex(0xFF5252)

/* 条形指示 / 趋势图量程（0.01 为单位） */
#define BAR_BUS_MAX      500
#define BAR_CUR_MAX      2000
#define BAR_CH3_MAX      500
#define TREND_POINTS     60

/* 故障记录：写侧在 Task_Protect，读侧在 Task_GUI，环形缓冲 + 临界区 */
typedef struct
{
    uint32_t sec;       /* 上电后秒数 */
    uint16_t v0;        /* 触发时 CH0 原始值 */
    uint16_t v1;        /* 触发时 CH1 原始值 */
    uint16_t v2;        /* 触发时 CH2 原始值 */
    uint8_t  src;       /* 1=母线过压 2=M1过流 3=M2过流 */
} fault_rec_t;

static fault_rec_t fault_log[FAULT_MAX];
static volatile uint8_t  fault_head = 0;
static volatile uint8_t  fault_cnt  = 0;
static volatile uint16_t fault_seq  = 0;

/* 页面与控件 */
static lv_obj_t * page_obj[PAGE_NUM];
static lv_obj_t * lbl_status;
static lv_obj_t * lbl_pageno;
static lv_obj_t * lbl_m1;
static lv_obj_t * lbl_m2;
static lv_obj_t * lbl_speed;
static lv_obj_t * lbl_bus;
static lv_obj_t * bar_bus;
static lv_obj_t * lbl_i1;
static lv_obj_t * bar_i1;
static lv_obj_t * lbl_i2;
static lv_obj_t * bar_i2;
static lv_obj_t * lbl_ch3;
static lv_obj_t * bar_ch3;
static lv_obj_t * chart;
static lv_chart_series_t * ser_i1;
static lv_chart_series_t * ser_i2;
static lv_obj_t * lbl_th_name_cur;
static lv_obj_t * lbl_th_name_vol;
static lv_obj_t * lbl_th_cur;
static lv_obj_t * lbl_th_vol;
static lv_obj_t * fault_rows[FAULT_MAX];
static lv_obj_t * lbl_fault_empty;
static lv_obj_t * lbl_tip;

static volatile uint8_t alarm_req = 0;   /* 报警请求：Task_Protect 置位 */
static volatile uint8_t page_req  = 0;   /* 切页请求：Task_MainApp 置位 */
static uint8_t cur_page  = 0;
static uint8_t trend_div = 0;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_obj_t * card_create(lv_obj_t * parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h);
static lv_obj_t * label_create(lv_obj_t * parent, const lv_font_t * font, lv_color_t color);
static lv_obj_t * bar_create(lv_obj_t * parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, int32_t max);
static void       app_header_create(lv_obj_t * parent);
static void       app_tip_bar_create(lv_obj_t * parent);
static void       page1_create(lv_obj_t * parent);
static void       page2_create(lv_obj_t * parent);
static void       page3_create(lv_obj_t * parent);
static const char * fault_type_str(uint8_t src);
static void       page_apply(void);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 创建整个界面（必须先 lv_init() + lv_port_disp_init()）
 */
void lv_app_init(void)
{
    lv_obj_t * scr = lv_scr_act();
    uint8_t i;

    lv_obj_set_style_bg_color(scr, COLOR_SCR, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    app_header_create(scr);

    /* 三个页面各自一个透明容器，只显示当前页 */
    for(i = 0; i < PAGE_NUM; i++)
    {
        page_obj[i] = lv_obj_create(scr);
        lv_obj_set_pos(page_obj[i], 0, 54);
        lv_obj_set_size(page_obj[i], 320, 386);
        lv_obj_set_style_bg_opa(page_obj[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(page_obj[i], 0, 0);
        lv_obj_set_style_pad_all(page_obj[i], 0, 0);
        lv_obj_clear_flag(page_obj[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    page1_create(page_obj[0]);
    page2_create(page_obj[1]);
    page3_create(page_obj[2]);

    app_tip_bar_create(scr);

    page_apply();
}

/**
 * @brief 请求底部提示条显示/清除报警（任意任务可调，只写标志位）
 */
void lv_app_request_alarm(uint8_t on)
{
    alarm_req = on ? 1 : 0;
}

/**
 * @brief 请求切到下一页（任意任务可调，只写标志位）
 */
void lv_app_page_next(void)
{
    page_req = 1;
}

/**
 * @brief 记一条保护触发记录（Task_Protect 调用）
 * @param src 1=母线过压 2=M1过流 3=M2过流
 * @param v0/v1/v2 触发瞬间 CH0/CH1/CH2 的原始 ADC 值
 */
void lv_app_add_fault(uint8_t src, uint16_t v0, uint16_t v1, uint16_t v2)
{
    taskENTER_CRITICAL();
    fault_log[fault_head].sec = (uint32_t)(xTaskGetTickCount() / 1000u);
    fault_log[fault_head].v0  = v0;
    fault_log[fault_head].v1  = v1;
    fault_log[fault_head].v2  = v2;
    fault_log[fault_head].src = src;
    fault_head = (uint8_t)((fault_head + 1u) % FAULT_MAX);
    if(fault_cnt < FAULT_MAX)
    {
        fault_cnt++;
    }
    fault_seq++;
    taskEXIT_CRITICAL();
}

/**
 * @brief 用一帧显示数据刷新界面
 */
void lv_app_update(const DisplayMsg_t * msg)
{
    char buf[40];
    float cur_th_a;
    float vol_th_v;
    int32_t bus_01v;
    int32_t i1_01a;
    int32_t i2_01a;
    int32_t ch3_01v;
    TickType_t now;
    uint8_t online;
    uint8_t i;
    uint8_t idx;
    uint16_t seq;
    uint32_t sec;
    fault_rec_t rec;

    if(msg == NULL) return;

    /* ---- 页切换请求 ---- */
    if(page_req)
    {
        page_req = 0;
        cur_page = (uint8_t)((cur_page + 1u) % PAGE_NUM);
        page_apply();
    }

    /* ---- 页1：电机 ---- */
    snprintf(buf, sizeof(buf), "%4u", (unsigned int)msg->m1_remain);
    lv_label_set_text(lbl_m1, buf);
    snprintf(buf, sizeof(buf), "%4u", (unsigned int)msg->m2_remain);
    lv_label_set_text(lbl_m2, buf);
    snprintf(buf, sizeof(buf), "ARR %4u", (unsigned int)msg->m_speed);
    lv_label_set_text(lbl_speed, buf);

    /* ---- 页1：模拟量 ---- */
    bus_01v = (int32_t)(msg->bus_vol * 100.0f);
    if(bus_01v < 0) bus_01v = 0;
    if(bus_01v > BAR_BUS_MAX) bus_01v = BAR_BUS_MAX;
    snprintf(buf, sizeof(buf), "%.2f V", (double)msg->bus_vol);
    lv_label_set_text(lbl_bus, buf);
    lv_bar_set_value(bar_bus, bus_01v, LV_ANIM_OFF);

    i1_01a = (int32_t)(msg->m1_cur * 100.0f);
    if(i1_01a < 0) i1_01a = 0;
    if(i1_01a > BAR_CUR_MAX) i1_01a = BAR_CUR_MAX;
    snprintf(buf, sizeof(buf), "%.2f A", (double)msg->m1_cur);
    lv_label_set_text(lbl_i1, buf);
    lv_bar_set_value(bar_i1, i1_01a, LV_ANIM_OFF);

    i2_01a = (int32_t)(msg->m2_cur * 100.0f);
    if(i2_01a < 0) i2_01a = 0;
    if(i2_01a > BAR_CUR_MAX) i2_01a = BAR_CUR_MAX;
    snprintf(buf, sizeof(buf), "%.2f A", (double)msg->m2_cur);
    lv_label_set_text(lbl_i2, buf);
    lv_bar_set_value(bar_i2, i2_01a, LV_ANIM_OFF);

    ch3_01v = (int32_t)(msg->ch3_vol * 100.0f);
    if(ch3_01v < 0) ch3_01v = 0;
    if(ch3_01v > BAR_CH3_MAX) ch3_01v = BAR_CH3_MAX;
    snprintf(buf, sizeof(buf), "%.2f V", (double)msg->ch3_vol);
    lv_label_set_text(lbl_ch3, buf);
    lv_bar_set_value(bar_ch3, ch3_01v, LV_ANIM_OFF);

    /* ---- 页1：超阈值染色 ---- */
    lv_obj_set_style_text_color(lbl_i1, (i1_01a >= (int32_t)msg->cur_th) ? COLOR_ERR : COLOR_TEXT, 0);
    lv_obj_set_style_text_color(lbl_i2, (i2_01a >= (int32_t)msg->cur_th) ? COLOR_ERR : COLOR_TEXT, 0);
    lv_obj_set_style_text_color(lbl_bus, (bus_01v >= (int32_t)msg->vol_th) ? COLOR_WARN : COLOR_TEXT, 0);

    /* ---- 页1：电流趋势，1 秒一个点（100ms 一帧，每 10 帧推一次） ---- */
    trend_div++;
    if(trend_div >= 10)
    {
        trend_div = 0;
        if(cur_page == 0)
        {
            lv_chart_set_next_value(chart, ser_i1, (lv_coord_t)i1_01a);
            lv_chart_set_next_value(chart, ser_i2, (lv_coord_t)i2_01a);
        }
    }

    /* ---- 页3：阈值 ----
       电流阈值 raw -> (raw*3.3/4095)/0.1 A；电压阈值 raw -> raw*3.3/4095 V */
    cur_th_a = (msg->cur_th * 3.3f / 4095.0f) / 0.1f;
    snprintf(buf, sizeof(buf), "%4u / %.1fA", (unsigned int)msg->cur_th, (double)cur_th_a);
    lv_label_set_text(lbl_th_cur, buf);

    vol_th_v = msg->vol_th * 3.3f / 4095.0f;
    snprintf(buf, sizeof(buf), "%4u / %.2fV", (unsigned int)msg->vol_th, (double)vol_th_v);
    lv_label_set_text(lbl_th_vol, buf);

    /* 选中项高亮：前缀 "> " 表示当前按键调节的是这一行 */
    lv_label_set_text(lbl_th_name_cur, msg->sel ? "\xE7\x94\xB5\xE6\xB5\x81\xE9\x98\x88\xE5\x80\xBC" : "> \xE7\x94\xB5\xE6\xB5\x81\xE9\x98\x88\xE5\x80\xBC");
    lv_obj_set_style_text_color(lbl_th_name_cur, msg->sel ? COLOR_DIM : COLOR_ACCENT, 0);
    lv_label_set_text(lbl_th_name_vol, msg->sel ? "> \xE7\x94\xB5\xE5\x8E\x8B\xE9\x98\x88\xE5\x80\xBC" : "\xE7\x94\xB5\xE5\x8E\x8B\xE9\x98\x88\xE5\x80\xBC");
    lv_obj_set_style_text_color(lbl_th_name_vol, msg->sel ? COLOR_ACCENT : COLOR_DIM, 0);

    /* ---- 页2：故障记录，最新的在最上面 ---- */
    for(i = 0; i < FAULT_MAX; i++)
    {
        taskENTER_CRITICAL();
        idx = (uint8_t)((fault_head + FAULT_MAX - 1u - i) % FAULT_MAX);
        rec = fault_log[idx];
        seq = fault_seq;
        taskEXIT_CRITICAL();

        if(i < fault_cnt)
        {
            sec = rec.sec;
            seq = (uint16_t)(seq - i);
            switch(rec.src)
            {
                case 1:
                    snprintf(buf, sizeof(buf), "%02u  %02u:%02u  %s  %.2fV",
                             (unsigned int)(seq % 100u),
                             (unsigned int)(sec / 60u), (unsigned int)(sec % 60u),
                             fault_type_str(rec.src), (double)(rec.v0 * 3.3f / 4095.0f));
                    break;
                case 2:
                    snprintf(buf, sizeof(buf), "%02u  %02u:%02u  %s  %.2fA",
                             (unsigned int)(seq % 100u),
                             (unsigned int)(sec / 60u), (unsigned int)(sec % 60u),
                             fault_type_str(rec.src), (double)(rec.v1 * 3.3f / 4095.0f / 0.1f));
                    break;
                default:
                    snprintf(buf, sizeof(buf), "%02u  %02u:%02u  %s  %.2fA",
                             (unsigned int)(seq % 100u),
                             (unsigned int)(sec / 60u), (unsigned int)(sec % 60u),
                             fault_type_str(rec.src), (double)(rec.v2 * 3.3f / 4095.0f / 0.1f));
                    break;
            }
            lv_label_set_text(fault_rows[i], buf);
            lv_obj_set_style_text_color(fault_rows[i], (i == 0) ? COLOR_ERR : COLOR_TEXT, 0);
        }
        else
        {
            lv_label_set_text(fault_rows[i], "");
        }
    }
    if(fault_cnt == 0)
        lv_obj_clear_flag(lbl_fault_empty, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(lbl_fault_empty, LV_OBJ_FLAG_HIDDEN);

    /* ---- 顶部：Modbus 通信状态 + 页码 ---- */
    now = xTaskGetTickCount();
    online = ((now - mb_last_active) <= pdMS_TO_TICKS(MB_TIMEOUT_MS)) ? 1 : 0;
    lv_obj_set_style_text_color(lbl_status, online ? COLOR_OK : COLOR_DIM, 0);
    lv_label_set_text(lbl_status, online ? "\xE5\x9C\xA8\xE7\xBA\xBF" : "\xE7\xA6\xBB\xE7\xBA\xBF");

    snprintf(buf, sizeof(buf), "%u/%u", (unsigned int)(cur_page + 1u), (unsigned int)PAGE_NUM);
    lv_label_set_text(lbl_pageno, buf);

    /* ---- 底部提示 / 报警 ---- */
    if(alarm_req)
    {
        lv_label_set_text(lbl_tip, "!!!PROTECT TRIGGER!!!");
        lv_obj_set_style_text_color(lbl_tip, COLOR_ERR, 0);
    }
    else
    {
        lv_label_set_text(lbl_tip, msg->tip_str);
        lv_obj_set_style_text_color(lbl_tip, COLOR_DIM, 0);
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* 卡片底框 */
static lv_obj_t * card_create(lv_obj_t * parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t * obj = lv_obj_create(parent);

    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, COLOR_CARD_BD, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    return obj;
}

/* 统一字体/颜色的标签 */
static lv_obj_t * label_create(lv_obj_t * parent, const lv_font_t * font, lv_color_t color)
{
    lv_obj_t * lbl = lv_label_create(parent);

    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_text_opa(lbl, LV_OPA_COVER, 0);

    return lbl;
}

/* 横向条形指示 */
static lv_obj_t * bar_create(lv_obj_t * parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, int32_t max)
{
    lv_obj_t * bar = lv_bar_create(parent);

    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, 6);
    lv_bar_set_range(bar, 0, max);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x0C1A20), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);

    return bar;
}

/* 顶部标题栏：标题 + 在线/离线 + 页码 */
static void app_header_create(lv_obj_t * parent)
{
    lv_obj_t * head = lv_obj_create(parent);

    lv_obj_set_pos(head, 0, 0);
    lv_obj_set_size(head, 320, 50);
    lv_obj_set_style_bg_color(head, COLOR_HEAD, 0);
    lv_obj_set_style_bg_opa(head, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(head, 0, 0);
    lv_obj_set_style_border_width(head, 0, 0);
    lv_obj_set_style_pad_all(head, 0, 0);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * title = label_create(head, &font_cn_24, COLOR_TEXT);
    lv_label_set_text(title, "\xE8\xBE\xB9\xE7\xBC\x98\xE7\xBD\x91\xE5\x85\xB3\xE7\x9B\x91\xE6\x8E\xA7");
    lv_obj_set_pos(title, 10, 11);

    lbl_status = label_create(head, &font_cn_16, COLOR_DIM);
    lv_label_set_text(lbl_status, "\xE7\xA6\xBB\xE7\xBA\xBF");
    lv_obj_align(lbl_status, LV_ALIGN_TOP_RIGHT, -10, 6);

    lbl_pageno = label_create(head, &lv_font_montserrat_16, COLOR_DIM);
    lv_label_set_text(lbl_pageno, "1/3");
    lv_obj_align(lbl_pageno, LV_ALIGN_TOP_RIGHT, -10, 28);
}

/* 底部提示条（三页共用） */
static void app_tip_bar_create(lv_obj_t * parent)
{
    lv_obj_t * bar = lv_obj_create(parent);

    lv_obj_set_pos(bar, 8, 446);
    lv_obj_set_size(bar, 304, 26);
    lv_obj_set_style_bg_color(bar, COLOR_HEAD, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 6, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lbl_tip = label_create(bar, &font_cn_16, COLOR_DIM);
    lv_label_set_text(lbl_tip, "Modbus Only");
    lv_obj_set_pos(lbl_tip, 8, 4);
}

/* 页1：电机状态与 ADC 采集 */
static void page1_create(lv_obj_t * parent)
{
    lv_obj_t * card;
    lv_obj_t * title;

    /* 电机状态卡片 */
    card  = card_create(parent, 8, 4, 304, 104);
    title = label_create(card, &font_cn_16, COLOR_ACCENT);
    lv_label_set_text(title, "\xE7\x94\xB5\xE6\x9C\xBA\xE7\x8A\xB6\xE6\x80\x81");
    lv_obj_set_pos(title, 10, 6);

    lbl_speed = label_create(card, &lv_font_montserrat_14, COLOR_DIM);
    lv_label_set_text(lbl_speed, "ARR    0");
    lv_obj_align(lbl_speed, LV_ALIGN_TOP_RIGHT, -10, 10);

    lv_obj_t * n1 = label_create(card, &font_cn_16, COLOR_DIM);
    lv_label_set_text(n1, "M1 \xE5\x89\xA9\xE4\xBD\x99\xE6\xAD\xA5\xE6\x95\xB0");
    lv_obj_set_pos(n1, 10, 36);

    lbl_m1 = label_create(card, &lv_font_montserrat_24, COLOR_TEXT);
    lv_label_set_text(lbl_m1, "0000");
    lv_obj_align(lbl_m1, LV_ALIGN_TOP_RIGHT, -10, 32);

    lv_obj_t * n2 = label_create(card, &font_cn_16, COLOR_DIM);
    lv_label_set_text(n2, "M2 \xE5\x89\xA9\xE4\xBD\x99\xE6\xAD\xA5\xE6\x95\xB0");
    lv_obj_set_pos(n2, 10, 72);

    lbl_m2 = label_create(card, &lv_font_montserrat_24, COLOR_TEXT);
    lv_label_set_text(lbl_m2, "0000");
    lv_obj_align(lbl_m2, LV_ALIGN_TOP_RIGHT, -10, 68);

    /* 模拟量卡片 */
    card  = card_create(parent, 8, 116, 304, 180);
    title = label_create(card, &font_cn_16, COLOR_ACCENT);
    lv_label_set_text(title, "\xE6\xA8\xA1\xE6\x8B\x9F\xE9\x87\x8F\xE9\x87\x87\xE9\x9B\x86");
    lv_obj_set_pos(title, 10, 6);

    lv_obj_t * n_bus = label_create(card, &font_cn_16, COLOR_DIM);
    lv_label_set_text(n_bus, "\xE6\xAF\x8D\xE7\xBA\xBF\xE7\x94\xB5\xE5\x8E\x8B");
    lv_obj_set_pos(n_bus, 10, 32);

    lbl_bus = label_create(card, &lv_font_montserrat_16, COLOR_TEXT);
    lv_label_set_text(lbl_bus, "0.00 V");
    lv_obj_align(lbl_bus, LV_ALIGN_TOP_RIGHT, -10, 32);

    bar_bus = bar_create(card, 10, 54, 284, BAR_BUS_MAX);

    lv_obj_t * n_i1 = label_create(card, &font_cn_16, COLOR_DIM);
    lv_label_set_text(n_i1, "M1 \xE7\x94\xB5\xE6\xB5\x81");
    lv_obj_set_pos(n_i1, 10, 68);

    lbl_i1 = label_create(card, &lv_font_montserrat_16, COLOR_TEXT);
    lv_label_set_text(lbl_i1, "0.00 A");
    lv_obj_align(lbl_i1, LV_ALIGN_TOP_RIGHT, -10, 68);

    bar_i1 = bar_create(card, 10, 90, 284, BAR_CUR_MAX);

    lv_obj_t * n_i2 = label_create(card, &font_cn_16, COLOR_DIM);
    lv_label_set_text(n_i2, "M2 \xE7\x94\xB5\xE6\xB5\x81");
    lv_obj_set_pos(n_i2, 10, 104);

    lbl_i2 = label_create(card, &lv_font_montserrat_16, COLOR_TEXT);
    lv_label_set_text(lbl_i2, "0.00 A");
    lv_obj_align(lbl_i2, LV_ALIGN_TOP_RIGHT, -10, 104);

    bar_i2 = bar_create(card, 10, 126, 284, BAR_CUR_MAX);

    lv_obj_t * n_ch3 = label_create(card, &font_cn_16, COLOR_DIM);
    lv_label_set_text(n_ch3, "\xE9\x80\x9A\xE9\x81\x93" "3 \xE7\x94\xB5\xE5\x8E\x8B");
    lv_obj_set_pos(n_ch3, 10, 140);

    lbl_ch3 = label_create(card, &lv_font_montserrat_16, COLOR_TEXT);
    lv_label_set_text(lbl_ch3, "0.00 V");
    lv_obj_align(lbl_ch3, LV_ALIGN_TOP_RIGHT, -10, 140);

    bar_ch3 = bar_create(card, 10, 162, 284, BAR_CH3_MAX);

    /* 电流趋势卡片 */
    card  = card_create(parent, 8, 304, 304, 78);
    title = label_create(card, &font_cn_16, COLOR_ACCENT);
    lv_label_set_text(title, "\xE7\x94\xB5\xE6\xB5\x81\xE8\xB6\x8B\xE5\x8A\xBF");
    lv_obj_set_pos(title, 10, 4);

    chart = lv_chart_create(card);
    lv_obj_set_pos(chart, 8, 26);
    lv_obj_set_size(chart, 288, 44);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, TREND_POINTS);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, BAR_CUR_MAX);
    lv_chart_set_div_line_count(chart, 3, 4);
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x0C1A20), 0);
    lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chart, 0, 0);
    lv_obj_set_style_radius(chart, 4, 0);
    lv_obj_set_style_pad_all(chart, 0, 0);
    ser_i1 = lv_chart_add_series(chart, COLOR_ACCENT, LV_CHART_AXIS_PRIMARY_Y);
    ser_i2 = lv_chart_add_series(chart, COLOR_WARN,   LV_CHART_AXIS_PRIMARY_Y);
}

/* 页2：故障记录 */
static void page2_create(lv_obj_t * parent)
{
    lv_obj_t * card = card_create(parent, 8, 4, 304, 378);
    lv_obj_t * title = label_create(card, &font_cn_16, COLOR_ACCENT);

    lv_label_set_text(title, "\xE6\x95\x85\xE9\x9A\x9C\xE8\xAE\xB0\xE5\xBD\x95");
    lv_obj_set_pos(title, 10, 6);

    lv_obj_t * hint = label_create(card, &lv_font_montserrat_14, COLOR_DIM);
    lv_label_set_text(hint, "#  mm:ss  type  value");
    lv_obj_set_pos(hint, 10, 30);

    uint8_t i;
    for(i = 0; i < FAULT_MAX; i++)
    {
        fault_rows[i] = label_create(card, &font_cn_16, COLOR_TEXT);
        lv_label_set_text(fault_rows[i], "");
        lv_obj_set_pos(fault_rows[i], 10, 56 + (lv_coord_t)i * 52);
    }

    lbl_fault_empty = label_create(card, &font_cn_16, COLOR_DIM);
    lv_label_set_text(lbl_fault_empty, "\xE6\x9A\x82\xE6\x97\xA0\xE6\x95\x85\xE9\x9A\x9C\xE8\xAE\xB0\xE5\xBD\x95");
    lv_obj_align(lbl_fault_empty, LV_ALIGN_CENTER, 0, -20);
}

/* 页3：参数设置 */
static void page3_create(lv_obj_t * parent)
{
    lv_obj_t * card;
    lv_obj_t * title;
    lv_obj_t * lbl;

    /* 阈值卡片 */
    card  = card_create(parent, 8, 4, 304, 150);
    title = label_create(card, &font_cn_16, COLOR_ACCENT);
    lv_label_set_text(title, "\xE5\x8F\x82\xE6\x95\xB0\xE8\xAE\xBE\xE7\xBD\xAE");
    lv_obj_set_pos(title, 10, 6);

    lbl_th_name_cur = label_create(card, &font_cn_16, COLOR_ACCENT);
    lv_label_set_text(lbl_th_name_cur, "> \xE7\x94\xB5\xE6\xB5\x81\xE9\x98\x88\xE5\x80\xBC");
    lv_obj_set_pos(lbl_th_name_cur, 10, 44);

    lbl_th_cur = label_create(card, &lv_font_montserrat_16, COLOR_TEXT);
    lv_label_set_text(lbl_th_cur, "0 / 0.0A");
    lv_obj_align(lbl_th_cur, LV_ALIGN_TOP_RIGHT, -10, 40);

    lbl_th_name_vol = label_create(card, &font_cn_16, COLOR_DIM);
    lv_label_set_text(lbl_th_name_vol, "\xE7\x94\xB5\xE5\x8E\x8B\xE9\x98\x88\xE5\x80\xBC");
    lv_obj_set_pos(lbl_th_name_vol, 10, 100);

    lbl_th_vol = label_create(card, &lv_font_montserrat_16, COLOR_TEXT);
    lv_label_set_text(lbl_th_vol, "0 / 0.00V");
    lv_obj_align(lbl_th_vol, LV_ALIGN_TOP_RIGHT, -10, 96);

    lbl = label_create(card, &font_cn_16, COLOR_DIM);
    lv_label_set_text(lbl, "Modbus: 6=\xE7\x94\xB5\xE6\xB5\x81\xE9\x98\x88\xE5\x80\xBC 7=\xE7\x94\xB5\xE5\x8E\x8B\xE9\x98\x88\xE5\x80\xBC");
    lv_obj_set_pos(lbl, 10, 128);

    /* 操作说明卡片 */
    card  = card_create(parent, 8, 162, 304, 170);
    title = label_create(card, &font_cn_16, COLOR_ACCENT);
    lv_label_set_text(title, "\xE6\x93\x8D\xE4\xBD\x9C\xE8\xAF\xB4\xE6\x98\x8E");
    lv_obj_set_pos(title, 10, 6);

    const char * tips[4] = { "KEY0   \xE5\x88\x87\xE9\xA1\xB5", "KEY2   \xE9\x80\x89\xE6\x8B\xA9", "KEY1   \xE5\x87\x8F" "50", "KEY_UP \xE5\x8A\xA0" "50" };
    uint8_t i;
    for(i = 0; i < 4; i++)
    {
        lv_obj_t * t = label_create(card, &font_cn_16, COLOR_TEXT);
        lv_label_set_text(t, tips[i]);
        lv_obj_set_pos(t, 10, 42 + (lv_coord_t)i * 30);
    }
}

/* 故障类型文字 */
static const char * fault_type_str(uint8_t src)
{
    switch(src)
    {
        case 1:  return "\xE6\xAF\x8D\xE7\xBA\xBF\xE8\xBF\x87\xE5\x8E\x8B";
        case 2:  return "M1\xE8\xBF\x87\xE6\xB5\x81";
        default: return "M2\xE8\xBF\x87\xE6\xB5\x81";
    }
}

/* 按当前页号显示/隐藏页面 */
static void page_apply(void)
{
    uint8_t i;

    for(i = 0; i < PAGE_NUM; i++)
    {
        if(i == cur_page)
        {
            lv_obj_clear_flag(page_obj[i], LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(page_obj[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}
