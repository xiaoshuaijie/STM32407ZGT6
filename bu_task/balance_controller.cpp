#include "balance_controller.hpp"

#include <algorithm>
#include <cmath>

namespace BuTask
{
namespace
{

constexpr uint32_t kTask3TimeLimitMs = 5000;
constexpr uint32_t kTask4TimeLimitMs = 8000;
constexpr uint32_t kLapTimeLimitMs = 30000;
constexpr uint8_t kTask3MovePositive = 1;
constexpr uint8_t kTask3MoveNegative = 2;

bool IsFreshMeasurement(const Maxican::BallMeasurement& measurement,
                        uint32_t now_ms, uint32_t timeout_ms)
{
  return measurement.valid &&
         static_cast<uint32_t>(now_ms - measurement.received_time_ms) <= timeout_ms;
}

}  // namespace

BalanceController::BalanceController(BalanceControllerConfig config)
    : config_(config),
      state_topic_(LibXR::Topic::CreateTopic<BalanceStatus>(kStateTopicName))
{
}

void BalanceController::SelectNextTask()
{
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    if (status_.run_state == BalanceRunState::Running)
    {
      return;
    }
    status_.selected_task = NextTask(status_.selected_task);
    status_.run_state = BalanceRunState::Ready;
    status_.fault = BalanceFault::None;
    status_.elapsed_ms = 0;
    status_.task_phase = 0;
  }
  PublishStatus();
}

void BalanceController::ToggleRun()
{
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    if (status_.run_state == BalanceRunState::Running)
    {
      abort_requested_ = true;
    }
    else
    {
      start_requested_ = true;
    }
  }
}

BalanceStatus BalanceController::GetStatus() const
{
  LibXR::Mutex::LockGuard lock(mutex_);
  return status_;
}

void BalanceController::Run()
{
  if (!IsConfigurationValid())
  {
    SetFault(BalanceFault::Configuration, LibXR::ErrorCode::ARG_ERR);
    return;
  }

  PublishStatus();
  while (true)
  {
    const uint32_t cycle_start_ms = LibXR::Thread::GetTime();
    HandleRequests();

    if (GetStatus().run_state == BalanceRunState::Running)
    {
      Maxican::BallMeasurement measurement;
      if (config_.mailbox->WaitForUpdate(measurement, config_.control_period_ms))
      {
        LibXR::Mutex::LockGuard lock(mutex_);
        latest_measurement_ = measurement;
        has_measurement_ = true;
      }
      UpdateControl(LibXR::Thread::GetTime());
    }
    else
    {
      LibXR::Thread::Sleep(config_.control_period_ms);
    }

    const uint32_t elapsed_ms =
        static_cast<uint32_t>(LibXR::Thread::GetTime() - cycle_start_ms);
    if (elapsed_ms < config_.control_period_ms)
    {
      LibXR::Thread::Sleep(config_.control_period_ms - elapsed_ms);
    }
  }
}

void BalanceController::HandleRequests()
{
  bool start = false;
  bool abort = false;
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    start = start_requested_;
    abort = abort_requested_;
    start_requested_ = false;
    abort_requested_ = false;
  }

  if (abort)
  {
    AbortTask(BalanceFault::None);
  }
  else if (start)
  {
    StartTask();
  }
}

void BalanceController::StartTask()
{
  const auto enable_result = config_.motor->Enable(true);
  if (enable_result != LibXR::ErrorCode::OK)
  {
    SetFault(BalanceFault::MotorCommand, enable_result);
    return;
  }
  if (config_.zero_motor_on_task_start)
  {
    const auto zero_result = config_.motor->SetCurrentPositionAsZero();
    if (zero_result != LibXR::ErrorCode::OK)
    {
      AbortTask(BalanceFault::MotorCommand);
      return;
    }
  }

  const uint32_t now_ms = LibXR::Thread::GetTime();
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    status_.run_state = BalanceRunState::Running;
    status_.fault = BalanceFault::None;
    status_.motor_enabled = true;
    status_.motor_error = static_cast<int32_t>(LibXR::ErrorCode::OK);
    status_.elapsed_ms = 0;
    status_.task_phase = status_.selected_task == ContestTask::Task3
                             ? kTask3MovePositive
                             : 0;
    status_.motor_position_degrees = 0.0F;
    status_.motor_position_valid = config_.zero_motor_on_task_start;
    status_.target_position_cm =
        status_.selected_task == ContestTask::Task6 ? config_.task6_target_cm : 0.0F;
    if (status_.selected_task == ContestTask::Task3)
    {
      status_.target_position_cm = config_.task3_positive_cm;
    }
    task_start_time_ms_ = now_ms;
    target_settle_start_ms_ = 0;
    last_motor_position_poll_ms_ =
        now_ms - config_.motor_position_poll_period_ms;
    motion_command_active_ = false;
    last_command_angle_degrees_ = 0.0F;
  }
  PublishStatus();
}

void BalanceController::AbortTask(BalanceFault fault)
{
  const auto stop_result = config_.motor->StopImmediately();
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    status_.run_state = fault == BalanceFault::None ? BalanceRunState::Ready
                                                     : BalanceRunState::Fault;
    status_.fault = fault;
    status_.motor_error = static_cast<int32_t>(stop_result);
    status_.motor_enabled = stop_result == LibXR::ErrorCode::OK;
    status_.task_phase = 0;
    motion_command_active_ = false;
    target_settle_start_ms_ = 0;
  }
  PublishStatus();
}

void BalanceController::CompleteTask()
{
  const auto stop_result = config_.motor->StopImmediately();
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    status_.run_state = stop_result == LibXR::ErrorCode::OK
                            ? BalanceRunState::Completed
                            : BalanceRunState::Fault;
    status_.fault = stop_result == LibXR::ErrorCode::OK ? BalanceFault::None
                                                         : BalanceFault::MotorCommand;
    status_.motor_error = static_cast<int32_t>(stop_result);
    status_.motor_enabled = stop_result == LibXR::ErrorCode::OK;
    status_.task_phase = 0;
    motion_command_active_ = false;
  }
  PublishStatus();
}

void BalanceController::UpdateControl(uint32_t now_ms)
{
  Maxican::BallMeasurement measurement;
  ContestTask selected_task = ContestTask::Task3;
  uint32_t task_start_ms = 0;
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    if (status_.run_state != BalanceRunState::Running)
    {
      return;
    }
    selected_task = status_.selected_task;
    task_start_ms = task_start_time_ms_;
    measurement = latest_measurement_;
    status_.elapsed_ms = static_cast<uint32_t>(now_ms - task_start_time_ms_);
    status_.vision_age_ms = has_measurement_
                                ? static_cast<uint32_t>(now_ms - measurement.received_time_ms)
                                : UINT32_MAX;
    status_.position_cm = measurement.position_cm;
    status_.velocity_pixel_s = measurement.velocity_pixel_s;
    status_.confidence = measurement.confidence;
    status_.source_frame_time_ms = measurement.frame_time_ms;
    status_.vision_valid = has_measurement_ &&
                           IsFreshMeasurement(measurement, now_ms,
                                              config_.vision_timeout_ms) &&
                           measurement.confidence >= config_.min_confidence &&
                           std::fabs(measurement.position_cm) <=
                               config_.max_abs_position_cm;
  }

  if (!has_measurement_ ||
      static_cast<uint32_t>(now_ms - measurement.received_time_ms) >
          config_.vision_timeout_ms)
  {
    AbortTask(BalanceFault::VisionTimeout);
    return;
  }
  if (!measurement.valid || measurement.confidence < config_.min_confidence ||
      std::fabs(measurement.position_cm) > config_.max_abs_position_cm)
  {
    AbortTask(BalanceFault::VisionInvalid);
    return;
  }
  if (static_cast<uint32_t>(now_ms - task_start_ms) >= TimeLimitMs(selected_task))
  {
    if (selected_task == ContestTask::Task3)
    {
      AbortTask(BalanceFault::TaskTimeout);
    }
    else
    {
      CompleteTask();
    }
    return;
  }

  UpdateTaskTarget(now_ms);
  if (GetStatus().run_state != BalanceRunState::Running)
  {
    return;
  }

  const BalanceStatus status = GetStatus();
  const float unconstrained_angle = config_.angle_offset_degrees +
                                    config_.position_gain_degrees_per_cm *
                                        (measurement.position_cm -
                                         status.target_position_cm) +
                                    config_.velocity_gain_degrees_per_pixel_s *
                                        measurement.velocity_pixel_s;
  if (!std::isfinite(unconstrained_angle))
  {
    AbortTask(BalanceFault::Configuration);
    return;
  }

  const float target_angle = std::clamp(unconstrained_angle,
                                        config_.min_angle_degrees,
                                        config_.max_angle_degrees);
  bool send_command = false;
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    status_.target_angle_degrees = target_angle;
    send_command = !motion_command_active_ ||
                   std::fabs(target_angle - last_command_angle_degrees_) >=
                       config_.minimum_command_delta_degrees;
  }
  if (send_command)
  {
    const auto result = config_.motor->MoveToAbsoluteAngle(
        target_angle, config_.motor_speed_rpm, config_.motor_acceleration);
    if (result != LibXR::ErrorCode::OK)
    {
      AbortTask(BalanceFault::MotorCommand);
      return;
    }
    LibXR::Mutex::LockGuard lock(mutex_);
    last_command_angle_degrees_ = target_angle;
    motion_command_active_ = true;
    status_.motor_error = static_cast<int32_t>(LibXR::ErrorCode::OK);
  }
  UpdateMotorPosition(now_ms);
  PublishStatus();
}

void BalanceController::UpdateMotorPosition(uint32_t now_ms)
{
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    if (static_cast<uint32_t>(now_ms - last_motor_position_poll_ms_) <
        config_.motor_position_poll_period_ms)
    {
      return;
    }
    last_motor_position_poll_ms_ = now_ms;
  }

  float position_degrees = 0.0F;
  const auto result = config_.motor->ReadRealtimeAngle(position_degrees);
  LibXR::Mutex::LockGuard lock(mutex_);
  if (result == LibXR::ErrorCode::OK)
  {
    status_.motor_position_degrees = position_degrees;
    status_.motor_position_valid = true;
  }
  else
  {
    status_.motor_position_valid = false;
  }
}

void BalanceController::UpdateTaskTarget(uint32_t now_ms)
{
  BalanceStatus snapshot = GetStatus();
  if (snapshot.selected_task != ContestTask::Task3)
  {
    return;
  }

  const float error_cm =
      std::fabs(snapshot.position_cm - snapshot.target_position_cm);
  if (error_cm > config_.position_tolerance_cm)
  {
    target_settle_start_ms_ = 0;
    return;
  }
  if (target_settle_start_ms_ == 0)
  {
    target_settle_start_ms_ = now_ms;
    return;
  }
  if (static_cast<uint32_t>(now_ms - target_settle_start_ms_) < config_.settle_time_ms)
  {
    return;
  }

  if (snapshot.task_phase == kTask3MovePositive)
  {
    {
      LibXR::Mutex::LockGuard lock(mutex_);
      status_.task_phase = kTask3MoveNegative;
      status_.target_position_cm = config_.task3_negative_cm;
      target_settle_start_ms_ = 0;
      motion_command_active_ = false;
    }
    PublishStatus();
    return;
  }
  CompleteTask();
}

void BalanceController::SetFault(BalanceFault fault, LibXR::ErrorCode motor_error)
{
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    status_.run_state = BalanceRunState::Fault;
    status_.fault = fault;
    status_.motor_error = static_cast<int32_t>(motor_error);
    status_.task_phase = 0;
    motion_command_active_ = false;
  }
  PublishStatus();
}

void BalanceController::PublishStatus()
{
  BalanceStatus snapshot = GetStatus();
  state_topic_.Publish(snapshot, LibXR::Timebase::GetMicroseconds());
}

bool BalanceController::IsConfigurationValid() const
{
  return config_.motor != nullptr && config_.mailbox != nullptr &&
         std::isfinite(config_.angle_offset_degrees) &&
         std::isfinite(config_.position_gain_degrees_per_cm) &&
         std::isfinite(config_.velocity_gain_degrees_per_pixel_s) &&
         std::isfinite(config_.min_angle_degrees) &&
         std::isfinite(config_.max_angle_degrees) &&
         config_.min_angle_degrees <= config_.max_angle_degrees &&
         std::isfinite(config_.minimum_command_delta_degrees) &&
         config_.minimum_command_delta_degrees >= 0.0F &&
         config_.min_confidence >= 0.0F && config_.min_confidence <= 1.0F &&
         std::isfinite(config_.task3_positive_cm) &&
         std::isfinite(config_.task3_negative_cm) &&
         std::isfinite(config_.task6_target_cm) &&
         std::isfinite(config_.position_tolerance_cm) &&
         config_.position_tolerance_cm > 0.0F &&
         std::isfinite(config_.max_abs_position_cm) &&
         config_.max_abs_position_cm > 0.0F && config_.motor_speed_rpm > 0 &&
         config_.control_period_ms > 0 && config_.vision_timeout_ms > 0 &&
         config_.settle_time_ms > 0 && config_.motor_position_poll_period_ms > 0;
}

ContestTask BalanceController::NextTask(ContestTask task)
{
  switch (task)
  {
    case ContestTask::Task3:
      return ContestTask::Task4;
    case ContestTask::Task4:
      return ContestTask::Task5;
    case ContestTask::Task5:
      return ContestTask::Task6;
    case ContestTask::Task6:
    default:
      return ContestTask::Task3;
  }
}

uint32_t BalanceController::TimeLimitMs(ContestTask task)
{
  switch (task)
  {
    case ContestTask::Task3:
      return kTask3TimeLimitMs;
    case ContestTask::Task4:
      return kTask4TimeLimitMs;
    case ContestTask::Task5:
    case ContestTask::Task6:
    default:
      return kLapTimeLimitMs;
  }
}

void BalanceControlThread(BalanceController* controller)
{
  if (controller != nullptr)
  {
    controller->Run();
  }
}

}  // namespace BuTask
