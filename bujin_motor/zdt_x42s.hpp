#pragma once

#include <cstddef>
#include <cstdint>

#include "libxr.hpp"
#include "uart.hpp"

// ZDT X42S 闭环步进电机的 TTL/RS485 串口协议封装。
//
// 本驱动不管理串口初始化；调用方应先按电机的实际配置完成 UART
// 初始化，然后将对应的 LibXR::UART 对象传入。每次命令调用都是同步的：
// 写入命令后会等待驱动器回复确认帧，必要时还会继续等待“到位”帧。
namespace BujinMotor
{

// 帧尾校验策略。不同版本的电机固件可能使用不同的校验方式。
enum class ChecksumMode : uint8_t
{
  // 帧尾固定为 0x6B，X42S 常见的默认协议格式。
  Fixed6B,
  // 帧尾为前面所有字节的按位异或结果。
  Xor,
};

// 电机协议中的方向字节，以及项目约定的正方向。
enum class Direction : uint8_t
{
  // 协议值 0x00：顺时针。
  Clockwise = 0x00,
  // 协议值 0x01：逆时针。
  CounterClockwise = 0x01,
};

// X42S 单电机串口驱动。
//
// 同一 UART 上的命令和响应必须严格一一对应。因此同一时刻应只由一个
// 任务访问同一个实例，避免其他任务插入命令而导致响应帧被错误消费。
class ZdtX42s
{
 public:
  // 与实际电机和通信链路对应的可配置参数。
  struct Config
  {
    // 电机从站地址。0 是无效地址，合法取值由电机固件决定。
    uint8_t address = 1;

    // 电机转动一整圈所对应的脉冲数，用于角度与协议脉冲数的换算。
    // 默认 3200 适用于 1.8 度电机、16 微步的常见配置；必须与驱动器
    // 当前细分设置一致，否则实际运动角度会产生比例误差。
    uint32_t pulses_per_revolution = 3200;

    // 对本机构定义的“目标角度增加”方向。传入负角度时驱动自动取反。
    Direction positive_direction = Direction::CounterClockwise;

    // 命令帧和响应帧使用的校验模式，发送和接收必须与电机一致。
    ChecksumMode checksum = ChecksumMode::Fixed6B;

    // UART 写操作的最长等待时间，单位为毫秒，0 视为无效配置。
    uint32_t write_timeout_ms = 100;

    // 等待普通响应帧的最长时间，单位为毫秒，0 视为无效配置。
    uint32_t response_timeout_ms = 100;
  };

  // 使用默认 Config 创建驱动实例。
  explicit ZdtX42s(LibXR::UART& uart);

  // 使用指定通信地址、脉冲数和校验方式创建驱动实例。
  ZdtX42s(LibXR::UART& uart, Config config);

  // 使能或释放电机。
  // enabled 为 true 时输出保持力矩；为 false 时释放电机。函数等待 F3
  // 命令的确认帧，并返回串口、协议或驱动器状态错误。
  [[nodiscard]] LibXR::ErrorCode Enable(bool enabled);

  // 立即停止当前运动。该命令不会等待机械系统完全静止，只等待驱动器
  // 对 FE 命令的确认帧。
  [[nodiscard]] LibXR::ErrorCode StopImmediately();

  // 将当前编码器位置写为电机零点。调用前应确保机构位于期望的机械零位。
  [[nodiscard]] LibXR::ErrorCode SetCurrentPositionAsZero();

  // 以绝对位置模式运动到指定角度。
  //
  // target_angle_degrees 相对于已设置的电机零点；可正可负，不限制在
  // [-360, 360]。speed_rpm 的有效范围是 1~3000 RPM，acceleration 直接
  // 按协议写入帧中。wait_for_reached 为 true 时，确认收帧后还会等待
  // FD 命令的到位帧，最长等待 reach_timeout_ms 毫秒。
  [[nodiscard]] LibXR::ErrorCode MoveToAbsoluteAngle(
      float target_angle_degrees, uint16_t speed_rpm, uint8_t acceleration,
      bool wait_for_reached = false, uint32_t reach_timeout_ms = 5000);

  // 读取单圈实时编码器角度，成功时写入 angle_degrees。
  // 返回值范围约为 [-360, 360)，负号由电机响应中的符号字节决定。
  [[nodiscard]] LibXR::ErrorCode ReadRealtimeAngle(float& angle_degrees);

  // 判断两个角度在圆周意义上是否足够接近。
  // 例如 359 度和 1 度的最短误差为 2 度，而不是 358 度。
  [[nodiscard]] static bool IsAngleWithinTolerance(
      float measured_angle_degrees, float target_angle_degrees,
      float tolerance_degrees);

 private:
  // 下面的常量为 X42S TTL 协议的功能码和响应状态码。
  static constexpr uint8_t kEnableCommand = 0xF3;
  static constexpr uint8_t kStopCommand = 0xFE;
  static constexpr uint8_t kSetZeroCommand = 0x0A;
  static constexpr uint8_t kPositionCommand = 0xFD;
  static constexpr uint8_t kReadPositionCommand = 0x36;

  // 普通命令已被驱动器接收，以及位置命令已经到位时的状态字节。
  static constexpr uint8_t kAcknowledged = 0x02;
  static constexpr uint8_t kReached = 0x9F;

  // Fixed6B 校验模式下固定写入帧尾的校验字节。
  static constexpr uint8_t kFixedChecksum = 0x6B;

  // 将完整帧同步写入 UART；超时采用 Config::write_timeout_ms。
  [[nodiscard]] LibXR::ErrorCode Send(const uint8_t* frame, size_t size);

  // 从 UART 读取指定长度的完整帧。UART 层负责在 timeout_ms 内收齐数据。
  [[nodiscard]] LibXR::ErrorCode Read(uint8_t* frame, size_t size,
                                      uint32_t timeout_ms);

  // 发送命令并读取其四字节确认帧，确认状态必须为 kAcknowledged。
  [[nodiscard]] LibXR::ErrorCode SendAndExpectAcknowledgement(
      const uint8_t* frame, size_t size, uint8_t command);

  // 在位置命令确认后读取其四字节到位帧，状态必须为 kReached。
  [[nodiscard]] LibXR::ErrorCode WaitForReached(uint8_t command,
                                                uint32_t timeout_ms);

  // 验证响应帧的长度、地址、功能码、校验和状态字节，并转换错误状态。
  [[nodiscard]] LibXR::ErrorCode ValidateResponse(
      const uint8_t* frame, size_t size, uint8_t command,
      uint8_t expected_status) const;

  // 将手册定义的响应状态字节映射为项目统一的 ErrorCode。
  [[nodiscard]] LibXR::ErrorCode ResponseStatusToError(uint8_t status) const;

  // 按配置为待发帧的最后一个字节写入校验值。
  void AppendChecksum(uint8_t* frame, size_t size) const;

  // 按配置验证收到帧最后一个字节的校验值。
  [[nodiscard]] bool HasValidChecksum(const uint8_t* frame, size_t size) const;

  // 防止以无地址、无角度换算依据或无超时的配置访问硬件。
  [[nodiscard]] bool HasValidConfig() const;

  // 串口由应用层拥有，驱动只保存引用，不负责其生命周期。
  LibXR::UART& uart_;

  // 驱动实例的协议和运动换算配置。
  Config config_;
};

}  // namespace BujinMotor
