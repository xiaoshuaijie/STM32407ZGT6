# 平衡滚球控制

本模块负责摆杆平衡任务流程，并且是运行时唯一可以通过 USART3 向 X42S 发送电机命令的线程。

## 比赛任务模式

| LCD 任务 | 行为 | 时间上限 |
| --- | --- | --- |
| T3 | 控制钢球从 O 点到 `+5 cm`，稳定后反向运行并稳定在 `-5 cm`。 | 5 s |
| T4 | 外部底盘从 A 行驶到 B 时，保持钢球在 O 点。 | 8 s |
| T5 | 外部底盘顺时针完成一圈时，保持钢球在 O 点。 | 30 s |
| T6 | 外部底盘顺时针完成一圈时，保持钢球在配置的 `task6_target_cm` 位置。 | 30 s |

底盘和红外循迹控制不属于本模块。按下 PA5 时应同时启动平衡计时与底盘；任务运行中再次按 PA5 会中止平衡任务，并保留 LCD 上显示的已用时间。

## 操作界面

- PA4：任务未运行时，依次选择 T3、T4、T5、T6。
- PA5：启动当前任务；任务运行时按下则中止。
- PC13：在 VIEW 下短按回单圈零点、长按 1 s 设置单圈零点；在 T6 未运行时短按使 `R + 1 cm`，长按 1 s 使 `R - 1 cm`。T6 运行期间不允许修改 `R`。
- 三个按键均使用内部上拉、低电平有效，软件消抖时间为 60 ms。
- 1.14 英寸 LCD 每秒刷新两次，显示当前任务、运行状态、已用时间、钢球实测/目标位置、视觉新鲜度、置信度和故障。

## Topic 与终端

- `ball_measurement`：`Maxican::BallReceiver` 每收到一帧校验通过的 `AA 55` 二进制数据后发布。Topic 时间戳为本机接收时刻，原始 `frame_time_ms` 保留在 payload 内。
- `balance_state`：`BalanceController` 在状态或控制量更新后发布。
- USART2 终端命令 `ball latest`：打印一次最新接收帧。
- USART2 终端命令 `ball show <time_ms> <interval_ms>`：限时循环打印。总时间最大为 10000 ms，间隔自动限制在 20 至 1000 ms。
- 串口终端使用 USART2 的 PA2(TX) 和 PA3(RX)，115200、8N1。与 USB 转串口模块交叉连接：PA2 接模块 RX，PA3 接模块 TX，并共地。

## 参数调试

上电进入 `VIEW` 后，控制器每秒尝试一次只读 `42 6C` 配置查询。只有完整 Emm 响应通过校验并同步实际每圈脉冲数后，PA5 自检、比赛任务和 Ozone 运动命令才允许使能电机。X 固件及读取失败会保持禁止运动状态。将摆杆调至机械水平后，使用 PA5 启动任务，使控制器在任务开始时建立电机坐标零点。

Ozone 可通过 `g_motor_debug_mailbox` 在不重新编译的情况下调参。该入口只在 `VIEW/READY`、无活动运动且写入解锁值 `0x58423432` 时受理；将 `operation=7`，再递增 `request_sequence` 即可一次性应用以下 RAM 参数：

先设置 `control_profile` 选择本次修改的独立 PID：`0=T3 +5 cm`、`1=T3 -5 cm`、`2=T4`、`3=T5`、`4=T6`。`operation=7` 只修改被选中的一组，不会覆盖其他任务。

| 字段 | 允许范围 |
| --- | --- |
| `control_angle_offset_degrees` | `-10..+10` 度 |
| `control_position_gain_degrees_per_cm` | `-10..+10` 度/厘米 |
| `control_integral_gain_degrees_per_cm_s` | `-2..+2` 度/(厘米*秒) |
| `control_integral_limit_degrees` | `0..10` 度，限制积分项最大输出 |
| `control_velocity_gain_degrees_per_pixel_s` | `-0.5..+0.5` |
| `control_angle_limit_degrees` | `1..20` 度，对称应用到正负限幅 |
| `control_minimum_command_delta_degrees` | `0..5` 度 |

任一值不是有限数或超出范围时，请求返回 `OUT_OF_RANGE`，全部参数均不改变；成功后 `control_tuning_valid=1`。操作 7 不使能、不置零、不运动，也不写入电机或 MCU Flash，复位后恢复 `User/app_main.cpp` 的编译配置。

按以下顺序调整控制参数：

1. 使用小增益和小角度限幅，确认 `position_gain_degrees_per_cm` 的正负方向。
2. 通过 `angle_offset_degrees` 补偿电机零点与机械水平之间的偏差。
3. 逐步增加位置增益，直至钢球能够稳定且不过度振荡。
4. 针对静止在目标外的稳态误差，加入小的 `integral_gain_degrees_per_cm_s`，并用 `control_integral_limit_degrees` 限制积分输出。
5. 仅在测得像素速度标定关系后，再加入较小的速度增益。
6. 将 `vision_timeout_ms`、置信度阈值、电机速度、加速度和输出限幅保持在安全范围内。

无效、低置信度、超时或超出摆杆范围（默认 `|x_cm| > 12.5`）的视觉数据都会调用 `StopImmediately()`，然后控制器返回操作状态。
