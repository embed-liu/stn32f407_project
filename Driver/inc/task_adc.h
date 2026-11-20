#ifndef __TASK_ADC_H
#define __TASK_ADC_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "task_param.h"
extern SemaphoreHandle_t hParamMutex;
extern Param_t g_sys_param;

//ADC数据包结构体
typedef struct
{
	uint16_t adc_raw[4];  //4路ADC原始采样值
	uint8_t limit_sta;    //限位预留
}AdcData_t;

//任务声明
void Task_ADC(void *pvParameters);

//外部全局
extern QueueHandle_t hAdcQueue;
extern SemaphoreHandle_t hProtectSem;
extern uint16_t adc_filter[4];

#endif
