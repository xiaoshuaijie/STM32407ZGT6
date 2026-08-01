# ZDT X42S 串口驱动

`zdt_x42s.hpp/.cpp` 是车载平衡摆杆使用的 ZDT X42S 闭环步进电机 STM32 LibXR 串口驱动。它使用 TTL/RS485 自由协议，而不是参考 `ZDTMotor.hpp` 工程使用的 CAN 帧传输。

## 硬件连接

- MCU 接口：USART3，PD8 为 TX、PD9 为 RX，115200 波特率、8 数据位、无校验、1 停止位。
- 电机通信端口功能：`UART_FUN`。
- 默认地址：`1`。
- 默认校验：固定 `0x6B`。驱动也可通过 `Config::checksum` 使用用户手册中的 XOR 校验。
- 上电初始换算值按 Emm 固件、1.8 度电机、16 微步配置，即每圈 3200 脉冲。`BalanceController` 会在 `VIEW` 中读取实物配置；完整响应校验通过后，驱动自动按电机类型和微步数更新每圈脉冲数。同步成功前控制器禁止使能和运动，因此无需为常规 Emm 电机手工修改该值。

## 已实现命令

| API | TTL 命令 | 作用 |
| --- | --- | --- |
| `Enable(bool)` | `F3 AB enable sync` | 使能或释放电机。 |
| `StopImmediately()` | `FE 98 sync` | 安全停止当前运动。 |
| `SetCurrentPositionAsZero()` | `0A 6D` | 将当前摆杆角度定义为电机零点。 |
| `MoveToAbsoluteAngleDirect(...)` | `FB direction speed angle mode sync` | X 固件直通限速绝对位置运动，适合连续更新的动态平衡目标。 |
| `MoveToAbsoluteAngle(...)` | `FD direction speed acceleration position mode sync` | 梯形曲线绝对位置运动，适合 VIEW、回零和恢复等点到点动作。 |
| `ReadRealtimeAngle(...)` | `36` | 读取有符号多圈编码器角度。 |
| `ReadMotorConfig(...)` | `42 6C` | 只读查询 Emm 电机类型、细分、地址和通讯配置；完整校验通过后自动同步角度换算使用的每圈脉冲数，不使能或运动电机。 |

所有命令均通过项目的 `LibXR::UART` 串行执行。驱动会等待确认帧、校验地址/功能码/校验字节，并将手册规定的状态字节映射为 `LibXR::ErrorCode`。正常平衡控制仅允许 `BalanceController` 线程访问 USART3，不应由其他线程同时发送电机命令。

`ReadMotorConfig()` 只在 33 字节 Emm 响应的地址、功能码、长度、参数数量、校验字节、电机类型和微步数全部有效时更新 `pulses_per_revolution`。失败或 X 固件响应不会改变原换算值；X 固件会返回 `NOT_SUPPORT`，控制器不会向其发送 Emm 运动帧。

## 最小使用示例

```cpp
BujinMotor::ZdtX42s motor(usart3);
motor.Enable(true);
motor.SetCurrentPositionAsZero();
motor.MoveToAbsoluteAngle(5.0F, 300, 100);
```

控制器使用 `angle_offset_degrees`、输出角度限幅、命令死区和 200 ms 视觉超时保护。应先以低速度、小角度确认摆杆运动方向；若钢球被推离目标位置，反转 `Config::positive_direction` 或位置增益符号。

## 协议范围

X42S 手册还规定了校准、回零、速度控制、参数写入、系统状态读取、多电机报文、CRC8 和 CAN 协议。这些功能不属于当前平衡摆杆所需的最小接口，且需要额外的响应长度处理，因此未并入此驱动。不要在本驱动运行时并发发送这些原始帧。

## 安全检查

1. 使用小输出角度确认摆杆运动方向正确。
2. 调参前，将机械水平的摆杆设为零点。
3. 从较小的 `position_gain_degrees_per_cm` 和输出限幅开始调试。
4. 验证无效、低置信度和丢失的 `$BALL` 帧都会触发立即停止。
5. 在 Ozone 中确认 `config_valid=1`，并核对自动读取的电机类型、微步数、每圈脉冲数、地址和校验模式。
