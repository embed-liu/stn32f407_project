#include "key.h"
#include "SysTick.h"

#include "FreeRTOS.h"

/*******************************************************************************
* 函 数 名         : KEY_Init
* 函数功能		   : 按键初始化
* 输    入         : 无
* 输    出         : 无
*******************************************************************************/
void KEY_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure; //定义结构体变量	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE|RCC_AHB1Periph_GPIOA,ENABLE); //使能端口时钟
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IN; //输入模式
	GPIO_InitStructure.GPIO_Pin=KEY0_PIN|KEY1_PIN|KEY2_PIN;//管脚设置
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_UP;//上拉
	GPIO_Init(KEY_PORT,&GPIO_InitStructure); //初始化结构体
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IN; //输入模式
	GPIO_InitStructure.GPIO_Pin=KEY_UP_PIN;//管脚设置
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_DOWN;//下拉
	GPIO_Init(KEY_UP_PORT,&GPIO_InitStructure); //初始化结构体
}

/*******************************************************************************
* 函 数 名         : KEY_Scan
* 函数功能		   : 按键扫描检测
* 输    入         : mode=0:单次按下按键
					 mode=1：连续按下按键
* 输    出         : 0：未有按键按下
					 KEY_UP_PRESS：KEY_UP键按下
					 KEY0_PRESS：KEY0键按下
					 KEY1_PRESS：KEY1键按下
					 KEY2_PRESS：KEY2键按下
*******************************************************************************/
uint8_t KEY_Scan(uint8_t mode)
{
    static uint8_t key_state = 0;
    static uint32_t key_tick = 0;
    uint8_t key_val = 0;

    if(mode == 1)
        key_state = 0;

    //读取按键电平
    if(KEY0 == 0) key_val |= (1<<0);
    if(KEY1 == 0) key_val |= (1<<1);
    if(KEY2 == 0) key_val |= (1<<2);
    if(KEY_UP == 1) key_val |= (1<<3);

    switch(key_state)
    {
        case 0: //空闲，检测按键按下
            if(key_val != 0)
            {
                key_tick = xTaskGetTickCount();
                key_state = 1;
            }
            break;
        case 1: //等待消抖 10ms
            if(xTaskGetTickCount() - key_tick >= pdMS_TO_TICKS(10))
            {
                if(key_val !=0)
                {
                    key_state = 2;
                    if(key_val & (1<<3)) return KEY_UP_PRESS;
                    if(key_val & (1<<0)) return KEY0_PRESS;
                    if(key_val & (1<<1)) return KEY1_PRESS;
                    if(key_val & (1<<2)) return KEY2_PRESS;
                }
                else
                {
                    key_state = 0;
                }
            }
            break;
        case 2: //等待松开
            if(key_val == 0)
            {
                key_state = 0;
                if(mode == 1)
                    key_state = 1;
            }
            break;
    }
    return 0;
}
