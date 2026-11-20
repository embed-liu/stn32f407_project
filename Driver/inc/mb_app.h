#ifndef __MB_APP_H
#define __MB_APP_H

#include "mb.h"
#include "FreeRTOS.h"
#include "task.h"

//最近一次收到主站有效请求的 tick，界面据此判断在线/离线
extern volatile TickType_t mb_last_active;

// 回调声明
eMBErrorCode eMBRegHoldingCB(UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs, eMBRegisterMode eMode);
eMBErrorCode eMBRegCoilsCB(UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNCoils, eMBRegisterMode eMode);
eMBErrorCode eMBRegDiscreteCB(UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNDiscrete);
eMBErrorCode eMBRegInputCB(UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs);

#endif
