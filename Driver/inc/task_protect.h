#ifndef __TASK_PROTECT_H
#define __TASK_PROTECT_H

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

void Task_Protect(void *pvParameters);
extern SemaphoreHandle_t hProtectSem;

#endif
