#include "task_param.h"
#include "24cxx.h"
#include "usart.h"
#include "string.h"
#include "wdt.h"
Param_t g_param;
uint8_t param_dirty_flag = 0;

#define PARAM_MAGIC 0xA5A5

//加载默认参数
void Param_SetDefault(Param_t *p)
{
    p->cur_th = 2000;
    p->vol_th = 2500;
    p->magic  = PARAM_MAGIC;
}

//保存参数到EEPROM
uint8_t Param_SaveToEeprom(Param_t *p)
{
    if(xSemaphoreTake(hParamMutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return 1; //拿不到锁失败
    }
    //先检测设备是否在线
    if(AT24CXX_Check())
    {
        xSemaphoreGive(hParamMutex);
        printf("[PARAM] EEPROM dev check fail!\r\n");
        return 2;
    }
    AT24CXX_Write(2, (uint8_t*)p, sizeof(Param_t));
    xSemaphoreGive(hParamMutex);
    printf("[PARAM] Save param ok!\r\n");
    return 0;
}

//从EEPROM读取参数
uint8_t Param_LoadFromEeprom(Param_t *p)
{
    if(xSemaphoreTake(hParamMutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return 1;
    }
    if(AT24CXX_Check())
    {
        xSemaphoreGive(hParamMutex);
        return 2; //器件检测失败
    }
    AT24CXX_Read(2, (uint8_t*)p, sizeof(Param_t));
    xSemaphoreGive(hParamMutex);

    if(p->magic != PARAM_MAGIC)
    {
        printf("[PARAM] EEPROM empty / magic error\r\n");
        return 3;
    }
    return 0;
}

void Task_Param(void *pvParameters)
{
    uint8_t ret;
    AT24CXX_Init();

    ret = Param_LoadFromEeprom(&g_param);
    printf("===== EEPROM Param Test Start =====\r\n");
    if(ret == 0)
    {
        printf("Read param ok! cur_th:%d, vol_th:%d\r\n",g_param.cur_th,g_param.vol_th);
    }
    else
    {
        printf("Load fail, load default param and save!\r\n");
        Param_SetDefault(&g_param);
        Param_SaveToEeprom(&g_param);
        printf("cur_th:%d, vol_th:%d\r\n",g_param.cur_th,g_param.vol_th);
    }
    printf("===== EEPROM Param Test End =====\r\n");

    while(1)
    {
        if(param_dirty_flag == 1)
        {
            uint8_t save_ret = Param_SaveToEeprom(&g_param);
            if(save_ret == 0)
            {
                param_dirty_flag = 0;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
				WDT_TaskHeartbeat(WDT_TASK_PARAM);
				vTaskDelay(pdMS_TO_TICKS(200));
    }

}
