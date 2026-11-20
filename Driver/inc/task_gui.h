#ifndef __TASK_GUI_H
#define __TASK_GUI_H
#include "FreeRTOS.h"
#include "queue.h"
#include "tftlcd.h"

//显示任务用的数据结构（原 task_display.h 里的定义，现在由 GUI 任务接收）
typedef struct
{
    uint16_t cur_th;    //电流阈值
    uint16_t vol_th;    //电压阈值
    uint16_t m1_remain;
    uint16_t m2_remain;
    float bus_vol;
    float m1_cur;
    float m2_cur;
    float ch3_vol;
    char tip_str[32];   //底部提示文字，如"Motor1 CCW Run"
    uint8_t sel;        //参数页选中的阈值项：0=电流阈值 1=电压阈值
    uint16_t m_speed;   //电机速度寄存器（TIM6 ARR）
} DisplayMsg_t;

extern QueueHandle_t hDisplayQueue;

void GUI_Init(void);
void Task_GUI(void *pvParameters);
#endif
