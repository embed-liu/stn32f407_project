#ifndef __WDT_H
#define __WDT_H
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "stm32f4xx_iwdg.h"

// 在这里添加你所有需要监控的任务 ID
typedef enum
{
    WDT_TASK_ADC = 0,
    WDT_TASK_MOTOR,
    WDT_TASK_DISPLAY,
    WDT_TASK_PARAM,
    WDT_TASK_PROTECT,
    WDT_TASK_MODBUS,
    WDT_TASK_MAINAPP,
    WDT_TASK_MAX
} WdtTaskID_t;

extern SemaphoreHandle_t hWdtSem;

void IWDG_Init(uint8_t prer, uint16_t rlr);
void IWDG_Feed(void);
void Task_WDT(void *pvParameters);
void WDT_TaskHeartbeat(WdtTaskID_t task_id);

#endif
