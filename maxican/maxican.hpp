#pragma once

#include <cstddef>
#include <cstdint>

#include "libxr.hpp"
#include "uart.hpp"

namespace Maxican
{

struct BallMeasurement
{
  bool valid = false;
  float position_cm = 0.0F;
  float velocity_pixel_s = 0.0F;
  float confidence = 0.0F;
  uint32_t frame_time_ms = 0;
  uint32_t received_time_ms = 0;
};

class BallMailbox
{
 public:
  void Publish(const BallMeasurement& measurement);

  [[nodiscard]] bool WaitForUpdate(BallMeasurement& measurement,
                                   uint32_t timeout_ms);
  [[nodiscard]] bool GetLatest(BallMeasurement& measurement) const;

 private:
  mutable LibXR::Mutex mutex_;
  LibXR::Semaphore update_ready_;
  BallMeasurement latest_;
  bool has_measurement_ = false;
};

class BallReceiver
{
 public:
  static constexpr size_t kFrameLength = 13;
  static constexpr const char* kTopicName = "ball_measurement";

  BallReceiver(LibXR::UART& uart, BallMailbox& mailbox, LibXR::RamFS& ramfs);

  void Run();

  [[nodiscard]] static bool ParseFrame(const uint8_t* frame, size_t size,
                                       BallMeasurement& measurement);

 private:
  static int CommandFunc(BallReceiver* receiver, int argc, char** argv);
  void PrintLatest() const;
  [[nodiscard]] bool ReadByte(uint8_t& byte);

  LibXR::UART& uart_;
  BallMailbox& mailbox_;
  LibXR::Topic topic_;
  LibXR::RamFS::File command_file_;
};

void ReceiveThread(BallReceiver* receiver);

}  // namespace Maxican
