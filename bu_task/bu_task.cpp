#include "bu_task.hpp"

#include <algorithm>
#include <cmath>

namespace BuTask
{

void ZeroAndMoveThread(ZeroAndMoveConfig* config)
{
  if (config == nullptr || config->motor == nullptr)
  {
    return;
  }

  config->result = config->motor->Enable(true);
  if (config->result != LibXR::ErrorCode::OK)
  {
    return;
  }
  LibXR::Thread::Sleep(5);

  config->result = config->motor->SetCurrentPositionAsZero();
  if (config->result != LibXR::ErrorCode::OK)
  {
    return;
  }
  LibXR::Thread::Sleep(5);

  config->result = config->motor->MoveToAbsoluteAngle(
      config->target_angle_degrees, config->speed_rpm, config->acceleration, false,
      config->reach_timeout_ms);
  if (config->result != LibXR::ErrorCode::OK)
  {
    return;
  }

  const uint32_t start_time = LibXR::Thread::GetTime();
  while (true)
  {
    config->result = config->motor->ReadRealtimeAngle(config->measured_angle_degrees);
    if (config->result != LibXR::ErrorCode::OK)
    {
      return;
    }

    if (BujinMotor::ZdtX42s::IsAngleWithinTolerance(
            config->measured_angle_degrees, config->target_angle_degrees,
            config->tolerance_degrees))
    {
      config->result = LibXR::ErrorCode::OK;
      return;
    }

    if (static_cast<uint32_t>(LibXR::Thread::GetTime() - start_time) >=
        config->reach_timeout_ms)
    {
      config->result = LibXR::ErrorCode::TIMEOUT;
      return;
    }
    LibXR::Thread::Sleep(10);
  }
}

void BallTrackingThread(BallTrackingConfig* config)
{
  if (config == nullptr || config->motor == nullptr || config->mailbox == nullptr ||
      config->min_angle_degrees > config->max_angle_degrees ||
      config->min_confidence < 0.0F || config->min_confidence > 1.0F ||
      config->input_timeout_ms == 0 || config->motor_speed_rpm == 0)
  {
    return;
  }

  config->result = config->motor->Enable(true);
  if (config->result != LibXR::ErrorCode::OK)
  {
    return;
  }
  LibXR::Thread::Sleep(5);

  config->result = config->motor->SetCurrentPositionAsZero();
  if (config->result != LibXR::ErrorCode::OK)
  {
    return;
  }

  bool motion_active = false;
  float last_target_angle = 0.0F;
  while (true)
  {
    Maxican::BallMeasurement measurement;
    if (!config->mailbox->WaitForUpdate(measurement, config->input_timeout_ms))
    {
      if (motion_active)
      {
        config->result = config->motor->StopImmediately();
        motion_active = false;
      }
      else
      {
        config->result = LibXR::ErrorCode::TIMEOUT;
      }
      continue;
    }

    config->last_measurement = measurement;
    const uint32_t age_ms =
        static_cast<uint32_t>(LibXR::Thread::GetTime() - measurement.received_time_ms);
    if (!measurement.valid || measurement.confidence < config->min_confidence ||
        age_ms > config->input_timeout_ms)
    {
      if (motion_active)
      {
        config->result = config->motor->StopImmediately();
        motion_active = false;
      }
      else
      {
        config->result = LibXR::ErrorCode::NO_RESPONSE;
      }
      continue;
    }

    const float unconstrained_target = config->angle_offset_degrees +
                                       config->position_gain_degrees_per_cm *
                                           (measurement.position_cm - config->center_position_cm) +
                                       config->velocity_gain_degrees_per_pixel_s *
                                           measurement.velocity_pixel_s;
    if (!std::isfinite(unconstrained_target))
    {
      config->result = LibXR::ErrorCode::ARG_ERR;
      continue;
    }

    const float target_angle = std::clamp(unconstrained_target,
                                          config->min_angle_degrees,
                                          config->max_angle_degrees);
    config->target_angle_degrees = target_angle;
    if (motion_active &&
        std::fabs(target_angle - last_target_angle) <
            config->minimum_command_delta_degrees)
    {
      config->result = LibXR::ErrorCode::OK;
      continue;
    }

    config->result = config->motor->MoveToAbsoluteAngle(
        target_angle, config->motor_speed_rpm, config->motor_acceleration);
    if (config->result == LibXR::ErrorCode::OK)
    {
      motion_active = true;
      last_target_angle = target_angle;
    }
    else
    {
      (void)config->motor->StopImmediately();
      motion_active = false;
    }
  }
}

}  // namespace BuTask
