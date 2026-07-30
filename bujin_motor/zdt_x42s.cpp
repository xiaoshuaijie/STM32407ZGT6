#include "zdt_x42s.hpp"

#include <array>
#include <cmath>
#include <limits>

namespace BujinMotor
{
namespace
{

// 读取实时位置响应时，第三字节表示后续位置值的符号。
constexpr uint8_t kNegativeSign = 0;
constexpr uint8_t kPositiveSign = 1;

// FD 位置命令的模式和同步执行字段。该驱动固定使用绝对位置、立即执行。
constexpr uint8_t kAbsolutePositionMode = 1;
constexpr uint8_t kImmediateExecution = 0;

// F3、FE、0A 命令中由 X42S 协议规定的固定辅助字节。
constexpr uint8_t kEnableAuxiliaryCode = 0xAB;
constexpr uint8_t kStopAuxiliaryCode = 0x98;
constexpr uint8_t kSetZeroAuxiliaryCode = 0x6D;

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

  // 角度可跨多圈。使用 double 计算并在最后四舍五入，减少浮点计算造成的
  // 单脉冲偏差。
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

  // 单圈编码器分辨率为 65536；大于 65535 的值违反协议范围。
  const uint32_t raw_position = ReadBigEndian32(&response[3]);
  if (raw_position > 65535U)
  {
    return LibXR::ErrorCode::OUT_OF_RANGE;
  }

  // 将 [0, 65535] 映射到 [0, 360)，再按符号字节恢复负角度。
  angle_degrees = static_cast<float>(raw_position) * 360.0F / 65536.0F;
  if (response[2] == kNegativeSign)
  {
    angle_degrees = -angle_degrees;
  }
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
