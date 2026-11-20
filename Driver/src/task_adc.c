#include "task_adc.h"
#include "adc.h"
#include "stm32f4xx.h"
#include "task_param.h"
#include "semphr.h"
#include "wdt.h"

/**
 * @brief ADC数据采集任务 Task_ADC
 * 优先级20，栈1024
 * 采集电压、电流模拟量；打包数据发送至ADC消息队列；超阈值释放信号量触发保护任务
 * 限位开关暂不读取，保留成员预留扩展
 */
void Task_ADC(void *pvParameters)
{
	AdcData_t adc_pkt;
	uint8_t i;
	uint16_t vol_th = 3000;   //电压阈值兜底
	uint16_t cur_th = 2000;   //电流阈值兜底
	BaseType_t qRet;
	static uint8_t prot_flag = 0; //保护锁存标志，只触发一次
	static uint8_t adc_warmup = 3; //上电先空转 3 轮不判保护：等 ADC 稳定 + 参数从 EEPROM 读出
	
	while(1)
	{
		// ========== 拿互斥锁读取参数【只拷贝，动作要快】 ==========
		if(xSemaphoreTake(hParamMutex, pdMS_TO_TICKS(50)) == pdTRUE)
		{
				vol_th = g_param.vol_th;
				cur_th = g_param.cur_th;
			xSemaphoreGive(hParamMutex);
		}
		//拿不到锁就继续用上面兜底的值，不卡采集
		
		//循环采集4个逻辑通道
		for(i = 0; i < 4; i++)
		{
			uint16_t raw = ADC_Read_Channel(i);
			adc_filter[i] = ADC_Filter(raw, adc_filter[i]); //一阶滤波
			adc_pkt.adc_raw[i] = adc_filter[i];
		}
		adc_pkt.limit_sta = 0;

		//入队发给显示任务
		qRet = xQueueSend(hAdcQueue, &adc_pkt, pdMS_TO_TICKS(10));
		
		//保护判断：CH0母线过压（对比vol_th）；CH1(M1)、CH2(M2)电机过流（对比cur_th）
		//上电前 3 轮只采集不判保护，避免刚上电时 ADC 未稳、阈值还是 0 造成误报警
		if(adc_warmup > 0)
		{
			adc_warmup--;
		}
		else if(adc_pkt.adc_raw[0] >= vol_th 
		|| adc_pkt.adc_raw[1] >= cur_th 
		|| adc_pkt.adc_raw[2] >= cur_th)
		{
			if(prot_flag == 0)
			{
				xSemaphoreGive(hProtectSem);
				prot_flag = 1;
			}
		}
		else
		{
			prot_flag = 0;
		}
		
		vTaskDelay(pdMS_TO_TICKS(100)); //100ms采集一轮全部4通道
				WDT_TaskHeartbeat(WDT_TASK_ADC);
    vTaskDelay(pdMS_TO_TICKS(200));
	}
}
