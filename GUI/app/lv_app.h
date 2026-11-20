/**
 * @file lv_app.h
 * 监控界面（LVGL 8.3）：电机状态与ADC采集 / 故障记录 / 参数设置 三页
 */

#ifndef __LV_APP_H
#define __LV_APP_H

#include "task_gui.h"
#include <stdint.h>

/**
 * @brief 创建界面（必须先 lv_init() + lv_port_disp_init()）
 */
void lv_app_init(void);

/**
 * @brief 请求底部提示条显示/清除报警（任意任务可调用，仅置标志位）
 * @param on 1=显示报警，0=恢复普通提示
 */
void lv_app_request_alarm(uint8_t on);

/**
 * @brief 请求切到下一页（任意任务可调用，仅置标志位）
 */
void lv_app_page_next(void);

/**
 * @brief 记一条保护触发记录，供故障记录页显示
 * @param src 1=母线过压 2=M1过流 3=M2过流
 * @param v0/v1/v2 触发瞬间 CH0/CH1/CH2 的原始 ADC 值
 */
void lv_app_add_fault(uint8_t src, uint16_t v0, uint16_t v1, uint16_t v2);

/**
 * @brief 用一帧显示数据刷新界面
 * @param msg 来自 hDisplayQueue 的显示数据
 */
void lv_app_update(const DisplayMsg_t *msg);

#endif /* __LV_APP_H */
