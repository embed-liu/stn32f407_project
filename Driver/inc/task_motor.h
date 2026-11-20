#ifndef __TASK_MOTOR_H
#define __TASK_MOTOR_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

//电机指令结构体
typedef struct
{
	uint8_t motor_id;    // 1/2
	uint8_t dir;         // 0:CCW,1:CW
	uint16_t step_cnt;   //总步数
	uint16_t speed_arr;  //TIM6 ARR值，控制速度
}MotorCmd_t;

//任务函数声明
void Task_Motor(void *pvParameters);

//外部引用句柄（句柄定义在main.c）
extern QueueHandle_t hMotorQueue;
extern SemaphoreHandle_t hMotorMutex;

#endif
