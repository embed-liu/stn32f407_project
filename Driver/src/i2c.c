#include "i2c.h"
#include "SysTick.h"

// F407 168MHz，专门给I2C时序用，保留你原来的循环延时
static void i2c_delay_us(uint32_t nus)
{
    uint32_t i,j;
    for(i = nus; i > 0; i--)
        for(j = 16; j > 0; j--);
}

void IIC_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    // ========== 重点修改：开漏 OD，不是推挽 PP ==========
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    IIC_SCL = 1;
    IIC_SDA = 1;
}

void SDA_OUT(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD; // 开漏！
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void SDA_IN(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void IIC_Start(void)
{
    I2C_ENTER_CRITICAL();
    SDA_OUT();
    IIC_SDA = 1;
    IIC_SCL = 1;
    i2c_delay_us(5);
    IIC_SDA = 0;
    i2c_delay_us(6);
    IIC_SCL = 0;
    I2C_EXIT_CRITICAL();
}

void IIC_Stop(void)
{
    I2C_ENTER_CRITICAL();
    SDA_OUT();
    IIC_SCL = 0;
    IIC_SDA = 0;
    IIC_SCL = 1;
    i2c_delay_us(6);
    IIC_SDA = 1;
    i2c_delay_us(6);
    I2C_EXIT_CRITICAL();
}

uint8_t IIC_Wait_Ack(void)
{
    I2C_ENTER_CRITICAL();
    uint8_t tempTime = 0;
    SDA_IN();
    IIC_SDA = 1;
    i2c_delay_us(1);
    IIC_SCL = 1;
    i2c_delay_us(1);
    while(READ_SDA)
    {
        tempTime++;
        if(tempTime > 250)
        {
            IIC_Stop();
            I2C_EXIT_CRITICAL();
            return 1;
        }
    }
    IIC_SCL = 0;
    I2C_EXIT_CRITICAL();
    return 0;
}

void IIC_Ack(void)
{
    I2C_ENTER_CRITICAL();
    IIC_SCL = 0;
    SDA_OUT();
    IIC_SDA = 0;
    i2c_delay_us(2);
    IIC_SCL = 1;
    i2c_delay_us(5);
    IIC_SCL = 0;
    I2C_EXIT_CRITICAL();
}

void IIC_NAck(void)
{
    I2C_ENTER_CRITICAL();
    IIC_SCL = 0;
    SDA_OUT();
    IIC_SDA = 1;
    i2c_delay_us(2);
    IIC_SCL = 1;
    i2c_delay_us(5);
    IIC_SCL = 0;
    I2C_EXIT_CRITICAL();
}

void IIC_Send_Byte(uint8_t txd)
{
    I2C_ENTER_CRITICAL();
    uint8_t t;
    SDA_OUT();
    IIC_SCL = 0;
    for(t = 0; t < 8; t++)
    {
        if((txd & 0x80) > 0)
            IIC_SDA = 1;
        else
            IIC_SDA = 0;
        txd <<= 1;
        i2c_delay_us(2);
        IIC_SCL = 1;
        i2c_delay_us(2);
        IIC_SCL = 0;
        i2c_delay_us(2);
    }
    I2C_EXIT_CRITICAL();
}

uint8_t IIC_Read_Byte(uint8_t ack)
{
    I2C_ENTER_CRITICAL();
    uint8_t i, receive = 0;
    SDA_IN();
    for(i = 0; i < 8; i++ )
    {
        IIC_SCL = 0;
        i2c_delay_us(2);
        IIC_SCL = 1;
        receive <<= 1;
        if(READ_SDA) receive++;
        i2c_delay_us(1);
    }
    if (!ack)
        IIC_NAck();
    else
        IIC_Ack();
    I2C_EXIT_CRITICAL();
    return receive;
}
