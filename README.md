# 26diansai — STM32 平衡滚球控制项目

本项目是基于 **STM32F407VGT6** 的平衡滚球（摆杆）控制系统，用于第 26 届电子设计竞赛（电赛）的平衡控制赛题。系统通过相机识别钢球位置，使用算法控制 **ZDT X42S 闭环步进电机** 驱动摆杆倾斜，从而让钢球沿摆杆滚动到目标位置并稳定。

固件采用 **STMicroelectronics STM32CubeMX + CMake + FreeRTOS**，运行逻辑基于 **LibXR** 实时系统框架（`Middlewares/Third_Party/LibXR`）。

---

## 目录结构

```
26diansai/
├── 26diansai.ioc              # STM32CubeMX 工程配置（引脚/时钟/外设）
├── CMakeLists.txt             # 顶层 CMake 构建脚本
├── CMakePresets.json          # Ninja 构建预设（Debug/Release）
├── STM32F407xx_FLASH.ld       # 链接脚本
├── Core/                      # CubeMX 生成的 HAL / FreeRTOS / 外设代码
│   ├── Inc/                   #   （main.h 等头文件）
│   └── Src/                   #   main.c / freertos.c / spi.c / usart.c / dma.c 等
├── User/                      # 应用入口与系统配置
│   ├── app_main.cpp           #   app_main()：外设实例化、任务创建、平衡参数配置
│   ├── app_main.h
│   ├── flash_map.hpp          #   自动生成的 Flash 分区表（STM32F407VGT6）
│   └── libxr_config.yaml      #   LibXR 外设与 Flash 布局配置
├── bu_task/                   # 平衡任务（业务逻辑）
│   ├── bu_task.cpp/.hpp       #   ZeroAndMove / BallTracking 线程接口
│   ├── balance_controller.cpp/.hpp  # 平衡控制器、任务状态机、电机调试邮箱
│   ├── operator_panel.cpp/.hpp      # 按键 + LCD 操作界面
│   └── README.md              # 平衡任务详细说明
├── bujin_motor/               # ZDT X42S 闭环步进电机串口驱动
│   ├── zdt_x42s.cpp/.hpp      #   TTL/RS485 自由协议驱动（USART3）
│   ├── Emm_V5.c/.h            #   EMM V5 参考命令实现
│   └── READMA.md              # 驱动说明
├── maxican/                   # 视觉小球测量接收
│   ├── maxican.cpp/.hpp       #   BallReceiver / BallMailbox / 帧解析
├── LCD/                       # SPI LCD 驱动（0.96 / 1.14 / 1.47 英寸）
│   ├── lcd_core.c  lcd_port.c  lcd_font.c
│   ├── lcd_libxr_port.cpp      #   通过 LibXR SPI 接入
│   ├── lcd.h  lcd_port.h  lcd_font.h  picture.h
├── Middlewares/               # 第三方库
│   └── Third_Party/LibXR/     #   LibXR 实时系统框架（git 子模块）
├── cmake/                     # 构建辅助脚本
│   ├── starm-clang.cmake      #   交叉编译工具链（arm clang）
│   ├── gcc-arm-none-eabi.cmake
│   ├── LibXR.CMake            #   LibXR 集成
│   └── stm32cubemx/           #   CubeMX 生成的 CMake
├── ZDTMotor-master/           # 参考实现（CAN 版电机工程）
└── build/                     # 构建产物目录
```

---

## 硬件与处理器

| 项目 | 配置 |
| --- | --- |
| MCU | STM32F407VGT6（Cortex-M4F，LQFP100） |
| 系统主频 | 168 MHz（HSE 8 MHz，PLL M=4 / N=168 / P=2，Q=7 → 48 MHz USB） |
| 基础时基 | TIM6（`TIM6_DAC_IRQn`），供 FreeRTOS 与 `HAL_IncTick` 使用 |
| 工具链 | CMake + Ninja + `starm-clang`（ARM cross-clang / picolibc） |

### 外设分配

| 外设 | 用途 | 引脚 / 说明 |
| --- | --- | --- |
| USART1 | Maxican 视觉小球数据接收 | `PA9(TX)` / `PA10(RX)`，DMA 循环接收 |
| USART2 | 串口终端（`STDIO`） | `PA2(TX)` / `PA3(RX)`，115200-8N1 |
| USART3 | ZDT X42S 电机通信 | `PD8(TX)` / `PD9(RX)`，115200-8N1 |
| SPI1 | LCD 屏幕（SPI 单线） | `PB3(SCK)` / `PB5(MOSI)`，~5.25 Mbit/s |
| GPIO | 按键 | `PA4`(选择) / `PA5`(启动/中止) / `PC13`(回零) |
| GPIO | LCD 控制 | `PD3`(PWR) / `PD4`(RST) / `PD7`(CS) / `PB4`(DC) |
| USB FS | CDC 虚拟串口演示 | `PA11(DM)` / `PA12(DP)`，设备模式 |

---

## 功能概览

系统整体是一条 **“视觉采集 → 平衡控制 → 电机执行”** 的闭环：

1. **Maxican（视觉）**：`USART1` 接收相机发送的 `AA 55` 二进制帧，解析出钢球位置 `position_cm`、像素速度 `velocity_pixel_s`、置信度 `confidence` 与帧时间戳，发布 Topic `ball_measurement` 并写入 `BallMailbox`。
2. **BalanceController（控制）**：以 50 Hz（20 ms）周期读取最新测量值，按任务的 PID 控制器计算目标倾角，通过 `ZdtX42s` 驱动电机。
3. **ZdtX42s（执行）**：经 `USART3` 以 TTL/RS485 自由协议向 X42S 闭环步进电机下发限速绝对位置命令。
4. **OperatorPanel（界面）**：轮询按键并刷新 LCD，显示任务、状态、已用时间、球位置、视觉新鲜度、置信度与故障。

### 比赛任务模式（T3–T6）

| 任务 | 行为 | 时间上限 |
| --- | --- | --- |
| T3 | 控制钢球从 O 点到 `+5 cm`，稳定后反向运行至 `-5 cm` | 5 s |
| T4 | 外部底盘从 A 行驶到 B 时，保持钢球在 O 点 | 8 s |
| T5 | 外部底盘顺时针完成一圈时，保持钢球在 O 点 | 30 s |
| T6 | 外部底盘顺时针完成一圈时，保持钢球在 `task6_target_cm` 位置 | 30 s |

### 操作按键

- **PA4**：任务未运行时依次选择 T3 / T4 / T5 / T6。
- **PA5**：启动当前任务；运行中按下则中止（保留 LCD 已用时间）。
- **PC13**：
  - VIEW 下短按回单圈零点，长按 1 s 设零点；
  - T6 未运行时短按使目标 `R + 1 cm`，长按 1 s 使 `R - 1 cm`（T6 运行中禁止修改）。

三个按键均为内部上拉、低电平有效，软件消抖 60 ms。

---

## 电机控制（bujin_motor）

`ZdtX42s` 使用 **TTL/RS485 自由协议**（而非参考工程 `ZDTMotor` 的 CAN 帧），通信端口为 `USART3`，默认地址 `1`，默认校验 `0x6B`。驱动核心命令包括：

| API | TTL 命令 | 作用 |
| --- | --- | --- |
| `Enable(bool)` | `F3 AB enable sync` | 使能 / 释放电机 |
| `StopImmediately()` | `FE 98 sync` | 安全停止 |
| `SetCurrentPositionAsZero()` | `0A 6D` | 将当前角度设为零点 |
| `MoveToAbsoluteAngleDirect(...)` | `FB ...` | 直通限速绝对位置（动态平衡） |
| `MoveToAbsoluteAngle(...)` | `FD ...` | 梯形曲线绝对位置（VIEW / 回零） |
| `ReadRealtimeAngle(...)` | `36` | 读取有符号多圈编码器角度 |
| `ReadMotorConfig(...)` | `42 6C` | 只读查询 Emm 电机配置并同步每圈脉冲数 |

安全机制：
- 平衡状态下仅允许 `BalanceController` 线程访问 `USART3`。
- 视觉数据无效 / 低置信度 / 超时 / 超出摆杆范围（默认 `|x_cm| > 12.5`）会调用 `StopImmediately()` 并中止任务。
- 上电进入 `VIEW` 后，控制器每秒尝试一次只读配置查询；只有完整 Emm 响应通过校验并同步每圈脉冲数后，才允许使能与运动。

---

## 平衡控制参数

平衡参数在 `User/app_main.cpp` 的 `BalanceControllerConfig` 中配置（编译期），分为 T3 正负两段及 T4 / T5 / T6 五组 PID。每个配置文件包含：

| 字段 | 说明 |
| --- | --- |
| `position_gain_degrees_per_cm` | 位置比例增益（度/厘米） |
| `integral_gain_degrees_per_cm_s` | 位置积分增益（度/(厘米·秒)） |
| `integral_limit_degrees` | 积分项输出限幅 |
| `velocity_gain_degrees_per_pixel_s` | 速度反馈增益（乘数为 pixel/s） |
| `derivative_gain_degrees_per_cm_s` | 微分增益 |
| `min_angle_degrees` / `max_angle_degrees` | 输出角度限幅 |
| `minimum_command_delta_degrees` | 目标角度变化小于该值则不发命令 |
| `min_confidence` | 视觉最低置信度 |

实际任务目标：T3 正 / T3 负 / T4 / T5 / T6 的 `task6_target_cm`（默认 `3.0 cm`）等。

常用全局参数：

| 字段 | 默认 | 说明 |
| --- | --- | --- |
| `control_period_ms` | 20 | 控制周期（50 Hz） |
| `vision_timeout_ms` | 200 | 视觉数据超时阈值 |
| `invalid_measurement_grace_ms` | 500 | 连续无效帧允许重捕获的宽限时间 |
| `invalid_measurement_neutralize_ms` | 100 | 无效时先回到水平偏置角的时间 |
| `max_abs_position_cm` | 13.5 | 位置绝对值有效上限 |

### 运行时调参（Ozone / 调试邮箱）

`g_motor_debug_mailbox` 允许在不重新编译的情况下调参。仅当处于 `VIEW/READY`、无活动运动、写入解锁值 `0x58423432`，且置 `operation=7` 并递增 `request_sequence` 时受理。先设 `control_profile` 选择 PID 组（0=T3+、1=T3-、2=T4、3=T5、4=T6），再写入以下字段：

| 字段 | 允许范围 |
| --- | --- |
| `control_angle_offset_degrees` | -10..+10 度 |
| `control_position_gain_degrees_per_cm` | -10..+10 度/厘米 |
| `control_integral_gain_degrees_per_cm_s` | -2..+2 度/(厘米·秒) |
| `control_integral_limit_degrees` | 0..10 度 |
| `control_velocity_gain_degrees_per_pixel_s` | -0.5..+0.5 |
| `control_angle_limit_degrees` | 1..20 度 |
| `control_minimum_command_delta_degrees` | 0..5 度 |

操作 7 不会使能、置零、运动或写入 Flash；复位后恢复编译期配置。任一值超出范围则整组不生效。

---

## 串口终端与 Topic

- 终端使用 **USART2**（`PA2`→模块 RX、`PA3`→模块 TX，115200-8N1，共地）。
- `ball latest`：打印一次最新接收帧。
- `ball show <time_ms> <interval_ms>`：限时循环打印（总时长 ≤ 10000 ms，间隔 20–1000 ms）。

LibXR Topic：
- `ball_measurement`：`Maxican::BallReceiver` 每收到一帧校验通过的 `AA 55` 数据发布。
- `balance_state`：`BalanceController` 在状态或控制量更新后发布。

---

## 构建与烧录

项目使用 CMake + Ninja + `starm-clang` 交叉工具链。

### Debug 构建

```bash
cmake --preset Debug
cmake --build build/Debug --target 26diansai --parallel 4
```

### Release 构建

```bash
cmake --preset Release
cmake --build build/Release --target 26diansai --parallel 4
```

构建产物位于 `build/<preset>/26diansai.elf`，可通过 ST-Link / Ozone 等工具烧录到 STM32F407VGT6。

> 依赖说明：`Middlewares/Third_Party/LibXR` 为 git 子模块（`https://gitee.com/jiu-xiao/libxr`），首次拉取需执行 `git submodule update --init --recursive`。

### Ozone 自动化测试宏（可选）

`CMakeLists.txt` 提供仅用于 Ozone 在线调试的自动化测试开关（默认关闭），用于复位后自动选择并启动任务、或在无运动情况下循环显示模式：

| CMake 选项 | 说明 |
| --- | --- |
| `OZONE_AUTOMATED_T3_TEST` | 复位后自动进入并启动 T3 |
| `OZONE_AUTOMATED_RUN_TASK` | 自动选定并运行 T3/T4/T5/T6（0=禁用） |
| `OZONE_AUTOMATED_IDLE_MODE_SELECTIONS` | 仅选择模式不启动（0..4） |
| `OZONE_AUTOMATED_IDLE_MODE_CYCLE_TEST` | 循环切换 VIEW/T3/T4/T5/T6 且不驱动电机 |
| `OZONE_RELEVEL_BEFORE_AUTOMATED_RUN` | 自动运行前将电机恢复到既有零点 |
| `OZONE_AUTOMATED_ZERO_RECOVERY_STEPS` | 有界 ±3 步的 20° 零点纠偏 |

这些开关之间互斥，`cmake` 配置阶段会做合法性校验。

---

## 调试建议

调整平衡控制参数的推荐顺序（见 `bu_task/README.md`）：

1. 使用小增益和小角度限幅，确认 `position_gain_degrees_per_cm` 的正负方向。
2. 通过 `angle_offset_degrees` 补偿电机零点与机械水平偏差。
3. 逐步增大位置增益，直到球稳定且不过度振荡。
4. 针对目标外静止误差，加入较小的积分增益并用 `control_integral_limit_degrees` 限制积分输出。
5. 仅在标定像素速度关系后加入较小的速度增益。
6. 将 `vision_timeout_ms`、置信度阈值、电机速度、加速度与输出限幅保持在安全范围内。

> 安全注意：先将摆杆调至机械水平并设为电机零点，再以低速度、小角度验证运动方向；若球被推向反方向，反转 `positive_direction` 或位置增益符号。

---

## 架构图演示

使用浏览器打开 [26diansai 运行时架构图](.agents/26diansai-runtime-architecture.html) 查看交互式 Archify 架构图。

---

## 相关文档

- `bu_task/README.md` — 平衡任务流程、按键、终端命令与调参说明
- `bujin_motor/READMA.md` — ZDT X42S 串口驱动协议与使用示例
- `task_plan.md` / `progress.md` / `findings.md` — 电机驱动适配工作的任务规划与记录
