#include "task_motor.h"
#include "step_motor.h"
#include "stm32f4xx.h"
#include "wdt.h"

/**
 * @brief 电机控制任务，优先级25，栈1024
 * 接收消息队列指令，互斥锁保护电机资源，设置步数、方向、速度
 */
void Task_Motor(void *pvParameters)
{
	MotorCmd_t cmd;
	BaseType_t ret;
	while(1)
	{
		// 每次循环第一时间打卡
		WDT_TaskHeartbeat(WDT_TASK_MOTOR);

		// 队列接收：最多阻塞 50ms，超时自动退出，回到循环刷新心跳
		ret = xQueueReceive(hMotorQueue, &cmd, pdMS_TO_TICKS(50));
		if(ret == pdPASS)
		{
			//获取互斥锁，占用电机资源，最多等待50ms
			if(xSemaphoreTake(hMotorMutex, pdMS_TO_TICKS(50)) == pdTRUE)
			{
				if(cmd.step_cnt == 0)
				{
					//step=0代表急停
					Motor_EmergencyStop();
				}
				else
				{
					//更新TIM6速度
					TIM6_SetParam(83, cmd.speed_arr);
					//下发电机运动指令
					Motor_Set(cmd.motor_id, cmd.dir, cmd.step_cnt);
				}
				//释放互斥锁
				xSemaphoreGive(hMotorMutex);
			}
		}
		// 不需要前面那个 vTaskDelay 了，xQueueReceive 自带阻塞延时
	}
}
