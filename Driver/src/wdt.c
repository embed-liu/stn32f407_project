#include "wdt.h"
#include "usart.h"

static const char * const task_name[] =
{
    "Task_ADC",
    "Task_Motor",
    "Task_GUI",
    "Task_Param",
    "Task_Protect",
    "Task_Modbus",
    "Task_MainApp"
};

static volatile TickType_t g_heartbeat[WDT_TASK_MAX] = {0};
// 心跳超时阈值：2000ms，任务必须 2s 内刷新一次心跳
#define WDT_HEARTBEAT_TIMEOUT    pdMS_TO_TICKS(2000)

SemaphoreHandle_t hWdtSem = NULL;

void IWDG_Init(uint8_t prer, uint16_t rlr)
{
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(prer);
    IWDG_SetReload(rlr);
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Disable);
    IWDG_Enable();
}

void IWDG_Feed(void)
{
    IWDG_ReloadCounter();
}

void WDT_TaskHeartbeat(WdtTaskID_t task_id)
{
    if(task_id >= WDT_TASK_MAX) return;
    taskENTER_CRITICAL();
    g_heartbeat[task_id] = xTaskGetTickCount();
    taskEXIT_CRITICAL();

    if(hWdtSem != NULL)
    {
        xSemaphoreGive(hWdtSem);
    }
}

void Task_WDT(void *pvParameters)
{
    BaseType_t sem_ret;
    TickType_t now_tick;
    uint8_t i;
    uint8_t fault_flag;

    // IWDG：预分频 64，重载 1000，溢出约 2s
    IWDG_Init(IWDG_Prescaler_64, 1000);

    while(1)
    {
        // 等待心跳通知，最长等待 2000ms
        sem_ret = xSemaphoreTake(hWdtSem, WDT_HEARTBEAT_TIMEOUT);

        now_tick = xTaskGetTickCount();
        fault_flag = 0;

        for(i = 0; i < WDT_TASK_MAX; i++)
        {
            if((now_tick - g_heartbeat[i]) >= WDT_HEARTBEAT_TIMEOUT)
            {
                printf("[WDT ERROR] Task timeout: %s\r\n", task_name[i]);
                fault_flag = 1;
                break;
            }
        }

        if(fault_flag == 1)
        {
            printf("[WDT] System reset!\r\n");
            NVIC_SystemReset();
        }
        else
        {
            // 全部任务心跳正常，喂硬件看门狗
            IWDG_Feed();
        }
    }
}
