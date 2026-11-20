#include "stm32f4xx.h"
#include "led.h"
#include "system.h"
#include "SysTick.h"
#include "tftlcd.h"
#include "key.h"
#include "beep.h"
#include "usart.h"
#include "i2c.h"
#include "step_motor.h"
#include "adc.h"
#include "mb.h"
#include "mb_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "task_motor.h"
#include "task_adc.h"
#include "task_protect.h"
#include "task_gui.h"
#include "lv_app.h"
#include "task_param.h"
#include "wdt.h"
#include "string.h"

char adc_buf_str[40];
extern volatile uint16_t motor1_remain_step;
extern volatile uint16_t motor2_remain_step;
volatile uint16_t motor1_target_step = 0;
volatile uint16_t motor2_target_step = 0;
//方向变量
volatile uint16_t motor1_dir_reg = 1;
volatile uint16_t motor2_dir_reg = 1;
//全局速度寄存器，TIM6 ARR，上电默认999，两台电机共用TIM6
volatile uint16_t motor_speed_reg = 999;

//==== 消息队列、信号量、互斥锁【定义放在main.c】====
QueueHandle_t hMotorQueue;
SemaphoreHandle_t hMotorMutex;
QueueHandle_t hAdcQueue;        //ADC采集数据消息队列
SemaphoreHandle_t hProtectSem;  //触发保护任务的二值信号量
QueueHandle_t hDisplayQueue;
SemaphoreHandle_t hParamMutex;
extern Param_t g_param;
extern uint8_t param_dirty_flag;

/**
 * @brief LED闪烁任务：优先级2，栈512字，500ms翻转一次LED
 */
void Task_LedBlink(void *pvParameters)
{
	while(1)
	{
		LED1 = !LED1;	//翻转LED
		vTaskDelay(pdMS_TO_TICKS(200));
	}
}

/**
 * @brief Modbus轮询任务，专门跑eMBPoll，优先级4
 */
void Task_Modbus(void *pvParameters)
{
	while(1)
	{
		eMBPoll();
		WDT_TaskHeartbeat(WDT_TASK_MODBUS);
		vTaskDelay(pdMS_TO_TICKS(2)); //2ms轮询，适配9600 RTU
	}
}

/**
 * @brief 主应用任务：电机、ADC、显示、按键扫描，优先级3
 */
void Task_MainApp(void *pvParameters)
{
	uint8_t sel = 0; //0=选中cur_th电流阈值，1=选中vol_th电压阈值
	MotorCmd_t send_cmd; //局部指令结构体
	AdcData_t adc_recv;  //接收ADC队列数据包
	DisplayMsg_t disp_send; //发给显示任务的包
	
	uint8_t key_val;
	
	//初始提示
	strcpy(disp_send.tip_str,"Motor:Modbus Only");
	
	while(1)
	{
		//==== 使用普中原版 KEY_Scan 轮询按键 ====
		// KEY0 切页 / KEY2 选择阈值项 / KEY1 减50 / KEY_UP 加50，改完自动存 EEPROM
		key_val = KEY_Scan(0);
		if(key_val != 0)
		{
			if(key_val == KEY0_PRESS)
			{
				lv_app_page_next();                 //界面切到下一页
				strcpy(disp_send.tip_str,"Page Next");
			}
			else if(key_val == KEY2_PRESS)
			{
				sel = !sel;
				if(sel == 0)
				{
					strcpy(disp_send.tip_str,"SEL: Cur Th");
				}
				else
				{
					strcpy(disp_send.tip_str,"SEL: Vol Th");
				}
			}
			else if(key_val == KEY1_PRESS)
			{
				//阈值 -50，下限 50
				if(xSemaphoreTake(hParamMutex, pdMS_TO_TICKS(50)) == pdTRUE)
				{
					if(sel == 0)
					{
						if(g_param.cur_th > 50)
							g_param.cur_th -= 50;
					}
					else
					{
						if(g_param.vol_th > 50)
							g_param.vol_th -= 50;
					}
					param_dirty_flag = 1;
					xSemaphoreGive(hParamMutex);
				}
				strcpy(disp_send.tip_str,"Th -50");
			}
			else if(key_val == KEY_UP_PRESS)
			{
				//阈值 +50，上限 4095
				if(xSemaphoreTake(hParamMutex, pdMS_TO_TICKS(50)) == pdTRUE)
				{
					if(sel == 0)
					{
						if(g_param.cur_th + 50 <= 4095)
							g_param.cur_th += 50;
					}
					else
					{
						if(g_param.vol_th + 50 <= 4095)
							g_param.vol_th += 50;
					}
					param_dirty_flag = 1;
					xSemaphoreGive(hParamMutex);
				}
				strcpy(disp_send.tip_str,"Th +50");
			}
		}

		uint16_t r1,r2;
		taskENTER_CRITICAL();
		r1 = motor1_remain_step;
		r2 = motor2_remain_step;
		taskEXIT_CRITICAL();
		disp_send.m1_remain = r1;
		disp_send.m2_remain = r2;
		disp_send.sel = sel;                     //参数页当前选中的阈值项
		disp_send.m_speed = motor_speed_reg;     //电机速度寄存器，界面显示用
		
		//Modbus下发控制，使用寄存器内的方向
		if(motor1_target_step != 0)
		{
			send_cmd.motor_id = 1;
			send_cmd.dir = (uint8_t)motor1_dir_reg;
			send_cmd.step_cnt = motor1_target_step;
			send_cmd.speed_arr = motor_speed_reg;
			xQueueSend(hMotorQueue, &send_cmd, 0);
			motor1_target_step = 0;
			strcpy(disp_send.tip_str,"Modbus M1 Run");
		}
		if(motor2_target_step != 0)
		{
			send_cmd.motor_id = 2;
			send_cmd.dir = (uint8_t)motor2_dir_reg;
			send_cmd.step_cnt = motor2_target_step;
			send_cmd.speed_arr = motor_speed_reg;
			xQueueSend(hMotorQueue, &send_cmd, 0);
			motor2_target_step = 0;
			strcpy(disp_send.tip_str,"Modbus M2 Run");
		}
		
		//读取ADC队列里最新采集数据，填充给显示包
		if(xQueueReceive(hAdcQueue, &adc_recv, 0) == pdPASS)
		{
		    //CH0 PA4：母线电压 V
		    disp_send.bus_vol = adc_recv.adc_raw[0] * 3.3f / 4095.0f;
		    //CH1 PA5：M1采样电压 → M1电流 A，Rs=0.1Ω
		    disp_send.m1_cur = adc_recv.adc_raw[1] * 3.3f / 4095.0f / 0.1f;
		    //CH2 PA6：M2采样电压 → M2电流 A，Rs=0.1Ω
		    disp_send.m2_cur = adc_recv.adc_raw[2] * 3.3f / 4095.0f / 0.1f;
		    //CH3 PA7：预留通道 电压 V
		    disp_send.ch3_vol = adc_recv.adc_raw[3] * 3.3f / 4095.0f;
		}

		//读取阈值，填充进显示包
		if(xSemaphoreTake(hParamMutex, pdMS_TO_TICKS(50)) == pdTRUE)
		{
			disp_send.cur_th = g_param.cur_th;
			disp_send.vol_th = g_param.vol_th;
			xSemaphoreGive(hParamMutex);
		}

		static uint32_t last_disp_tick = 0;
		if(xTaskGetTickCount() - last_disp_tick >= pdMS_TO_TICKS(100))
		{
			xQueueSend(hDisplayQueue, &disp_send, 0);
			last_disp_tick = xTaskGetTickCount();
		}
		
		WDT_TaskHeartbeat(WDT_TASK_MAINAPP);
		vTaskDelay(pdMS_TO_TICKS(20));
	}
}


int main(void)
{
	SysTick_Init(168);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	
	led_Init();
	TFTLCD_Init();
	KEY_Init();
	BEEP_Init();
	USART1_Init(115200);
	STEP_Motor_Init();
	STEP_Motor2_Init();
	TIM6_Init(83,999); // 168M APB1: 84MHz, 84M/(83+1)/(999+1)=1000Hz中断
	ADC1_Init();
	
	//初始化FreeModbus
	eMBInit(MB_RTU,0x01,0x02,9600,MB_PAR_NONE);
	eMBEnable();
	
	FRONT_COLOR=BLACK;
	//==== LVGL 图形界面初始化：内核 + HX8357D 显示移植 + 监控界面 ====
	//LCD 底层(TFTLCD_Init)已完成，这里不再用 LCD_ShowString 裸写文字，界面全部交给 LVGL 绘制
	GUI_Init();
	
	//==== 创建电机指令队列：最多存放5条指令 ====
	hMotorQueue = xQueueCreate(5, sizeof(MotorCmd_t));
	//==== 创建互斥锁，保护电机资源 ====
	hMotorMutex = xSemaphoreCreateMutex();
	//创建EEPROM互斥锁
	hParamMutex = xSemaphoreCreateMutex();
	if(hParamMutex == NULL)
	{
			printf("Param mutex create fail!\r\n");
	}

	//==== ADC数据队列：最多缓存8组采集数据包
	hAdcQueue = xQueueCreate(8, sizeof(AdcData_t));
	//==== 创建保护触发二值信号量
	hProtectSem = xSemaphoreCreateBinary();

	//==== 显示队列，最多缓存3帧画面，丢帧优先保证控制
	hDisplayQueue = xQueueCreate(3, sizeof(DisplayMsg_t));

	//==== WDT 计数信号量，标准API ====
	hWdtSem = xSemaphoreCreateCounting(16, 0);
	if(hWdtSem == NULL)
	{
		printf("wdt sem create fail\r\n");
	}
	

	//==== 保护阈值兜底：ADC 任务优先级(20) 高于 Param 任务(10)，第一轮采集时 g_param 还是 0 ====
	//==== 0 阈值会让"任何采样值 >= 0"都成立，误判过流/过压，表现为上电蜂鸣器先响一次 ====
	Param_SetDefault(&g_param);

	// 创建任务
	xTaskCreate(Task_LedBlink, "Task_LedBlink", 512, NULL, 2, NULL);
	xTaskCreate(Task_MainApp,  "Task_MainApp",  1024, NULL,3,NULL);
	xTaskCreate(Task_ADC,      "Task_ADC",1024,NULL,20,NULL); //ADC采集任务 优先级20
	xTaskCreate(Task_Motor,    "Task_Motor",1024,NULL,25,NULL);
	xTaskCreate(Task_Modbus,   "Task_Modbus",512,NULL,4,NULL);
	xTaskCreate(Task_GUI,      "Task_GUI",1024,NULL,5,NULL);   //GUI 任务：LVGL 刷新 + 从 hDisplayQueue 取数据显示（接管原 Task_Display 职责）
	xTaskCreate(Task_Protect,  "Task_Protect",512,NULL,30,NULL);
	xTaskCreate(Task_Param,    "Task_Param", 512, NULL, 10, NULL);
	xTaskCreate(Task_WDT,      "Task_WDT",   512, NULL, 8, NULL);  //看门狗任务
	
	//启动调度器
	vTaskStartScheduler();
	
	while(1)
	{
	}
}
#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line)
{
    while (1)
    {
    }
}
#else
void __aeabi_assert(const char * x1, const char * x2, int x3)
{
}
#endif
