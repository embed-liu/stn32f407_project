#include "adc.h"
#include "SysTick.h"
#include "FreeRTOS.h"
uint16_t adc_buf[ADC_CH_NUM];
uint16_t adc_filter[ADC_CH_NUM];

void ADC1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    ADC_InitTypeDef ADC_InitStruct;
    ADC_CommonInitTypeDef ADC_CommonInitStruct;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    // PA4 PA5 PA6 PA7
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AN;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    //ADC公共配置
    ADC_CommonInitStruct.ADC_Mode = ADC_Mode_Independent;
    ADC_CommonInitStruct.ADC_Prescaler = ADC_Prescaler_Div2;
    ADC_CommonInitStruct.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
    ADC_CommonInitStruct.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
    ADC_CommonInit(&ADC_CommonInitStruct);
    
    ADC_InitStruct.ADC_Resolution = ADC_Resolution_12b;
    ADC_InitStruct.ADC_ScanConvMode = DISABLE;
    ADC_InitStruct.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStruct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    ADC_InitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
    ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStruct.ADC_NbrOfConversion = 1;
    ADC_Init(ADC1, &ADC_InitStruct);
    ADC_Cmd(ADC1, ENABLE);
		
		//滤波数组初始化
		adc_filter[0] = 0;
		adc_filter[1] = 0;
		adc_filter[2] = 0;
		adc_filter[3] = 0;

}

uint16_t ADC_Read_Channel(uint8_t ch)
{
    uint8_t adc_hw_ch;
    //逻辑通道 -> STM32硬件ADC通道映射
    switch(ch)
    {
        case 0: adc_hw_ch = ADC_Channel_4; break; //PA4
        case 1: adc_hw_ch = ADC_Channel_5; break; //PA5
        case 2: adc_hw_ch = ADC_Channel_6; break; //PA6
        case 3: adc_hw_ch = ADC_Channel_7; break; //PA7
        default: adc_hw_ch = ADC_Channel_4; break;
    }

    ADC_RegularChannelConfig(ADC1, adc_hw_ch, 1, ADC_SampleTime_480Cycles);
    ADC_SoftwareStartConv(ADC1);
    while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    uint16_t val = ADC_GetConversionValue(ADC1);
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
    return val;
}



//一阶滑动滤波
uint16_t ADC_Filter(uint16_t new_val, uint16_t old_val)
{
    return (old_val * 3 + new_val) / 4;
}
