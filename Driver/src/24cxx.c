#include "24cxx.h"
#include "SysTick.h"
#include "FreeRTOS.h"
#include "task.h"

static void AT24C_delay_us(uint32_t nus)
{
    uint32_t i,j;
    for(i = nus; i > 0; i--)
        for(j = 16; j > 0; j--);
}

void AT24CXX_Init(void)
{
	IIC_Init();
}

void AT24CXX_WriteOneByte(u16 WriteAddr, u8 DataToWrite)
{
    taskENTER_CRITICAL();
    IIC_Start();
    IIC_Send_Byte(0XA0);
    IIC_Wait_Ack();
    IIC_Send_Byte(WriteAddr);
    IIC_Wait_Ack();
    IIC_Send_Byte(DataToWrite);
    IIC_Wait_Ack();
    IIC_Stop();
    taskEXIT_CRITICAL();
    AT24C_delay_us(5000);  //EEPROM Ò³ÉÕÐ´µÈ´ý
}

u8 AT24CXX_ReadOneByte(u16 ReadAddr)
{
    u8 temp = 0;
    taskENTER_CRITICAL();
    IIC_Start();
    IIC_Send_Byte(0XA0);
    IIC_Wait_Ack();
    IIC_Send_Byte(ReadAddr);
    IIC_Wait_Ack();
    IIC_Start();
    IIC_Send_Byte(0XA1);
    IIC_Wait_Ack();
    temp = IIC_Read_Byte(0);
    IIC_Stop();
    taskEXIT_CRITICAL();
    return temp;
}

void AT24CXX_Write(u16 WriteAddr,u8 *pBuffer,u16 NumToWrite)
{
    while(NumToWrite--)
    {
        AT24CXX_WriteOneByte(WriteAddr,*pBuffer);
        WriteAddr++;
        pBuffer++;
    }
}

void AT24CXX_Read(u16 ReadAddr,u8 *pBuffer,u16 NumToRead)
{
    while(NumToRead--)
    {
        *pBuffer = AT24CXX_ReadOneByte(ReadAddr);
        ReadAddr++;
        pBuffer++;
    }
}

u8 AT24CXX_Check(void)
{
    u8 temp;
    temp = AT24CXX_ReadOneByte(0);
    if(temp == 0XA5)
        return 0;
    else
        AT24CXX_WriteOneByte(0,0XA5);
    temp = AT24CXX_ReadOneByte(0);
    if(temp == 0XA5)
        return 0;
    return 1;
}
