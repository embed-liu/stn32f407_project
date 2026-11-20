# LVGL 8.3.11 移植说明（STM32F407ZGT6 + HX8357D 3.5 寸 320*480 FSMC）

工程：`stm32f407_xiangmu.uvprojx`
移植日期：2026-11-15

## 一、硬约束遵守情况

| 约束 | 结果 |
|---|---|
| 不改 `tftlcd.c` / `tftlcd.h` | 未改动。LVGL 只调用公有接口 `LCD_Set_Window()` 和 `TFTLCD->LCD_DATA` |
| 不改 `i2c.c` | 未改动 |
| 不改 `step_motor.c` | 未改动 |
| 其他原有驱动 / 任务 | `adc.c`、`wdt.c`、`task_display.c/.h` 等全部原样，`task_display.c` 只是不再被调度 |

## 二、新增的目录与文件

```
GUI/
├─ lv_conf.h                 LVGL 配置文件
├─ LVGL移植说明.md           本文件
├─ lvgl/                     LVGL 8.3.11 官方源码（lvgl.h + src/ 下 192 个 .c，原样拷贝）
├─ lvgl_driver/
│   ├─ lv_port_disp.c        显示移植层：LVGL 刷屏 → HX8357D
│   └─ lv_port_disp.h
├─ app/
│   ├─ lv_app.c              监控主界面（控件创建 + 数据刷新）
│   ├─ lv_app.h
│   ├─ font_cn_16.c          中文 16 号字（bpp4）
│   └─ font_cn_24.c          中文 24 号字（bpp4）
└─ (无其它文件)
```

GUI 任务的文件按工程原有习惯放在驱动目录里（和 task_display.c / task_display.h 一致）：

```
Driver/
├─ src/
│   └─ task_gui.c            GUI_Init() / GUI_Task（和 task_motor.c 一样，归 App 分组）
└─ inc/
    └─ task_gui.h            GUI_Init / GUI_Task 声明 + DisplayMsg_t + hDisplayQueue
```
Keil 工程的 `Options → C/C++ → Include Paths` 已追加 4 条：
`.\GUI`、`.\GUI\lvgl`、`.\GUI\lvgl_driver`、`.\GUI\app`

## 三、关键设计决策

### 1. 刷屏为什么不用工程自带的 `LCD_Color_Fill()`
`tftlcd.c` 里的 `LCD_Color_Fill()` 是**逐像素**调用 `LCD_Set_Window()` 的写法，一个像素就要走一次
`0x2A/0x2B/0x2C` 三条指令。LVGL 整屏有 153,600 个像素，用它刷一屏要几秒，完全不可用。

`lv_port_disp.c` 的做法：**整块区域只设一次窗口**，然后在一个循环里直接写 `TFTLCD->LCD_DATA`。
HX8357DN 是 8 位总线时序，一个像素必须写两次（先高字节后低字节），这里与 `tftlcd.c` 里
`LCD_WriteData_Color()` 的处理完全一致，只是省掉了每个像素的函数调用。

### 2. 单缓冲 + 部分刷新
- 缓冲区：`LV_HOR_RES_MAX * 40` = 320*40 = 12,800 像素 = 25.6KB SRAM
- `disp_drv.hor_res / ver_res` 直接取 `tftlcd_data.width / height`（`LCD_Display_Dir(0)` 竖屏 = 320*480），
  没有写死数字，以后改屏幕方向不会刷错位。

### 3. 内存
- `LV_MEM_CUSTOM 0`：用 LVGL 内部静态池，**不用 malloc**，避免和 FreeRTOS `heap_4`（36KB）抢堆
- `LV_MEM_SIZE = 32KB`：当前界面约用 10~15KB，留了余量；RAM 吃紧时可降到 16KB

### 4. 中文字体
用 `lv_font_conv` + 系统黑体 `simhei.ttf` 生成，4bpp，只包含界面实际用到的字：

> 工业监控节点 / 在线 / 电机运行 / 剩余步数 / 模拟量采集 / 母线电压 / 电流 / 通道3 电压 /
> 阈值设置 / 报警 / 正常

ASCII 覆盖 0x20-0x7F。两个字号合计约 176KB Flash。**以后界面要加新的中文词，必须重新生成字体**，
否则新字会显示成方块（可以用 `lv_font_conv --size 16 --bpp 4 -r 0x20-0x7F --symbols "旧字新字" --lv-font-name font_cn_16` 重跑）。

### 5. 线程模型（重要）
LVGL 不是线程安全的，所以**所有 LVGL 调用都集中在 `Task_GUI` 一个任务里**：
- `GUI_Init()` 只做 `lv_init()` + `lv_port_disp_init()`，很浅，可以在 `main()` 里调用；
- 界面控件在 `Task_GUI` 里创建——**因为 startup 里 `Stack_Size` 只有 0x400（1KB）**，在 `main()` 里建控件会爆 MSP 栈；
- 时基：任务里用 `xTaskGetTickCount()` 的差值调 `lv_tick_inc()`，**没有改动 FreeRTOS 的 SysTick 中断**；
- 数据：`xQueueReceive(hDisplayQueue, &msg, 5ms)` → `lv_app_update(&msg)`，队列空时靠超时形成节拍。

### 6. `task_display.c/.h` 已删除
原 `Task_Display` 拿 `hDisplayQueue` 里的数据用 `LCD_ShowString` 裸写文字，和 LVGL 界面重复，所以整个功能由 `Task_GUI` 接替：
队列 `hDisplayQueue` 只留一个消费者，数据结构 `DisplayMsg_t` 和 `extern hDisplayQueue` 搬到了 `Driver/inc/task_gui.h`。

`Driver/src/task_display.c`、`Driver/inc/task_display.h` 已删除，工程 App 分组里的条目也一并移除（原文件在工作目录外留了备份，需要回退可以从备份恢复）。

`Task_GUI` 接管了原 `Task_Display` 的看门狗心跳槽位 `WDT_TASK_DISPLAY`，所以 `wdt.c` 的监控表不用改，
也不会因为任务没了而误复位。

## 四、main.c 的改动（只有 3 处）

1. 增加 `#include "task_gui.h"`（原 `task_display.h` 的位置，第 21 行）
2. 把原来"`FRONT_COLOR=BLACK;` + `LCD_ShowString` 裸写文字"那一整段
   替换成 `GUI_Init();`（第 237-239 行附近），并删掉了只给裸写文字用的 `uint16_t color` 变量
3. `xTaskCreate(Task_Display, ...)` 换成 `xTaskCreate(GUI_Task, "Task_GUI",1024,NULL,5,NULL);`（第 273 行，
   和 Task_ADC / Task_Motor 一样的写法，栈 4KB、优先级 5）

其余（时钟、外设、FreeModbus、8 个原有任务、队列、互斥锁）全部没动。

## 五、编译与资源占用

```
Build Result: 0 Error(s), 57 Warning(s)

Program Size: Code=210802  RO-data=89526  RW-data=1084  ZI-data=98460
```

| 项目 | 移植前 | 移植后 |
|---|---|---|
| Code | 28,404 | 210,802 |
| RO-data | 20,412 | 89,526 |
| RW-data | 496 | 1,084 |
| ZI-data | 39,144 | 98,460 |
| Flash 占用 | 约 48KB | 约 294KB / 1MB（29%） |
| RAM 占用（RW+ZI） | 约 38KB | 约 97KB / 128KB（76%） |

RAM 里新增的主要是：LVGL 静态内存池 32KB + 绘制缓冲 25.6KB。
**剩余约 31KB**，够用；如果后面控件继续加，优先把 `LV_MEM_SIZE` 降到 24KB 或把缓冲行数从 40 降到 24 行。

### 57 条告警说明
- 全部在 LVGL 源码和原有代码里，**本次新增的源文件零告警**。
- 其中 2 条是原工程本来就有的，和本次改动无关：
  - `main.c(229) #223-D: function "TIM6_Init" declared implicitly`
    （`TIM6_Init` 定义在 `step_motor.c`，头文件里没有声明；按"不动驱动"的约束未处理）
  - `main.c(296) #1461-D: function declared with "noreturn" does return`
- 其余 60 条是 LVGL 自身的类型/未使用变量告警，不影响功能。

## 六、界面说明（320*480 竖屏，深色工业风）

```
┌────────────────────────────────────┐
│ 工业监控节点                  在线 │  ← 标题栏
├────────────────────────────────────┤
│ 电机运行                           │
│ M1 剩余步数              1234      │
│ M2 剩余步数               456      │
├────────────────────────────────────┤
│ 模拟量采集                         │
│ 母线电压                4.87 V     │
│ ████████████████░░░░░░             │
│ M1 电流                 1.24 A     │
│ █████░░░░░░░░░░░░░░░░              │
│ M2 电流                 0.66 A     │
│ ███░░░░░░░░░░░░░░░░░               │
│ 通道3 电压              0.12 V     │
│ ░░░░░░░░░░░░░░░░░░░░               │
├────────────────────────────────────┤
│ 阈值设置                           │
│ 电流阈值        2000 / 1.6A        │
│ 电压阈值        3000 / 2.42V       │
├────────────────────────────────────┤
│ Motor:Modbus Only                  │  ← 底部提示条（tip_str）
└────────────────────────────────────┘
```

- 超阈值时：M1/M2 电流数值变红，母线电压数值变橙，方便现场一眼看出异常；
- 阶梯 100ms 刷新一次，LVGL 自身按 30ms 周期重绘脏区域，不是整屏重画。

## 七、上板验证清单（建议按顺序）

1. 烧写后先看**是否亮屏、颜色对不对**（若颜色反了：把 `lv_conf.h` 里 `LV_COLOR_16_SWAP` 改 1）；
2. 看中文有没有显示（方块 = 字体缺字，需重新生成字体）；
3. 用 Modbus Poll 写 motor 1-2 寄存器，看 M1/M2 剩余步数是否跟着变；
4. 拧电位器改 ADC，看电压/电流数值与条形是否联动；
5. 写电流阈值寄存器到很小值，看数值是否变红。

## 八、可选调优点

| 想改什么 | 改哪里 |
|---|---|
| 刷屏更快 | Keil `Options → C/C++ → Optimization` 从 Level 0 调到 Level 1 或 Level 2（**会改变全工程代码生成，原有驱动虽逻辑不变但请先确认愿意**） |
| 缓冲区更大/更小 | `GUI/lvgl_driver/lv_port_disp.h` 的 `LV_PORT_DISP_BUF_LINES`（行数×320×2 字节 = SRAM 占用） |
| LVGL 内存池 | `GUI/lv_conf.h` 的 `LV_MEM_SIZE` |
| 换屏幕方向 | 改 `tftlcd.c` 里 `LCD_Display_Dir()` 的调用参数（移植层自动跟随 `tftlcd_data` 宽高） |
| 界面配色/布局 | `GUI/app/lv_app.c` 顶部的 `COLOR_*` 宏和 `lv_app_init()` 里的坐标 |
| GUI 任务的栈/优先级 | `User/main.c` 里那行 `xTaskCreate(GUI_Task, ...)` |
| 加新的中文词 | 必须用 `lv_font_conv` 重新生成 `font_cn_16.c` / `font_cn_24.c` |

> 注意：`GUI` 分组里的 LVGL 源码是官方原样代码，不要改。所有定制都在 `lv_conf.h`、
> `lvgl_driver/`、`app/`、`Driver/src/task_gui.c` 这几个自己的文件里。

## 九、中文显示的关键约束（重要，改界面必读）

Keil 工程里的 .c 文件是 **GBK 编码**，字符串字面量编译出来就是 GBK 字节；
而 LVGL 的 LV_TXT_ENC 是 UTF-8，两边对不上就**不显示任何中文字**（本次踩过这个坑）。

所以 lv_app.c 里所有中文都写成 **UTF-8 字节转义**，例如：

`c
lv_label_set_text(title, "\xE5\xB7\xA5\xE4\xB8\x9A\xE7\x9B\x91\xE6\x8E\xA7\xE8\x8A\x82\xE7\x82\xB9"); /* 工业监控节点 */
`

规则：
- 注释里可以放心写中文（GBK 不影响编译，Keil 里也能正常读）；
- **真正传给 LVGL 的字符串必须是 \x 转义**，不能直接写中文；
- 转义后面紧跟 0~9 / A~F 这些字符时，要用相邻字符串拆开写，例如
  "\xE9\x80\x9A\xE9\x81\x93" "3 \xE7\x94\xB5\xE5\x8E\x8B"（否则 \x 会把后面的 3 当成十六进制位吞掉）；
- 新增中文字必须先确认字体里有该字形（重新生成 ont_cn_16.c / ont_cn_24.c），否则显示为方块。

## 十、保护任务的报警显示（2026-11-15 调整）

`Task_Protect` 原来用 `LCD_ShowString(10,200,...)` 直接往屏上写 "!!!PROTECT TRIGGER!!!" / "Motor State: Ready"，这两处裸写已全部去掉（界面完全由 LVGL 绘制）。

现在改成：`Task_Protect` 调 `lv_app_request_alarm(1)` 置位标志 → `Task_GUI` 在底部提示条上用红字显示 "!!!PROTECT TRIGGER!!!"，1 秒后 `lv_app_request_alarm(0)` 恢复普通提示。跨任务只传标志位，LVGL 调用仍然只在 `Task_GUI` 里，线程安全。

> 以后需要从别的任务提示界面，照这个做法加标志位，不要直接调 LVGL，也不要再裸写 LCD。
## 十一、三页界面与新增的 Modbus 功能（2026-11-15）

### 界面拆成三页，KEY0 循环切页

| 页 | 内容 |
|---|---|
| 1 电机状态与ADC采集 | M1/M2 剩余步数、速度(ARR)、母线电压、M1/M2 电流、通道3 电压（带条形指示）、最近 60 秒电流趋势图 |
| 2 故障记录 | 最近 6 次保护触发：序号、时间(mm:ss)、类型（母线过压/M1过流/M2过流）、触发瞬间的数值 |
| 3 参数设置 | 电流阈值 / 电压阈值（原始值 + 折算值），当前选中项用 "> " 标出；下方是按键与 Modbus 操作说明 |

顶部标题栏三页共用：标题 + **Modbus 在线/离线**（3 秒内收到过主站有效请求即为在线，由 `mb_app.c` 的 `mb_last_active` 打点）+ 页码。底部提示条与报警也三页共用。

### 按键（Task_MainApp 里轮询）

| 键 | 功能 |
|---|---|
| KEY0 | 切到下一页 |
| KEY2 | 选择要调的阈值（电流 / 电压） |
| KEY1 | 阈值 -50 |
| KEY_UP | 阈值 +50 |

改完自动置 `param_dirty_flag`，由 `Task_Param` 写入 24C02。

### Modbus 保持寄存器（Poll 起始地址 1）

| Poll 地址 | 读写 | 含义 |
|---|---|---|
| 1~4 | 读 | ADC CH0~CH3 原始值 |
| 5 / 6 | 读 | M1 / M2 剩余步数 |
| 7 / 8 | 读/写 | 电流阈值 / 电压阈值（1~4095，写入后自动存 EEPROM） |
| 10 / 11 | 写 | M1 / M2 目标步数 |
| 12 / 13 | 读/写 | M1 / M2 方向 |
| 14 | 读/写 | 全局速度（ARR，10~2000） |

> 阈值写寄存器和按键共用同一份 `g_param`，用 `hParamMutex` 保护；界面 100ms 刷新一次，改完立刻能看到。

### 中文字体已重新生成

`GUI/app/font_cn_16.c` / `font_cn_24.c` 用 lv_font_conv 重新生成，字符集是 ASCII 0x20~0x7F + 界面用到的 51 个汉字（`暂无`、`故障`、`离线`、`趋势`、`过流`、`操作说明` 等新增字都在里面）。改界面文字后记得按第十节的步骤重新生成字体，否则新字不显示。
