#pragma once

#include <cstdint>

#include "libxr.hpp"
#include "maxican.hpp"
#include "zdt_x42s.hpp"

namespace BuTask
{

struct ZeroAndMoveConfig
{
  BujinMotor::ZdtX42s* motor = nullptr;
  float target_angle_degrees = 90.0F;
  uint16_t speed_rpm = 300;
  uint8_t acceleration = 100;
  float tolerance_degrees = 0.8F;
  uint32_t reach_timeout_ms = 5000;
  LibXR::ErrorCode result = LibXR::ErrorCode::PENDING;
  float measured_angle_degrees = 0.0F;
};

void ZeroAndMoveThread(ZeroAndMoveConfig* config);

struct BallTrackingConfig
{
  BujinMotor::ZdtX42s* motor = nullptr;
  Maxican::BallMailbox* mailbox = nullptr;
  float center_position_cm = 0.0F;
  float angle_offset_degrees = 0.0F;
  float position_gain_degrees_per_cm = 1.0F;
  float velocity_gain_degrees_per_pixel_s = 0.0F;
  float min_angle_degrees = -30.0F;
  float max_angle_degrees = 30.0F;
  float min_confidence = 0.50F;
  float minimum_command_delta_degrees = 0.2F;
  uint16_t motor_speed_rpm = 300;
  uint8_t motor_acceleration = 100;
  uint32_t input_timeout_ms = 200;
  LibXR::ErrorCode result = LibXR::ErrorCode::PENDING;
  Maxican::BallMeasurement last_measurement;
  float target_angle_degrees = 0.0F;
};

void BallTrackingThread(BallTrackingConfig* config);

}  // namespace BuTask
