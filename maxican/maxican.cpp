#include "maxican.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace Maxican {
namespace {

constexpr uint8_t kFrameHead0 = 0xAAU;
constexpr uint8_t kFrameHead1 = 0x55U;
constexpr size_t kChecksumIndex = BallReceiver::kFrameLength - 1U;
constexpr uint32_t kMinimumShowIntervalMs = 20;
constexpr uint32_t kMaximumShowIntervalMs = 1000;
constexpr uint32_t kMaximumShowDurationMs = 10000;

uint8_t CalculateChecksum(const uint8_t* data, size_t length)
{
  uint8_t checksum = 0U;
  for (size_t index = 0; index < length; ++index)
  {
    checksum ^= data[index];
  }
  return checksum;
}

uint16_t ReadU16Le(const uint8_t* data)
{
  return static_cast<uint16_t>(data[0]) |
         (static_cast<uint16_t>(data[1]) << 8U);
}

uint32_t ReadU32Le(const uint8_t* data)
{
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8U) |
         (static_cast<uint32_t>(data[2]) << 16U) |
         (static_cast<uint32_t>(data[3]) << 24U);
}

bool ParseCommandMilliseconds(const char* text, uint32_t& value)
{
  if (text == nullptr || text[0] == '\0')
  {
    return false;
  }

  uint32_t parsed = 0;
  for (const char* current = text; *current != '\0'; ++current)
  {
    if (*current < '0' || *current > '9')
    {
      return false;
    }

    const uint32_t digit = static_cast<uint32_t>(*current - '0');
    if (parsed > (std::numeric_limits<uint32_t>::max() - digit) / 10U)
    {
      return false;
    }
    parsed = parsed * 10U + digit;
  }

  value = parsed;
  return true;
}

} // namespace

void BallMailbox::Publish(const BallMeasurement &measurement) {
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    latest_ = measurement;
    has_measurement_ = true;
  }
  update_ready_.Post();
}

bool BallMailbox::WaitForUpdate(BallMeasurement &measurement,
                                uint32_t timeout_ms) {
  if (update_ready_.Wait(timeout_ms) != LibXR::ErrorCode::OK) {
    return false;
  }

  while (update_ready_.Wait(0) == LibXR::ErrorCode::OK) {
  }

  LibXR::Mutex::LockGuard lock(mutex_);
  if (!has_measurement_) {
    return false;
  }
  measurement = latest_;
  return true;
}

bool BallMailbox::GetLatest(BallMeasurement& measurement) const
{
  LibXR::Mutex::LockGuard lock(mutex_);
  if (!has_measurement_)
  {
    return false;
  }
  measurement = latest_;
  return true;
}

BallReceiver::BallReceiver(LibXR::UART& uart, BallMailbox& mailbox,
                           LibXR::RamFS& ramfs)
    : uart_(uart),
      mailbox_(mailbox),
      topic_(LibXR::Topic::CreateTopic<BallMeasurement>(kTopicName)),
      command_file_(LibXR::RamFS::CreateFile("ball", CommandFunc, this))
{
  ramfs.Add(command_file_);
}

void BallReceiver::Run() {
  std::array<uint8_t, kFrameLength> frame{};
  size_t size = 0;

  while (true) {
    uint8_t byte = 0;
    if (!ReadByte(byte)) {
      LibXR::Thread::Sleep(1);
      continue;
    }

    if (size == 0U) {
      if (byte == kFrameHead0) {
        frame[size++] = byte;
      }
      continue;
    }

    if (size == 1U) {
      if (byte == kFrameHead1) {
        frame[size++] = byte;
      } else if (byte == kFrameHead0) {
        frame[0] = byte;
      } else {
        size = 0U;
      }
      continue;
    }

    frame[size++] = byte;
    if (size != frame.size()) {
      continue;
    }

    BallMeasurement measurement;
    const bool parsed = ParseFrame(frame.data(), frame.size(), measurement);
    if (parsed) {
      measurement.received_time_ms = LibXR::Thread::GetTime();
      mailbox_.Publish(measurement);
      topic_.Publish(measurement, LibXR::Timebase::GetMicroseconds());
    }

    // On a damaged frame, keep any header already present in its tail so the
    // next valid frame is not discarded while the stream is resynchronized.
    size = 0U;
    if (!parsed) {
      for (size_t index = 1U; index < frame.size(); ++index) {
        if (frame[index] != kFrameHead0) {
          continue;
        }

        if (index + 1U < frame.size() && frame[index + 1U] == kFrameHead1) {
          const size_t retained_size = frame.size() - index;
          std::memmove(frame.data(), frame.data() + index, retained_size);
          size = retained_size;
          break;
        }
        if (index + 1U == frame.size()) {
          frame[0] = kFrameHead0;
          size = 1U;
          break;
        }
      }
    }
  }
}

int BallReceiver::CommandFunc(BallReceiver* receiver, int argc, char** argv)
{
  if (receiver == nullptr)
  {
    return -1;
  }

  if (argc == 1)
  {
    LibXR::STDIO::Printf<"Usage: ball latest | ball show <time_ms> <interval_ms>\r\n">();
    return 0;
  }
  if (argc == 2 && std::strcmp(argv[1], "latest") == 0)
  {
    receiver->PrintLatest();
    return 0;
  }
  if (argc == 4 && std::strcmp(argv[1], "show") == 0)
  {
    uint32_t duration_ms = 0;
    uint32_t interval_ms = 0;
    if (!ParseCommandMilliseconds(argv[2], duration_ms) ||
        !ParseCommandMilliseconds(argv[3], interval_ms) || duration_ms == 0 ||
        interval_ms == 0)
    {
      LibXR::STDIO::Printf<"Error: time_ms and interval_ms must be positive integers.\r\n">();
      return -1;
    }

    duration_ms = std::min(duration_ms, kMaximumShowDurationMs);
    interval_ms = std::clamp(interval_ms, kMinimumShowIntervalMs,
                             kMaximumShowIntervalMs);
    for (uint32_t elapsed_ms = 0; elapsed_ms < duration_ms;
         elapsed_ms += interval_ms)
    {
      receiver->PrintLatest();
      LibXR::Thread::Sleep(interval_ms);
    }
    return 0;
  }

  LibXR::STDIO::Printf<"Error: invalid ball command.\r\n">();
  return -1;
}

void BallReceiver::PrintLatest() const
{
  BallMeasurement measurement;
  if (!mailbox_.GetLatest(measurement))
  {
    LibXR::STDIO::Printf<"BALL: no frame received.\r\n">();
    return;
  }

  const uint32_t age_ms =
      static_cast<uint32_t>(LibXR::Thread::GetTime() - measurement.received_time_ms);
  LibXR::STDIO::Printf<"BALL v=%u x=%+.2fcm vx=%+.2fpx/s c=%.2f src=%u age=%ums\r\n">(
      measurement.valid ? 1U : 0U, measurement.position_cm,
      measurement.velocity_pixel_s, measurement.confidence,
      static_cast<unsigned>(measurement.frame_time_ms), static_cast<unsigned>(age_ms));
}

bool BallReceiver::ParseFrame(const uint8_t* frame, size_t size,
                              BallMeasurement &measurement) {
  if (frame == nullptr || size != kFrameLength ||
      frame[0] != kFrameHead0 || frame[1] != kFrameHead1 ||
      CalculateChecksum(frame, kChecksumIndex) != frame[kChecksumIndex] ||
      frame[2] > 1U || frame[7] > 100U) {
    return false;
  }

  BallMeasurement parsed;
  parsed.valid = frame[2] == 1U;
  parsed.frame_time_ms = ReadU32Le(&frame[8]);
  if (parsed.valid) {
    parsed.position_cm = static_cast<float>(static_cast<int16_t>(ReadU16Le(&frame[3]))) /
                         100.0F;
    parsed.velocity_pixel_s =
        static_cast<float>(static_cast<int16_t>(ReadU16Le(&frame[5]))) / 10.0F;
    parsed.confidence = static_cast<float>(frame[7]) / 100.0F;
  }

  measurement = parsed;
  return true;
}

bool BallReceiver::ReadByte(uint8_t &byte) {
  LibXR::Semaphore completed;
  LibXR::ReadOperation operation(completed, UINT32_MAX);
  return uart_.Read(LibXR::RawData(byte), operation) == LibXR::ErrorCode::OK;
}

void ReceiveThread(BallReceiver *receiver) {
  if (receiver != nullptr) {
    receiver->Run();
  }
}

} // namespace Maxican
