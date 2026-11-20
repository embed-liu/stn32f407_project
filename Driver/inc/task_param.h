#ifndef __TASK_PARAM_H
#define __TASK_PARAM_H
#include "FreeRTOS.h"
#include "semphr.h"

typedef struct
{
    uint16_t cur_th;   //电流保护阈值
    uint16_t vol_th;   //电压保护阈值
    uint16_t magic;    //增加魔术字校验，避免乱数据
}Param_t;

extern Param_t g_param;
extern SemaphoreHandle_t hParamMutex;
extern uint8_t param_dirty_flag;  // 1=需要存到eeprom

void Param_SetDefault(Param_t *p);
uint8_t Param_SaveToEeprom(Param_t *p);
uint8_t Param_LoadFromEeprom(Param_t *p);
void Task_Param(void *pvParameters);

#endif
