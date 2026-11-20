#include "task_protect.h"
#include "step_motor.h"
#include "beep.h"
#include "tftlcd.h"
#include "stm32f4xx.h"
#include "wdt.h"
#include "lv_app.h"
#include "adc.h"
#include "task_param.h"

/**
 * @brief 保护任务：优先级30（最高优先级），收到信号量，电机急停+蜂鸣报警
 */
void Task_Protect(void *pvParameters)
{
	while(1)
	{
		// 【第一行】优先打卡心跳，保证周期性刷新
		WDT_TaskHeartbeat(WDT_TASK_PROTECT);

		// 信号量等待，最多 50ms，超时就退出，回到循环重新打卡
		if(xSemaphoreTake(hProtectSem, pdMS_TO_TICKS(50)) == pdTRUE)
		{
			uint16_t v0 = adc_filter[0];
			uint16_t v1 = adc_filter[1];
			uint16_t v2 = adc_filter[2];
			uint8_t  src;

			Motor_EmergencyStop();  //双电机急停
			BEEP = 1;               //蜂鸣器打开

			//记一条故障记录，供界面的故障记录页显示（先判母线过压，再判 M1/M2 过流）
			if(v0 >= g_param.vol_th)      src = 1;
			else if(v1 >= g_param.cur_th) src = 2;
			else                          src = 3;
			lv_app_add_fault(src, v0, v1, v2);

			lv_app_request_alarm(1);   //界面由 LVGL 绘制：在底部提示条上显示报警
			vTaskDelay(pdMS_TO_TICKS(1000));
			BEEP = 0;
			lv_app_request_alarm(0);   //清除报警提示
			//电机就绪提示已由 LVGL 界面显示，这里不再裸写文字
		}
	}
}
