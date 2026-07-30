# ZDTMotor

面向 LibXR/XRobot 的 ZDT XS 第二代闭环步进电机 **Emm 固件 CAN** 模块。
协议实现依据资料包中的《ZDT_X42S 第二代闭环步进电机用户手册
V1.0.4（2026-04-01）》及官方 `Emm_V5.c/.h` 示例。

## 硬件要求

构造函数需要一个以 `can_name`、`can`、`can1` 或 `CAN1` 注册的
`LibXR::CAN`。STM32 FDCAN 对象需要同时以基类类型注册：

```cpp
LibXR::CAN &can1 = fdcan1;
peripherals.Register(
    LibXR::Entry<LibXR::CAN>({can1, {"can1", "can", "CAN1"}}));
```

## 构造参数

- `can_name`：CAN 硬件别名，默认 `"can1"`。
- `motor_id`：电机地址，默认 `1`；设为 `0` 时接收所有地址回包。
- `bitrate`：CAN 仲裁速率，默认 `500000`。
- `configure_can`：是否在构造时调用 `CAN::SetConfig()`，默认 `false`。
- `pulses_per_unit`：业务层目标增量到位置脉冲数的换算比例。
- `default_speed_rpm`：业务层相对位置命令速度。
- `default_acceleration`：业务层默认加速度档位。
- `duty_max_speed_rpm`：`SetOpenLoopDuty(1)` 对应的最大速度。
- `checksum_mode`：`0/1/2` 分别为固定 `0x6B`、XOR、CRC8。
- `receive_all_motor_ids`：是否接收总线上所有电机回包。

## 能力覆盖

### 触发动作

- `TriggerEncoderCalibration()`：编码器线性化校准。
- `ResetMotor()`：重启第二代电机。
- `ResetCurrentPositionToZero()`：当前位置清零。
- `ResetProtection()`：解除堵转、过热和过流保护。
- `RestoreFactorySettings()`：恢复出厂设置。

### 运动控制

- `Enable()` / `Enable(bool, bool)`：使能、失能及同步缓存。
- `Velocity()`：Emm 速度模式。
- `Position()`：相对上次目标、绝对坐标、相对当前位置三种位置模式。
- `ConfigureFastPosition()` + `FastPosition()`：V2.0.0 固件新增快速位置模式。
- `Stop()`：立即停止或缓存停止。
- `SynchronousMotion()`：默认以广播地址触发多机同步运动。
- `ConfigurePowerOnVelocity()` / `ClearPowerOnVelocity()`：Emm 上电自动运行。

速度模式和位置模式按手册限制为 `0..3000 RPM`。`FastPosition()` 参数为
有符号 `int32_t` 脉冲数，直接按大端补码发送。

### 原点回零

- 单圈就近、单圈方向、无限位碰撞、限位开关、绝对坐标零点、掉电位置六种模式。
- 设置单圈零点、触发回零、强制中断、读取和修改完整回零参数。
- 读取/修改碰撞回零返回角度。
- 完整解析编码器、校准、回零、过热和过流状态位。

### 系统信息

`ReadSystemParam()` 和 `AutoReturnSystemParam()` 支持：

- 固件/硬件版本、相电阻/相电感。
- 总线电压/电流、相电流、编码器原始值和线性化值。
- 实时脉冲、输入脉冲、目标位置、设定位置、速度、位置和位置误差。
- 电池电压、温度、电机状态、回零状态、组合状态和引脚状态。
- `ReadSystemStateParams()` 一次读取完整 Emm 系统状态。

对应解析器包括 `DecodeFirmwareHardwareVersion()`、
`DecodeSignedMagnitude32()`、`DecodeSpeed()`、`DecodeEmmSystemState()` 等。
Emm 位置值可通过 `SignedMagnitude32::EmmDegrees()` 换算为角度。

### 驱动参数

- ID、细分、掉电标志、电机类型、固件类型（含 Emm 狂暴模式）。
- 第二代开环/闭环模式；旧 X42 辅助码兼容接口为
  `ModifyControlModeLegacy()`。
- 正方向、按键锁、速度值缩小 10 倍输入。
- 开环电流、闭环堵转最大电流、Emm PID。
- DMX512、位置到达窗口、过热过流阈值、心跳保护、积分限幅。
- 广播读取 ID、参数修改锁定等级。
- `ReadMotorConfigParams()` / `ModifyMotorConfigParams()` 读写完整 Emm
  驱动配置，包括端口复用、通信速率、协议、应答方式和堵转检测参数。

`Dmx512Params::start_channel` 为兼容旧 API 保留；在第二代手册中该字段实际表示
DMX512 控制台的“总通道数”，也可通过 `TotalChannels()` 读取。

## CAN 帧与校验

- 固定使用 29 位扩展帧，ID 为 `(motor_id << 8) | packet_index`。
- 每帧 `data[0]` 都重复功能码，其余 7 字节承载连续命令数据。
- 超过 8 字节的命令和回包自动拆包/重组，包序从 `0` 开始。
- 支持固定 `0x6B`、XOR 和手册 CRC8 三种校验，并校验完整串行形式中的
  `motor_id + command + payload`。
- `Response` 保存完整重组数据、最终包号、帧数、校验结果和应答状态。
- `SetResponseCallback()` 可在完整回包就绪时接收通知；回调可能运行在 CAN ISR。

`RxCount()` 统计接收帧数，`ResponseCount()` 统计完整回包数；
`ChecksumErrorCount()`、`SequenceErrorCount()` 和 `OverflowCount()` 用于诊断。

## 多电机命令

`CommandBatch` 实现手册 `0xAA` 多电机包装格式，并自动生成每条内层命令及
外层命令的校验码：

```cpp
ZDTMotor::CommandBatch<> batch;
batch.AppendPosition(2, ZDTMotor::Direction::CCW, 1500, 8, 32000,
                     ZDTMotor::MotionMode::RELATIVE, false);
batch.AppendPosition(3, ZDTMotor::Direction::CW, 1000, 10, 64000,
                     ZDTMotor::MotionMode::ABSOLUTE, true);
batch.AppendReadSystemParam(4, ZDTMotor::SystemParam::POSITION);
motor.SendBatch(batch);
```

批量对象的校验模式必须与 `ZDTMotor` 当前校验模式一致。除便利方法外，
`AppendRaw()` 可装入任意受支持命令。要接收多个地址的读取结果，请将
`receive_all_motor_ids` 设为 `true`，或使用地址 `0` 的接收实例。

## 业务控制层

- `SetTargetDelta()` + `Update()`：按 `pulses_per_unit` 发送相对位置命令。
- `SetOpenLoopDuty()` + `Update()`：把 `[-1, 1]` 映射为正反向速度命令。
- `Disable()` / `Relax()`：停止业务命令并失能电机。
- `GetFeedback()`：读取业务层已发送的目标增量和累计脉冲反馈。

## 测试

主机协议测试覆盖手册位置模式示例、快速位置、XOR/CRC8、多电机 `0xAA`
示例、长回包重组、完整状态解析和校验失败：

```powershell
cmake -S Modules/ZDTMotor/tests -B build/ZDTMotorTests -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build/ZDTMotorTests
ctest --test-dir build/ZDTMotorTests --output-on-failure
```

独立克隆模块仓库时，配置阶段另传
`-DLIBXR_ROOT=<LibXR 仓库根目录>`。
