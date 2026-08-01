#include "zdt_x42s.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace BujinMotor
{
namespace
{

// 读取实时位置响应时，第三字节表示后续位置值的符号。
constexpr uint8_t kPositiveSign = 0;
constexpr uint8_t kNegativeSign = 1;

// FD 位置命令的模式和同步执行字段。该驱动固定使用绝对位置、立即执行。
constexpr uint8_t kAbsolutePositionMode = 1;
constexpr uint8_t kImmediateExecution = 0;

// F3、FE、0A 命令中由 X42S 协议规定的固定辅助字节。
constexpr uint8_t kEnableAuxiliaryCode = 0xAB;
constexpr uint8_t kStopAuxiliaryCode = 0x98;
constexpr uint8_t kSetZeroAuxiliaryCode = 0x6D;
constexpr uint8_t kSetSingleTurnZeroAuxiliaryCode = 0x88;
constexpr uint8_t kCalibrateEncoderAuxiliaryCode = 0x45;
constexpr uint8_t kResetMotorAuxiliaryCode = 0x97;
constexpr uint8_t kClearClogProtectionAuxiliaryCode = 0x52;
constexpr uint8_t kRestoreFactorySettingsAuxiliaryCode = 0x5F;
constexpr uint8_t kInterruptHomingAuxiliaryCode = 0x48;
constexpr uint8_t kSynchronizeMotionAuxiliaryCode = 0x66;
constexpr uint8_t kConfigureHomingAuxiliaryCode = 0xAE;

// Direction 的枚举值恰好等于协议要求的方向字节，集中转换可避免散落的
// static_cast，并明确这是一次协议编码。
uint8_t ToByte(Direction direction)
{
  return static_cast<uint8_t>(direction);
}

// 目标角度为负时，脉冲数仍以无符号绝对值编码，方向需要相对于机构正方向
// 翻转。这里只处理两个合法方向枚举值。
Direction Opposite(Direction direction)
{
  return direction == Direction::Clockwise ? Direction::CounterClockwise
                                            : Direction::Clockwise;
}

// 将数值按 X42S 协议要求写为高字节在前的 16 位整数。
void WriteBigEndian16(uint8_t* destination, uint16_t value)
{
  destination[0] = static_cast<uint8_t>(value >> 8U);
  destination[1] = static_cast<uint8_t>(value);
}

// 将脉冲数按 X42S 协议要求写为高字节在前的 32 位整数。
void WriteBigEndian32(uint8_t* destination, uint32_t value)
{
  destination[0] = static_cast<uint8_t>(value >> 24U);
  destination[1] = static_cast<uint8_t>(value >> 16U);
  destination[2] = static_cast<uint8_t>(value >> 8U);
  destination[3] = static_cast<uint8_t>(value);
}

// 从高字节在前的四个字节恢复无符号 32 位数值。
uint32_t ReadBigEndian32(const uint8_t* source)
{
  return (static_cast<uint32_t>(source[0]) << 24U) |
         (static_cast<uint32_t>(source[1]) << 16U) |
         (static_cast<uint32_t>(source[2]) << 8U) |
         static_cast<uint32_t>(source[3]);
}

uint16_t ReadBigEndian16(const uint8_t* source)
{
  return static_cast<uint16_t>((static_cast<uint16_t>(source[0]) << 8U) |
                               static_cast<uint16_t>(source[1]));
}

constexpr int64_t SignedPositionCounts(uint8_t sign, uint32_t magnitude)
{
  return sign == kNegativeSign ? -static_cast<int64_t>(magnitude)
                               : static_cast<int64_t>(magnitude);
}

constexpr uint32_t PulsesPerRevolution(uint8_t motor_type, uint8_t microstep)
{
  const uint32_t full_steps = motor_type == 0x19U ? 200U :
                              motor_type == 0x32U ? 400U : 0U;
  const uint32_t effective_microstep = microstep == 0U ? 256U : microstep;
  return full_steps * effective_microstep;
}

// The existing application-level acceleration parameter follows the Emm
// 0..255 ramp setting.  X firmware uses RPM/s.  For non-zero values the Emm
// manual defines one RPM increment every (256 - acc) * 50 us, which converts
// directly to this rate.  A zero Emm setting means immediate start, so use a
// deliberately high but in-range X acceleration instead of incorrectly
// converting it to a very slow ramp.
uint16_t XAccelerationRpmPerSecond(uint8_t acceleration)
{
  constexpr uint32_t kImmediateAccelerationRpmPerSecond = 30000U;
  constexpr uint32_t kMaximumAccelerationRpmPerSecond = 65535U;
  if (acceleration == 0U)
  {
    return static_cast<uint16_t>(kImmediateAccelerationRpmPerSecond);
  }

  const uint32_t denominator = (256U - static_cast<uint32_t>(acceleration)) * 50U;
  const uint32_t rounded_rate = (1000000U + denominator / 2U) / denominator;
  return static_cast<uint16_t>(
      std::min(rounded_rate, kMaximumAccelerationRpmPerSecond));
}

static_assert(PulsesPerRevolution(0x19U, 16U) == 3200U);
static_assert(PulsesPerRevolution(0x19U, 64U) == 12800U);
static_assert(PulsesPerRevolution(0x19U, 32U) == 6400U);
static_assert(PulsesPerRevolution(0x32U, 16U) == 6400U);
static_assert(PulsesPerRevolution(0x19U, 0U) == 51200U);
static_assert(SignedPositionCounts(kPositiveSign, 65536U) == 65536);
static_assert(SignedPositionCounts(kNegativeSign, 65536U) == -65536);
static_assert(SignedPositionCounts(kPositiveSign, 131072U) == 131072);

}  // namespace

// 默认构造函数委托给完整构造函数，确保默认值只有 Config 一处定义。
ZdtX42s::ZdtX42s(LibXR::UART& uart) : ZdtX42s(uart, Config{}) {}

// UART 由调用方管理其生命周期；Config 按值保存，创建后不会受外部变量修改。
ZdtX42s::ZdtX42s(LibXR::UART& uart, Config config) : uart_(uart), config_(config) {}

LibXR::ErrorCode ZdtX42s::Enable(bool enabled)
{
  // 先拦截无效配置，避免向地址 0 或没有超时保护的链路发出命令。
  if (!HasValidConfig())
  {
    return LibXR::ErrorCode::ARG_ERR;
  }

  std::array<uint8_t, 6> frame = {
      // 帧格式：地址、F3、AB、使能标志、同步方式、校验。
      config_.address,
      kEnableCommand,
      kEnableAuxiliaryCode,
      static_cast<uint8_t>(enabled),
      kImmediateExecution,
      0,
  };
  // AppendChecksum 只改写预留的最后一个字节。
  AppendChecksum(frame.data(), frame.size());
  return SendAndExpectAcknowledgement(frame.data(), frame.size(), kEnableCommand);
}

LibXR::ErrorCode ZdtX42s::StopImmediately()
{
  if (!HasValidConfig())
  {
    return LibXR::ErrorCode::ARG_ERR;
  }

  std::array<uint8_t, 5> frame = {
      // 帧格式：地址、FE、98、同步方式、校验。
      config_.address,
      kStopCommand,
      kStopAuxiliaryCode,
      kImmediateExecution,
      0,
  };
  AppendChecksum(frame.data(), frame.size());
  return SendAndExpectAcknowledgement(frame.data(), frame.size(), kStopCommand);
}

LibXR::ErrorCode ZdtX42s::SetCurrentPositionAsZero()
{
  if (!HasValidConfig())
  {
    return LibXR::ErrorCode::ARG_ERR;
  }

  std::array<uint8_t, 4> frame = {
      // 帧格式：地址、0A、6D、校验。该操作会改变电机内部的零点基准。
      config_.address,
      kSetZeroCommand,
      kSetZeroAuxiliaryCode,
      0,
  };
  AppendChecksum(frame.data(), frame.size());
  return SendAndExpectAcknowledgement(frame.data(), frame.size(), kSetZeroCommand);
}

LibXR::ErrorCode ZdtX42s::SetSingleTurnHomingZero(bool save)
{
  if (!HasValidConfig())
  {
    return LibXR::ErrorCode::ARG_ERR;
  }

  std::array<uint8_t, 5> frame = {
      config_.address,
      kSetSingleTurnZeroCommand,
      kSetSingleTurnZeroAuxiliaryCode,
      static_cast<uint8_t>(save),
      0,
  };
  AppendChecksum(frame.data(), frame.size());
  return SendAndExpectAcknowledgement(frame.data(), frame.size(),
                                      kSetSingleTurnZeroCommand);
}

LibXR::ErrorCode ZdtX42s::InterruptHoming()
{
  return SendSimpleAction(kInterruptHomingCommand, kInterruptHomingAuxiliaryCode);
}

LibXR::ErrorCode ZdtX42s::CalibrateEncoder()
{
  return SendSimpleAction(kCalibrateEncoderCommand, kCalibrateEncoderAuxiliaryCode);
}

LibXR::ErrorCode ZdtX42s::ResetMotor()
{
  return SendSimpleAction(kResetMotorCommand, kResetMotorAuxiliaryCode);
}

LibXR::ErrorCode ZdtX42s::ClearClogProtection()
{
  return SendSimpleAction(kClearClogProtectionCommand,
                          kClearClogProtectionAuxiliaryCode);
}

LibXR::ErrorCode ZdtX42s::RestoreFactorySettings()
{
  return SendSimpleAction(kRestoreFactorySettingsCommand,
                          kRestoreFactorySettingsAuxiliaryCode);
}

LibXR::ErrorCode ZdtX42s::SetVelocity(Direction direction, uint16_t speed_rpm,
                                       uint8_t acceleration, bool sync)
{
  if (!HasValidConfig() || speed_rpm > 5000U)
  {
    return LibXR::ErrorCode::ARG_ERR;
  }
  if (firmware_family_ != FirmwareFamily::Emm)
  {
    return LibXR::ErrorCode::NOT_SUPPORT;
  }

  std::array<uint8_t, 8> frame = {
      config_.address, kVelocityCommand, ToByte(direction), 0, 0,
      acceleration, static_cast<uint8_t>(sync), 0,
  };
  WriteBigEndian16(&frame[3], speed_rpm);
  AppendChecksum(frame.data(), frame.size());
  return SendAndExpectAcknowledgement(frame.data(), frame.size(), kVelocityCommand);
}

LibXR::ErrorCode ZdtX42s::MoveToPosition(Direction direction, uint16_t speed_rpm,
                                          uint8_t acceleration, uint32_t pulses,
                                          PositionMode mode, bool sync)
{
  if (!HasValidConfig() || speed_rpm > 5000U ||
      static_cast<uint8_t>(mode) >
          static_cast<uint8_t>(PositionMode::RelativeToCurrentPosition))
  {
    return LibXR::ErrorCode::ARG_ERR;
  }
  if (firmware_family_ != FirmwareFamily::Emm)
  {
    return LibXR::ErrorCode::NOT_SUPPORT;
  }

  std::array<uint8_t, 13> frame = {
      config_.address, kPositionCommand, ToByte(direction), 0, 0, acceleration,
      0, 0, 0, 0, static_cast<uint8_t>(mode), static_cast<uint8_t>(sync), 0,
  };
  WriteBigEndian16(&frame[3], speed_rpm);
  WriteBigEndian32(&frame[6], pulses);
  AppendChecksum(frame.data(), frame.size());
  return SendAndExpectAcknowledgement(frame.data(), frame.size(), kPositionCommand);
}

LibXR::ErrorCode ZdtX42s::ConfigureQuickPosition(uint16_t speed_rpm,
                                                  uint8_t acceleration,
                                                  PositionMode mode, bool sync)
{
  if (!HasValidConfig() || speed_rpm > 5000U ||
      static_cast<uint8_t>(mode) >
          static_cast<uint8_t>(PositionMode::RelativeToCurrentPosition))
  {
    return LibXR::ErrorCode::ARG_ERR;
  }
  if (firmware_family_ != FirmwareFamily::Emm)
  {
    return LibXR::ErrorCode::NOT_SUPPORT;
  }

  std::array<uint8_t, 8> frame = {
      config_.address, kQuickPositionParamsCommand, 0, 0, acceleration,
      static_cast<uint8_t>(mode), static_cast<uint8_t>(sync), 0,
  };
  WriteBigEndian16(&frame[2], speed_rpm);
  AppendChecksum(frame.data(), frame.size());
  return SendAndExpectAcknowledgement(frame.data(), frame.size(),
                                      kQuickPositionParamsCommand);
}

LibXR::ErrorCode ZdtX42s::MoveQuickPosition(int32_t pulses)
{
  if (!HasValidConfig())
  {
    return LibXR::ErrorCode::ARG_ERR;
  }
  if (firmware_family_ != FirmwareFamily::Emm)
  {
    return LibXR::ErrorCode::NOT_SUPPORT;
  }

  std::array<uint8_t, 7> frame = {
      config_.address, kQuickPositionCommand, 0, 0, 0, 0, 0,
  };
  WriteBigEndian32(&frame[2], static_cast<uint32_t>(pulses));
  AppendChecksum(frame.data(), frame.size());
  return SendAndExpectAcknowledgement(frame.data(), frame.size(),
                                      kQuickPositionCommand);
}

LibXR::ErrorCode ZdtX42s::SynchronizeMotion()
{
  return SendSimpleAction(kSynchronizeMotionCommand, kSynchronizeMotionAuxiliaryCode);
}

LibXR::ErrorCode ZdtX42s::ConfigureHoming(
    bool save, uint8_t mode, Direction direction, uint16_t speed_rpm,
    uint32_t timeout_ms, uint16_t stall_speed_rpm, uint16_t stall_current_ma,
    uint16_t stall_time_ms, bool home_on_power_up)
{
  if (!HasValidConfig() || mode > 5U)
  {
    return LibXR::ErrorCode::ARG_ERR;
  }

  std::array<uint8_t, 20> frame = {
      config_.address, kConfigureHomingCommand, kConfigureHomingAuxiliaryCode,
      static_cast<uint8_t>(save), mode, ToByte(direction), 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, static_cast<uint8_t>(home_on_power_up), 0,
  };
  WriteBigEndian16(&frame[6], speed_rpm);
  WriteBigEndian32(&frame[8], timeout_ms);
  WriteBigEndian16(&frame[12], stall_speed_rpm);
  WriteBigEndian16(&frame[14], stall_current_ma);
  WriteBigEndian16(&frame[16], stall_time_ms);
  AppendChecksum(frame.data(), frame.size());
  return SendAndExpectAcknowledgement(frame.data(), frame.size(),
                                      kConfigureHomingCommand);
}

LibXR::ErrorCode ZdtX42s::SetMotorAddress(bool save, uint8_t address)
{
  if (!HasValidConfig() || address == 0U)
  {
    return LibXR::ErrorCode::ARG_ERR;
  }
  const uint8_t payload[] = {address};
  const auto result = SendConfigurationCommand(
      kSetMotorAddressCommand, 0x4B, save, payload, sizeof(payload));
  if (result == LibXR::ErrorCode::OK)
  {
    config_.address = address;
  }
  return result;
}

LibXR::ErrorCode ZdtX42s::SetMicrostep(bool save, uint8_t microstep)
{
  if (!HasValidConfig())
  {
    return LibXR::ErrorCode::ARG_ERR;
  }
  const uint8_t payload[] = {microstep};
  return SendConfigurationCommand(kSetMicrostepCommand, 0x8A, save, payload,
                                  sizeof(payload));
}

LibXR::ErrorCode ZdtX42s::SetPowerDownFlag(bool enabled)
{
  if (!HasValidConfig())
  {
    return LibXR::ErrorCode::ARG_ERR;
  }
  std::array<uint8_t, 4> frame = {
      config_.address, kSetPowerDownFlagCommand, static_cast<uint8_t>(enabled), 0,
  };
  AppendChecksum(frame.data(), frame.size());
  return SendAndExpectAcknowledgement(frame.data(), frame.size(),
                                      kSetPowerDownFlagCommand);
}

LibXR::ErrorCode ZdtX42s::SetMotorType(bool save, bool motor_is_1p8_degree)
{
  const uint8_t payload[] = {static_cast<uint8_t>(motor_is_1p8_degree ? 25U : 50U)};
  return SendConfigurationCommand(kSetMotorTypeCommand, 0x35, save, payload,
                                  sizeof(payload));
}

LibXR::ErrorCode ZdtX42s::SetFirmwareType(bool save, bool x_firmware)
{
  const uint8_t payload[] = {static_cast<uint8_t>(x_firmware)};
  const auto result = SendConfigurationCommand(
      kSetFirmwareTypeCommand, 0x69, save, payload, sizeof(payload));
  if (result == LibXR::ErrorCode::OK)
  {
    firmware_family_ = x_firmware ? FirmwareFamily::X : FirmwareFamily::Emm;
  }
  return result;
}

LibXR::ErrorCode ZdtX42s::SetControlMode(bool save, bool closed_loop)
{
  const uint8_t payload[] = {static_cast<uint8_t>(closed_loop)};
  return SendConfigurationCommand(kSetControlModeCommand, 0x69, save, payload,
                                  sizeof(payload));
}

LibXR::ErrorCode ZdtX42s::SetOpenLoopCurrent(bool save, uint16_t current_ma)
{
  std::array<uint8_t, 2> payload{};
  WriteBigEndian16(payload.data(), current_ma);
  return SendConfigurationCommand(kSetOpenLoopCurrentCommand, 0x33, save,
                                  payload.data(), payload.size());
}

LibXR::ErrorCode ZdtX42s::SetClosedLoopCurrent(bool save, uint16_t current_ma)
{
  std::array<uint8_t, 2> payload{};
  WriteBigEndian16(payload.data(), current_ma);
  return SendConfigurationCommand(kSetClosedLoopCurrentCommand, 0x66, save,
                                  payload.data(), payload.size());
}

LibXR::ErrorCode ZdtX42s::SetPid(bool save, uint32_t proportional,
                                  uint32_t integral, uint32_t derivative)
{
  std::array<uint8_t, 12> payload{};
  WriteBigEndian32(payload.data(), proportional);
  WriteBigEndian32(payload.data() + 4U, integral);
  WriteBigEndian32(payload.data() + 8U, derivative);
  return SendConfigurationCommand(kSetPidCommand, 0xC3, save, payload.data(),
                                  payload.size());
}

LibXR::ErrorCode ZdtX42s::SetPositionWindow(bool save, uint16_t tenths_degree)
{
  std::array<uint8_t, 2> payload{};
  WriteBigEndian16(payload.data(), tenths_degree);
  return SendConfigurationCommand(kSetPositionWindowCommand, 0x07, save,
                                  payload.data(), payload.size());
}

LibXR::ErrorCode ZdtX42s::SetHeartbeatProtection(bool save, uint32_t timeout_ms)
{
  std::array<uint8_t, 4> payload{};
  WriteBigEndian32(payload.data(), timeout_ms);
  return SendConfigurationCommand(kSetHeartbeatProtectionCommand, 0x38, save,
                                  payload.data(), payload.size());
}

LibXR::ErrorCode ZdtX42s::TriggerHoming(uint8_t mode, bool sync)
{
  if (!HasValidConfig() || mode > 5U)
  {
    return LibXR::ErrorCode::ARG_ERR;
  }

  std::array<uint8_t, 5> frame = {
      config_.address,
      kHomingCommand,
      mode,
      static_cast<uint8_t>(sync),
      0,
  };
  AppendChecksum(frame.data(), frame.size());
  return SendAndAcceptHomingResponse(frame.data(), frame.size());
}

LibXR::ErrorCode ZdtX42s::MoveToAbsoluteAngleDirect(
    float target_angle_degrees, uint16_t speed_limit_rpm,
    bool wait_for_reached, uint32_t reach_timeout_ms)
{
  if (!HasValidConfig() || !std::isfinite(target_angle_degrees) ||
      speed_limit_rpm == 0U || speed_limit_rpm > 3000U ||
      (wait_for_reached && reach_timeout_ms == 0U))
  {
    return LibXR::ErrorCode::ARG_ERR;
  }
  if (firmware_family_ != FirmwareFamily::X)
  {
    return LibXR::ErrorCode::NOT_SUPPORT;
  }

  const double tenths_of_degree =
      static_cast<double>(target_angle_degrees) * 10.0;
  if (std::fabs(tenths_of_degree) >
      static_cast<double>(std::numeric_limits<uint32_t>::max()))
  {
    return LibXR::ErrorCode::OUT_OF_RANGE;
  }

  const int64_t signed_tenths =
      static_cast<int64_t>(std::llround(tenths_of_degree));
  const uint32_t position_tenths = static_cast<uint32_t>(
      signed_tenths < 0 ? -signed_tenths : signed_tenths);
  const Direction direction = signed_tenths < 0
                                  ? Opposite(config_.positive_direction)
                                  : config_.positive_direction;

  std::array<uint8_t, 12> frame = {
      config_.address,
      kDirectPositionCommand,
      ToByte(direction),
      0,
      0,
      0,
      0,
      0,
      0,
      kAbsolutePositionMode,
      kImmediateExecution,
      0,
  };
  WriteBigEndian16(&frame[3],
                   static_cast<uint16_t>(speed_limit_rpm * 10U));
  WriteBigEndian32(&frame[5], position_tenths);
  AppendChecksum(frame.data(), frame.size());

  const auto result = SendAndExpectAcknowledgement(
      frame.data(), frame.size(), kDirectPositionCommand);
  if (result != LibXR::ErrorCode::OK || !wait_for_reached)
  {
    return result;
  }
  return WaitForReached(kDirectPositionCommand, reach_timeout_ms);
}

LibXR::ErrorCode ZdtX42s::MoveToAbsoluteAngle(float target_angle_degrees,
                                               uint16_t speed_rpm,
                                               uint8_t acceleration,
                                               bool wait_for_reached,
                                               uint32_t reach_timeout_ms)
{
  // 协议限制速度范围；到位等待模式必须有正超时，避免无限阻塞。
  if (!HasValidConfig() || !std::isfinite(target_angle_degrees) || speed_rpm == 0 ||
      speed_rpm > 3000 || (wait_for_reached && reach_timeout_ms == 0))
  {
    return LibXR::ErrorCode::ARG_ERR;
  }

  if (firmware_family_ == FirmwareFamily::X)
  {
    // X firmware encodes the target in 0.1 degree units, the maximum speed in
    // 0.1 RPM units and independent acceleration/deceleration in RPM/s.  This
    // is deliberately a separate frame from the 13-byte Emm FD frame below.
    const double tenths_of_degree = static_cast<double>(target_angle_degrees) * 10.0;
    if (std::fabs(tenths_of_degree) >
        static_cast<double>(std::numeric_limits<uint32_t>::max()))
    {
      return LibXR::ErrorCode::OUT_OF_RANGE;
    }

    const int64_t signed_tenths =
        static_cast<int64_t>(std::llround(tenths_of_degree));
    const uint32_t position_tenths = static_cast<uint32_t>(
        signed_tenths < 0 ? -signed_tenths : signed_tenths);
    const uint32_t speed_tenths = static_cast<uint32_t>(speed_rpm) * 10U;
    const uint16_t ramp_rpm_per_second = XAccelerationRpmPerSecond(acceleration);
    const Direction direction = signed_tenths < 0 ? Opposite(config_.positive_direction)
                                                   : config_.positive_direction;

    std::array<uint8_t, 16> frame = {
        config_.address,
        kPositionCommand,
        ToByte(direction),
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        kAbsolutePositionMode,
        kImmediateExecution,
        0,
    };
    WriteBigEndian16(&frame[3], ramp_rpm_per_second);
    WriteBigEndian16(&frame[5], ramp_rpm_per_second);
    WriteBigEndian16(&frame[7], static_cast<uint16_t>(speed_tenths));
    WriteBigEndian32(&frame[9], position_tenths);
    AppendChecksum(frame.data(), frame.size());

    const auto result =
        SendAndExpectAcknowledgement(frame.data(), frame.size(), kPositionCommand);
    if (result != LibXR::ErrorCode::OK || !wait_for_reached)
    {
      return result;
    }
    return WaitForReached(kPositionCommand, reach_timeout_ms);
  }

  // Emm firmware uses a pulse count, whole-RPM speed and one acceleration
  // byte.  Use double and round only at the wire-format boundary to avoid a
  // single-pulse floating-point error.
  const double pulses = static_cast<double>(target_angle_degrees) *
                        static_cast<double>(config_.pulses_per_revolution) / 360.0;
  if (std::fabs(pulses) > static_cast<double>(std::numeric_limits<uint32_t>::max()))
  {
    return LibXR::ErrorCode::OUT_OF_RANGE;
  }

  const int64_t signed_pulses = static_cast<int64_t>(std::llround(pulses));
  // 协议分别传输方向和无符号脉冲数，因此拆分符号与绝对值。
  const uint32_t pulse_count = static_cast<uint32_t>(
      signed_pulses < 0 ? -signed_pulses : signed_pulses);
  const Direction direction = signed_pulses < 0 ? Opposite(config_.positive_direction)
                                                 : config_.positive_direction;

  std::array<uint8_t, 13> frame = {
      // 帧格式：地址、FD、方向、速度(2)、加速度、脉冲数(4)、模式、同步、校验。
      config_.address,
      kPositionCommand,
      ToByte(direction),
      0,
      0,
      acceleration,
      0,
      0,
      0,
      0,
      kAbsolutePositionMode,
      kImmediateExecution,
      0,
  };
  // 将多字节字段写入固定槽位，避免依赖 MCU 的本地字节序。
  WriteBigEndian16(&frame[3], speed_rpm);
  WriteBigEndian32(&frame[6], pulse_count);
  AppendChecksum(frame.data(), frame.size());

  auto result =
      SendAndExpectAcknowledgement(frame.data(), frame.size(), kPositionCommand);
  // 未要求等待到位时，收到“已接收”确认即可返回；否则继续读取后续到位帧。
  if (result != LibXR::ErrorCode::OK || !wait_for_reached)
  {
    return result;
  }

  return WaitForReached(kPositionCommand, reach_timeout_ms);
}

LibXR::ErrorCode ZdtX42s::ReadRealtimeAngle(float& angle_degrees)
{
  if (!HasValidConfig())
  {
    return LibXR::ErrorCode::ARG_ERR;
  }

  std::array<uint8_t, 3> command = {
      // 读取命令格式：地址、36、校验。
      config_.address,
      kReadPositionCommand,
      0,
  };
  AppendChecksum(command.data(), command.size());

  auto result = Send(command.data(), command.size());
  if (result != LibXR::ErrorCode::OK)
  {
    return result;
  }

  std::array<uint8_t, 8> response{};
  result = Read(response.data(), response.size(), config_.response_timeout_ms);
  if (result != LibXR::ErrorCode::OK)
  {
    return result;
  }
  // 实时位置响应格式：地址、36、符号、位置(4)、校验。
  // 地址、功能码或校验不匹配时，响应不属于本次请求。
  if (response[0] != config_.address || response[1] != kReadPositionCommand ||
      !HasValidChecksum(response.data(), response.size()))
  {
    return LibXR::ErrorCode::CHECK_ERR;
  }
  if (response[2] != kNegativeSign && response[2] != kPositiveSign)
  {
    return LibXR::ErrorCode::CHECK_ERR;
  }

  const uint32_t raw_position = ReadBigEndian32(&response[3]);
  const int64_t signed_position = SignedPositionCounts(response[2], raw_position);
  const float raw_angle_degrees =
      firmware_family_ == FirmwareFamily::X
          ? static_cast<float>(static_cast<double>(signed_position) / 10.0)
          : static_cast<float>(static_cast<double>(signed_position) *
                               360.0 / 65536.0);
  // The motor reports positive position in its native clockwise coordinate.
  // MoveToAbsoluteAngle(), however, defines positive application angle through
  // config_.positive_direction. Apply the same mapping to feedback so a
  // commanded -5 degrees cannot be displayed as +5 degrees merely because the
  // configured positive mechanism direction is counter-clockwise.
  angle_degrees = config_.positive_direction == Direction::Clockwise
                      ? raw_angle_degrees
                      : -raw_angle_degrees;
  return LibXR::ErrorCode::OK;
}

LibXR::ErrorCode ZdtX42s::ReadMotorConfig(MotorConfigReadback& readback)
{
  readback = {};
  if (!HasValidConfig())
  {
    return LibXR::ErrorCode::ARG_ERR;
  }

  std::array<uint8_t, 4> command = {
      config_.address,
      kReadMotorConfigCommand,
      kReadMotorConfigAuxiliaryCode,
      0,
  };
  AppendChecksum(command.data(), command.size());

  auto result = Send(command.data(), command.size());
  if (result != LibXR::ErrorCode::OK)
  {
    return result;
  }

  constexpr size_t kEmmResponseSize = 33U;
  constexpr size_t kXResponseSize = 37U;
  constexpr uint8_t kEmmParameterCount = 0x15U;
  constexpr uint8_t kXParameterCount = 0x18U;
  std::array<uint8_t, kXResponseSize> response{};
  result = Read(response.data(), 4U, config_.response_timeout_ms);
  if (result != LibXR::ErrorCode::OK)
  {
    return result;
  }
  if (response[0] != config_.address ||
      response[1] != kReadMotorConfigCommand ||
      (response[2] != kEmmResponseSize && response[2] != kXResponseSize))
  {
    return LibXR::ErrorCode::CHECK_ERR;
  }

  const size_t response_size = response[2];
  result = Read(response.data() + 4U, response_size - 4U,
                config_.response_timeout_ms);
  if (result != LibXR::ErrorCode::OK)
  {
    return result;
  }
  if (!HasValidChecksum(response.data(), response_size))
  {
    return LibXR::ErrorCode::CHECK_ERR;
  }

  readback.total_bytes = response[2];
  readback.parameter_count = response[3];
  if (response_size == kXResponseSize)
  {
    readback.firmware_type = 0U;
    if (response[3] != kXParameterCount)
    {
      return LibXR::ErrorCode::CHECK_ERR;
    }
    firmware_family_ = FirmwareFamily::X;
    return LibXR::ErrorCode::NOT_SUPPORT;
  }
  readback.firmware_type = 1U;
  if (response[3] != kEmmParameterCount)
  {
    return LibXR::ErrorCode::CHECK_ERR;
  }

  readback.motor_type = response[4];
  readback.pulse_port_mode = response[5];
  readback.communication_port_mode = response[6];
  readback.enable_pin_level = response[7];
  readback.positive_direction = response[8];
  readback.microstep = response[9];
  readback.interpolation = response[10] != 0U;
  readback.serial_baud_rate = response[18];
  readback.address = response[20];
  readback.checksum_mode = response[21];
  readback.response_mode = response[22];
  readback.position_window_tenths_degree = ReadBigEndian16(&response[30]);
  readback.pulses_per_revolution =
      PulsesPerRevolution(readback.motor_type, readback.microstep);

  if (readback.pulses_per_revolution == 0U ||
      readback.pulse_port_mode > 4U ||
      readback.communication_port_mode > 4U ||
      readback.enable_pin_level > 2U ||
      readback.positive_direction > 1U ||
      response[10] > 1U ||
      readback.serial_baud_rate > 8U ||
      readback.address != config_.address ||
      readback.checksum_mode > 4U ||
      readback.response_mode > 4U)
  {
    return LibXR::ErrorCode::OUT_OF_RANGE;
  }

  // Motion commands use the driver's configured microstep count. Apply the
  // validated readback only after the complete frame has passed all checks.
  config_.pulses_per_revolution = readback.pulses_per_revolution;
  firmware_family_ = FirmwareFamily::Emm;
  return LibXR::ErrorCode::OK;
}

bool ZdtX42s::IsAngleWithinTolerance(float measured_angle_degrees,
                                     float target_angle_degrees,
                                     float tolerance_degrees)
{
  // NaN/无穷值不能用于运动判定；负容差没有物理意义。
  if (!std::isfinite(measured_angle_degrees) || !std::isfinite(target_angle_degrees) ||
      !std::isfinite(tolerance_degrees) || tolerance_degrees < 0.0F)
  {
    return false;
  }

  // 先将误差折算到一个周期内，再归一化到 [-180, 180]，得到圆周上的最短角距。
  float error = std::fmod(measured_angle_degrees - target_angle_degrees, 360.0F);
  if (error > 180.0F)
  {
    error -= 360.0F;
  }
  else if (error < -180.0F)
  {
    error += 360.0F;
  }
  return std::fabs(error) <= tolerance_degrees;
}

LibXR::ErrorCode ZdtX42s::Send(const uint8_t* frame, size_t size)
{
  // 操作对象引用局部信号量；Write 在函数返回前完成或超时，因此其生命周期安全。
  LibXR::Semaphore completed;
  LibXR::WriteOperation operation(completed, config_.write_timeout_ms);
  return uart_.Write(LibXR::ConstRawData(frame, size), operation);
}

LibXR::ErrorCode ZdtX42s::Read(uint8_t* frame, size_t size, uint32_t timeout_ms)
{
  // 与 Send 相同，ReadOperation 封装一次同步读和它的完成同步对象。
  LibXR::Semaphore completed;
  LibXR::ReadOperation operation(completed, timeout_ms);
  return uart_.Read(LibXR::RawData(frame, size), operation);
}

LibXR::ErrorCode ZdtX42s::SendAndExpectAcknowledgement(const uint8_t* frame,
                                                        size_t size,
                                                        uint8_t command)
{
  auto result = Send(frame, size);
  if (result != LibXR::ErrorCode::OK)
  {
    return result;
  }

  std::array<uint8_t, 4> response{};
  result = Read(response.data(), response.size(), config_.response_timeout_ms);
  if (result != LibXR::ErrorCode::OK)
  {
    return result;
  }
  // 通用确认帧格式：地址、功能码、状态、校验。
  return ValidateResponse(response.data(), response.size(), command, kAcknowledged);
}

LibXR::ErrorCode ZdtX42s::SendAndAcceptHomingResponse(const uint8_t* frame,
                                                       size_t size)
{
  auto result = Send(frame, size);
  if (result != LibXR::ErrorCode::OK)
  {
    return result;
  }

  std::array<uint8_t, 4> response{};
  result = Read(response.data(), response.size(), config_.response_timeout_ms);
  if (result != LibXR::ErrorCode::OK)
  {
    return result;
  }
  if (response[0] != config_.address || response[1] != kHomingCommand ||
      !HasValidChecksum(response.data(), response.size()))
  {
    return LibXR::ErrorCode::CHECK_ERR;
  }
  switch (response[2])
  {
    case kAcknowledged:
    case 0x12:
    case kReached:
      return LibXR::ErrorCode::OK;
    default:
      return ResponseStatusToError(response[2]);
  }
}

LibXR::ErrorCode ZdtX42s::SendSimpleAction(uint8_t command,
                                            uint8_t auxiliary_code)
{
  if (!HasValidConfig())
  {
    return LibXR::ErrorCode::ARG_ERR;
  }

  std::array<uint8_t, 4> frame = {
      config_.address,
      command,
      auxiliary_code,
      0,
  };
  AppendChecksum(frame.data(), frame.size());
  return SendAndExpectAcknowledgement(frame.data(), frame.size(), command);
}

LibXR::ErrorCode ZdtX42s::SendConfigurationCommand(
    uint8_t command, uint8_t auxiliary_code, bool save, const uint8_t* payload,
    size_t payload_size)
{
  if (!HasValidConfig() || payload == nullptr || payload_size > 15U)
  {
    return LibXR::ErrorCode::ARG_ERR;
  }

  // The largest EMM V5 configuration frame currently exposed is the 17-byte
  // PID frame. Keep the storage fixed to avoid heap use on the MCU.
  std::array<uint8_t, 20> frame{};
  frame[0] = config_.address;
  frame[1] = command;
  frame[2] = auxiliary_code;
  frame[3] = static_cast<uint8_t>(save);
  std::copy(payload, payload + payload_size, frame.data() + 4U);
  const size_t frame_size = payload_size + 5U;
  AppendChecksum(frame.data(), frame_size);
  return SendAndExpectAcknowledgement(frame.data(), frame_size, command);
}

LibXR::ErrorCode ZdtX42s::WaitForReached(uint8_t command, uint32_t timeout_ms)
{
  // X42S 在位置命令执行完成后额外发送一帧状态为 0x9F 的到位通知。
  std::array<uint8_t, 4> response{};
  const auto result = Read(response.data(), response.size(), timeout_ms);
  if (result != LibXR::ErrorCode::OK)
  {
    return result;
  }
  return ValidateResponse(response.data(), response.size(), command, kReached);
}

LibXR::ErrorCode ZdtX42s::ValidateResponse(const uint8_t* frame, size_t size,
                                            uint8_t command,
                                            uint8_t expected_status) const
{
  // 固定长度检查必须在访问固定字段前完成，防止错误帧被当作有效协议解析。
  if (size != 4 || frame[0] != config_.address || frame[1] != command ||
      !HasValidChecksum(frame, size))
  {
    return LibXR::ErrorCode::CHECK_ERR;
  }
  if (frame[2] == expected_status)
  {
    return LibXR::ErrorCode::OK;
  }
  // 状态未达到期望值时，尽量保留手册定义的具体错误类型。
  return ResponseStatusToError(frame[2]);
}

LibXR::ErrorCode ZdtX42s::ResponseStatusToError(uint8_t status) const
{
  // 下列状态码来自电机协议：状态异常、参数错误和校验错误分别映射到
  // LibXR 的可区分错误码，未知状态统一视为失败。
  switch (status)
  {
    case 0x12:
    case 0x22:
      return LibXR::ErrorCode::STATE_ERR;
    case 0xE2:
      return LibXR::ErrorCode::ARG_ERR;
    case 0xEE:
      return LibXR::ErrorCode::CHECK_ERR;
    default:
      return LibXR::ErrorCode::FAILED;
  }
}

void ZdtX42s::AppendChecksum(uint8_t* frame, size_t size) const
{
  // 所有调用处均提供至少一个字节的帧；末字节由此函数专门保留给校验值。
  if (config_.checksum == ChecksumMode::Fixed6B)
  {
    frame[size - 1] = kFixedChecksum;
    return;
  }

  // XOR 模式不包含帧尾自身，从第 0 字节异或到倒数第 2 字节。
  uint8_t checksum = 0;
  for (size_t index = 0; index + 1 < size; ++index)
  {
    checksum ^= frame[index];
  }
  frame[size - 1] = checksum;
}

bool ZdtX42s::HasValidChecksum(const uint8_t* frame, size_t size) const
{
  // 空帧没有可验证的校验字节，先处理以避免 size - 1 下溢。
  if (size == 0)
  {
    return false;
  }
  if (config_.checksum == ChecksumMode::Fixed6B)
  {
    return frame[size - 1] == kFixedChecksum;
  }

  uint8_t checksum = 0;
  for (size_t index = 0; index + 1 < size; ++index)
  {
    checksum ^= frame[index];
  }
  return frame[size - 1] == checksum;
}

bool ZdtX42s::HasValidConfig() const
{
  // 这几项决定命令可被正确寻址、角度可被正确换算，且通信不会无限等待。
  return config_.address != 0 && config_.pulses_per_revolution != 0 &&
         config_.write_timeout_ms != 0 && config_.response_timeout_ms != 0;
}

}  // namespace BujinMotor
