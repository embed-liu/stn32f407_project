#include "task_gui.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_app.h"
#include "wdt.h"

#define GUI_TASK_RECV_MS        5           //队列等待时间，同时作为刷新节拍

//LVGL 初始化：内核 + 显示移植层（在 main 里 TFTLCD_Init 之后、启动调度器之前调用）
void GUI_Init(void)
{
    lv_init();              //LVGL 内核初始化
    lv_port_disp_init();    //注册 HX8357D 显示设备，分辨率取 tftlcd_data 的宽高
}

//GUI 任务：刷 LVGL + 从 hDisplayQueue 取数据显示（任务在 main.c 里用 xTaskCreate 创建）
void Task_GUI(void *pvParameters)
{
    DisplayMsg_t msg;
    TickType_t last_tick;
    TickType_t now_tick;
    uint32_t elapsed;

    (void)pvParameters;

    lv_app_init();          //界面控件在任务栈里创建，不占 main 的 MSP 栈

    last_tick = xTaskGetTickCount();

    while(1)
    {
        //1. 用 tick 差值给 LVGL 补毫秒时基
        now_tick  = xTaskGetTickCount();
        elapsed   = (uint32_t)(now_tick - last_tick);
        last_tick = now_tick;
        if(elapsed > 0)
        {
            lv_tick_inc(elapsed);
        }

        //2. 处理 LVGL 定时器，把脏区域刷到屏上
        lv_timer_handler();

        //3. 取一帧显示数据，队列空就靠超时形成节拍
        if(xQueueReceive(hDisplayQueue, &msg, pdMS_TO_TICKS(GUI_TASK_RECV_MS)) == pdPASS)
        {
            lv_app_update(&msg);
        }

        //4. 接管原 Task_Display 的看门狗心跳槽位
        WDT_TaskHeartbeat(WDT_TASK_DISPLAY);
    }
}
