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
constexpr float kTask6TargetStepCm = 1.0F;
constexpr uint8_t kTask3MovePositive = 1;
constexpr uint8_t kTask3MoveNegative = 2;
constexpr uint8_t kViewTestMovePositive = 3;
constexpr uint8_t kViewTestMoveNegative = 4;
constexpr float kViewTestPositiveDegrees = 10.0F;
constexpr float kViewTestNegativeDegrees = -10.0F;
constexpr float kViewTestPositionToleranceDegrees = 0.5F;
constexpr uint16_t kViewTestSpeedRpm = 60;
constexpr uint8_t kViewTestAcceleration = 220;
constexpr uint32_t kViewTestPhaseTimeoutMs = 3000;
constexpr uint32_t kViewTestPositionPollPeriodMs = 50;
constexpr float kMotorDebugMaxAbsTargetDegrees = 10.0F;
constexpr float kMotorDebugPositionToleranceDegrees = 0.5F;
constexpr uint16_t kMotorDebugMaxSpeedRpm = 60;
constexpr uint8_t kMotorDebugMaxAcceleration = 50;
constexpr uint32_t kMotorDebugPollPeriodMs = 50;
constexpr uint32_t kMotorDebugMotionTimeoutMs = 3000;
constexpr uint8_t kMotorDebugRequiredSettledSamples = 3;
constexpr uint32_t kMotorConfigurationRetryPeriodMs = 1000;
constexpr float kControlTuningMaxAbsOffsetDegrees = 10.0F;
constexpr float kControlTuningMaxAbsPositionGain = 10.0F;
constexpr float kControlTuningMaxAbsIntegralGain = 2.0F;
constexpr float kControlTuningMaxIntegralLimitDegrees = 10.0F;
constexpr float kControlTuningMaxAbsVelocityGain = 0.5F;
constexpr float kControlTuningMinAngleLimitDegrees = 1.0F;
constexpr float kControlTuningMaxAngleLimitDegrees = 20.0F;
constexpr float kControlTuningMaxCommandDeltaDegrees = 5.0F;
constexpr uint32_t kVisionRejectReasonInvalidFlag = 1U << 0U;
constexpr uint32_t kVisionRejectReasonLowConfidence = 1U << 1U;
constexpr uint32_t kVisionRejectReasonOutOfRange = 1U << 2U;

bool IsFreshMeasurement(const Maxican::BallMeasurement& measurement,
                        uint32_t now_ms, uint32_t timeout_ms)
{
  return measurement.valid &&
         static_cast<uint32_t>(now_ms - measurement.received_time_ms) <= timeout_ms;
}

float ShortestAngleError(float measured_degrees, float target_degrees)
{
  float error = std::fmod(measured_degrees - target_degrees, 360.0F);
  if (error > 180.0F)
  {
    error -= 360.0F;
  }
  else if (error < -180.0F)
  {
    error += 360.0F;
  }
  return error;
}

}  // namespace

extern "C"
{
volatile MotorDebugMailbox g_motor_debug_mailbox{};
}

BalanceController::BalanceController(BalanceControllerConfig config)
    : config_(config),
      state_topic_(LibXR::Topic::CreateTopic<BalanceStatus>(kStateTopicName))
{
  const BalanceControlProfile& profile = config_.task3_positive_pid;
  g_motor_debug_mailbox.control_angle_offset_degrees =
      config_.angle_offset_degrees;
  g_motor_debug_mailbox.control_position_gain_degrees_per_cm =
      profile.position_gain_degrees_per_cm;
  g_motor_debug_mailbox.control_integral_gain_degrees_per_cm_s =
      profile.integral_gain_degrees_per_cm_s;
  g_motor_debug_mailbox.control_integral_limit_degrees =
      profile.integral_limit_degrees;
  g_motor_debug_mailbox.control_velocity_gain_degrees_per_pixel_s =
      profile.velocity_gain_degrees_per_pixel_s;
  g_motor_debug_mailbox.control_angle_limit_degrees =
      std::max(std::fabs(profile.min_angle_degrees),
               std::fabs(profile.max_angle_degrees));
  g_motor_debug_mailbox.control_minimum_command_delta_degrees =
      profile.minimum_command_delta_degrees;
  g_motor_debug_mailbox.control_tuning_valid = 1;
}

void BalanceController::SelectNextTask()
{
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    if (status_.run_state == BalanceRunState::Running)
    {
      if (status_.selected_task == ContestTask::Monitor)
      {
        view_test_reverse_requested_ = true;
      }
      return;
    }
    if (g_motor_debug_mailbox.motion_active != 0)
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

void BalanceController::StepTask6Target(int8_t direction)
{
  bool changed = false;
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    if (direction == 0 || status_.selected_task != ContestTask::Task6 ||
        status_.run_state == BalanceRunState::Running)
    {
      return;
    }

    const float target_limit_cm =
        config_.max_abs_position_cm - config_.position_tolerance_cm;
    float next_target_cm = config_.task6_target_cm;
    if (direction > 0)
    {
      next_target_cm = next_target_cm >= target_limit_cm
                           ? -target_limit_cm
                           : std::min(next_target_cm + kTask6TargetStepCm,
                                      target_limit_cm);
    }
    else
    {
      next_target_cm = next_target_cm <= -target_limit_cm
                           ? target_limit_cm
                           : std::max(next_target_cm - kTask6TargetStepCm,
                                      -target_limit_cm);
    }

    config_.task6_target_cm = next_target_cm;
    status_.target_position_cm = next_target_cm;
    changed = true;
  }

  if (changed)
  {
    PublishStatus();
  }
}

void BalanceController::RequestSetSingleTurnHomingZero()
{
  LibXR::Mutex::LockGuard lock(mutex_);
  if (status_.selected_task == ContestTask::Monitor &&
      status_.run_state != BalanceRunState::Running)
  {
    set_homing_zero_requested_ = true;
    homing_requested_ = false;
  }
}

void BalanceController::RequestHoming()
{
  LibXR::Mutex::LockGuard lock(mutex_);
  if (status_.selected_task == ContestTask::Monitor &&
      status_.run_state != BalanceRunState::Running)
  {
    homing_requested_ = true;
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
#if defined(OZONE_AUTOMATED_IDLE_MODE_CYCLE_TEST)
  uint32_t last_ozone_idle_mode_change_ms = LibXR::Thread::GetTime();
#endif
  while (true)
  {
    const uint32_t cycle_start_ms = LibXR::Thread::GetTime();
    HandleRequests();
    HandleViewKeyRequests();

    BalanceStatus status = GetStatus();
#if defined(OZONE_AUTOMATED_IDLE_MODE_CYCLE_TEST)
    // Hardware-in-the-loop display check: advance through every selectable
    // mode without requesting RUN, so no motor command can be emitted.
    if (status.run_state != BalanceRunState::Running &&
        static_cast<uint32_t>(cycle_start_ms - last_ozone_idle_mode_change_ms) >=
            2000U)
    {
      SelectNextTask();
      last_ozone_idle_mode_change_ms = cycle_start_ms;
      status = GetStatus();
    }
#endif
    if (status.run_state == BalanceRunState::Running)
    {
      if (status.selected_task == ContestTask::Monitor)
      {
        UpdateViewMotorTest(LibXR::Thread::GetTime());
      }
      else
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
    }
    else
    {
      const uint32_t now_ms = LibXR::Thread::GetTime();
      // The Ozone motor-debug mailbox remains VIEW-only.  Vision telemetry,
      // however, must remain live in every inactive task so that the operator
      // can verify the camera before pressing RUN.
      if (status.selected_task == ContestTask::Monitor)
      {
        HandleMotorDebugMailbox(now_ms);
      }
      UpdateIdleStatus(now_ms);
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

void BalanceController::HandleViewKeyRequests()
{
  bool set_homing_zero = false;
  bool home = false;
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    set_homing_zero = set_homing_zero_requested_;
    home = homing_requested_;
    set_homing_zero_requested_ = false;
    homing_requested_ = false;
    if (status_.selected_task != ContestTask::Monitor ||
        status_.run_state == BalanceRunState::Running)
    {
      set_homing_zero = false;
      home = false;
    }
  }

  if (!set_homing_zero && !home)
  {
    return;
  }

  auto result = config_.motor->Enable(true);
  if (result == LibXR::ErrorCode::OK && set_homing_zero)
  {
    result = config_.motor->SetSingleTurnHomingZero(true);
  }
  if (result == LibXR::ErrorCode::OK && home)
  {
    result = config_.motor->TriggerHoming(0, false);
  }
  if (result != LibXR::ErrorCode::OK)
  {
    SetFault(BalanceFault::MotorCommand, result);
    return;
  }

  {
    LibXR::Mutex::LockGuard lock(mutex_);
    status_.motor_enabled = true;
    status_.motor_error = static_cast<int32_t>(result);
    if (set_homing_zero)
    {
      status_.motor_position_degrees = 0.0F;
      status_.motor_position_valid = true;
      status_.target_angle_degrees = 0.0F;
    }
    if (home)
    {
      status_.target_angle_degrees = 0.0F;
      status_.motor_position_valid = false;
    }
  }
  PublishStatus();
}

void BalanceController::StartTask()
{
  g_motor_debug_mailbox.task_start_stage = 0x10;  // Request accepted.
  g_motor_debug_mailbox.task_start_result = static_cast<int32_t>(LibXR::ErrorCode::OK);
  bool start_view_test = false;
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    start_view_test = status_.selected_task == ContestTask::Monitor;
  }
  if (start_view_test)
  {
    StartViewMotorTest();
    return;
  }

  if (!motor_configuration_checked_)
  {
    const auto configuration_result =
        RefreshMotorConfiguration(LibXR::Thread::GetTime());
    if (configuration_result != LibXR::ErrorCode::OK &&
        configuration_result != LibXR::ErrorCode::NOT_SUPPORT)
    {
      g_motor_debug_mailbox.task_start_stage = 0x20;  // Configuration query failed.
      g_motor_debug_mailbox.task_start_result = static_cast<int32_t>(configuration_result);
      SetFault(BalanceFault::Configuration, configuration_result);
      return;
    }
  }

  const auto enable_result = config_.motor->Enable(true);
  if (enable_result != LibXR::ErrorCode::OK)
  {
    g_motor_debug_mailbox.task_start_stage = 0x30;  // Enable failed.
    g_motor_debug_mailbox.task_start_result = static_cast<int32_t>(enable_result);
    SetFault(BalanceFault::MotorCommand, enable_result);
    return;
  }
  g_motor_debug_mailbox.task_start_stage = 0x40;  // Enable acknowledged.
  if (config_.zero_motor_on_task_start)
  {
    const auto zero_result = config_.motor->SetCurrentPositionAsZero();
    if (zero_result != LibXR::ErrorCode::OK)
    {
      g_motor_debug_mailbox.task_start_stage = 0x50;  // Set-zero failed.
      g_motor_debug_mailbox.task_start_result = static_cast<int32_t>(zero_result);
      AbortTask(BalanceFault::MotorCommand);
      return;
    }
    g_motor_debug_mailbox.task_start_stage = 0x60;  // Set-zero acknowledged.
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
    // A rejected/lost frame is tolerated only for the same bounded interval
    // already used for a missing/stale frame. During this grace interval the
    // controller sends no new command and the motor merely holds the last
    // accepted setpoint.
    last_accepted_measurement_time_ms_ = now_ms;
    target_settle_start_ms_ = 0;
    last_motor_position_poll_ms_ =
        now_ms - config_.motor_position_poll_period_ms;
    motion_command_active_ = false;
    last_command_angle_degrees_ = 0.0F;
    position_error_integral_cm_s_ = 0.0F;
    last_control_time_ms_ = now_ms;
    control_command_count_ = 0;
  }
  g_motor_debug_mailbox.task_position_sample_count = 0;
  g_motor_debug_mailbox.task_min_position_cm = 0.0F;
  g_motor_debug_mailbox.task_max_position_cm = 0.0F;
  g_motor_debug_mailbox.task_best_abs_error_cm = 0.0F;
  g_motor_debug_mailbox.task_best_position_cm = 0.0F;
  g_motor_debug_mailbox.task_in_tolerance_sample_count = 0;
  g_motor_debug_mailbox.task_target_settled = 0;
  g_motor_debug_mailbox.task_settled_position_cm = 0.0F;
  g_motor_debug_mailbox.task_vision_reject_count = 0;
  g_motor_debug_mailbox.task_vision_reject_reason = 0;
  g_motor_debug_mailbox.task_rejected_measurement_valid = 0;
  g_motor_debug_mailbox.task_rejected_position_cm = 0.0F;
  g_motor_debug_mailbox.task_rejected_confidence = 0.0F;
  g_motor_debug_mailbox.task_rejected_duration_ms = 0;
  g_motor_debug_mailbox.task_start_stage = 0x70;  // Entered RUNNING.
  g_motor_debug_mailbox.task_start_result = static_cast<int32_t>(LibXR::ErrorCode::OK);
  PublishStatus();
}

void BalanceController::StartViewMotorTest()
{
  if (!motor_configuration_checked_)
  {
    const auto configuration_result =
        RefreshMotorConfiguration(LibXR::Thread::GetTime());
    if (configuration_result != LibXR::ErrorCode::OK &&
        configuration_result != LibXR::ErrorCode::NOT_SUPPORT)
    {
      SetFault(BalanceFault::Configuration, configuration_result);
      return;
    }
  }

  const auto enable_result = config_.motor->Enable(true);
  if (enable_result != LibXR::ErrorCode::OK)
  {
    SetFault(BalanceFault::MotorCommand, enable_result);
    return;
  }

  const auto zero_result = config_.motor->SetCurrentPositionAsZero();
  if (zero_result != LibXR::ErrorCode::OK)
  {
    AbortTask(BalanceFault::MotorCommand);
    return;
  }

  const auto move_result = config_.motor->MoveToAbsoluteAngle(
      kViewTestPositiveDegrees, kViewTestSpeedRpm, kViewTestAcceleration);
  if (move_result != LibXR::ErrorCode::OK)
  {
    AbortTask(BalanceFault::MotorCommand);
    return;
  }

  const uint32_t now_ms = LibXR::Thread::GetTime();
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    status_.run_state = BalanceRunState::Running;
    status_.fault = BalanceFault::None;
    status_.motor_enabled = true;
    status_.motor_position_valid = true;
    status_.motor_error = static_cast<int32_t>(LibXR::ErrorCode::OK);
    status_.elapsed_ms = 0;
    status_.task_phase = kViewTestMovePositive;
    status_.target_position_cm = 0.0F;
    status_.target_angle_degrees = kViewTestPositiveDegrees;
    status_.motor_position_degrees = 0.0F;
    task_start_time_ms_ = now_ms;
    view_test_phase_start_ms_ = now_ms;
    view_test_last_position_poll_ms_ = now_ms - kViewTestPositionPollPeriodMs;
    view_test_reverse_requested_ = false;
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
    position_error_integral_cm_s_ = 0.0F;
    last_control_time_ms_ = 0;
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
    position_error_integral_cm_s_ = 0.0F;
    last_control_time_ms_ = 0;
  }
  PublishStatus();
}

void BalanceController::UpdateControl(uint32_t now_ms)
{
  Maxican::BallMeasurement measurement;
  ContestTask selected_task = ContestTask::Task3;
  uint8_t task_phase = 0;
  uint32_t task_start_ms = 0;
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    if (status_.run_state != BalanceRunState::Running)
    {
      return;
    }
    selected_task = status_.selected_task;
    task_phase = status_.task_phase;
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
    const BalanceControlProfile& profile =
        ControlProfileFor(selected_task, task_phase);
    status_.vision_valid = has_measurement_ &&
                           IsFreshMeasurement(measurement, now_ms,
                                              config_.vision_timeout_ms) &&
                           measurement.confidence >= profile.min_confidence &&
                           std::fabs(measurement.position_cm) <=
                               config_.max_abs_position_cm;
  }

  const BalanceControlProfile& control_profile =
      ControlProfileFor(selected_task, task_phase);

  if (!has_measurement_ ||
      static_cast<uint32_t>(now_ms - measurement.received_time_ms) >
          config_.vision_timeout_ms)
  {
    g_motor_debug_mailbox.task_start_stage = 0xE1;  // Vision timeout during RUN.
    AbortTask(BalanceFault::VisionTimeout);
    return;
  }
  if (!measurement.valid ||
      measurement.confidence < control_profile.min_confidence ||
      std::fabs(measurement.position_cm) > config_.max_abs_position_cm)
  {
    uint32_t reject_reason = 0;
    if (!measurement.valid)
    {
      reject_reason |= kVisionRejectReasonInvalidFlag;
    }
    if (measurement.confidence < control_profile.min_confidence)
    {
      reject_reason |= kVisionRejectReasonLowConfidence;
    }
    if (std::fabs(measurement.position_cm) > config_.max_abs_position_cm)
    {
      reject_reason |= kVisionRejectReasonOutOfRange;
    }
    const uint32_t rejected_duration_ms =
        static_cast<uint32_t>(now_ms - last_accepted_measurement_time_ms_);
    g_motor_debug_mailbox.task_vision_reject_count =
        g_motor_debug_mailbox.task_vision_reject_count + 1U;
    g_motor_debug_mailbox.task_vision_reject_reason = reject_reason;
    g_motor_debug_mailbox.task_rejected_measurement_valid =
        measurement.valid ? 1U : 0U;
    g_motor_debug_mailbox.task_rejected_position_cm = measurement.position_cm;
    g_motor_debug_mailbox.task_rejected_confidence = measurement.confidence;
    g_motor_debug_mailbox.task_rejected_duration_ms = rejected_duration_ms;
    if (rejected_duration_ms >= config_.invalid_measurement_neutralize_ms)
    {
      bool send_neutral_command = false;
      {
        LibXR::Mutex::LockGuard lock(mutex_);
        send_neutral_command =
            !motion_command_active_ ||
            std::fabs(last_command_angle_degrees_ -
                      config_.angle_offset_degrees) >=
                control_profile.minimum_command_delta_degrees;
      }
      if (send_neutral_command)
      {
        const auto neutral_result = config_.motor->MoveToAbsoluteAngle(
            config_.angle_offset_degrees, config_.motor_speed_rpm,
            config_.motor_acceleration);
        if (neutral_result != LibXR::ErrorCode::OK)
        {
          g_motor_debug_mailbox.task_start_stage =
              0xE5;  // Neutral command after vision loss failed.
          g_motor_debug_mailbox.control_last_command_result =
              static_cast<int32_t>(neutral_result);
          AbortTask(BalanceFault::MotorCommand);
          return;
        }
        {
          LibXR::Mutex::LockGuard lock(mutex_);
          last_command_angle_degrees_ = config_.angle_offset_degrees;
          motion_command_active_ = true;
          ++control_command_count_;
          status_.target_angle_degrees = config_.angle_offset_degrees;
          status_.motor_error = static_cast<int32_t>(LibXR::ErrorCode::OK);
        }
        g_motor_debug_mailbox.control_last_command_angle_degrees =
            config_.angle_offset_degrees;
        g_motor_debug_mailbox.control_last_command_result =
            static_cast<int32_t>(LibXR::ErrorCode::OK);
      }
    }
    if (rejected_duration_ms > config_.invalid_measurement_grace_ms)
    {
      g_motor_debug_mailbox.task_start_stage =
          0xE2;  // Vision rejected continuously during RUN.
      AbortTask(BalanceFault::VisionInvalid);
    }
    last_control_time_ms_ = now_ms;
    return;
  }
  last_accepted_measurement_time_ms_ = now_ms;
  if (static_cast<uint32_t>(now_ms - task_start_ms) >= TimeLimitMs(selected_task))
  {
    if (selected_task == ContestTask::Task3)
    {
      g_motor_debug_mailbox.task_start_stage = 0xE3;  // T3 time limit elapsed.
      AbortTask(BalanceFault::TaskTimeout);
    }
    else
    {
      CompleteTask();
    }
    return;
  }

  const BalanceStatus target_snapshot = GetStatus();
  const float absolute_error_cm =
      std::fabs(measurement.position_cm - target_snapshot.target_position_cm);
  if (g_motor_debug_mailbox.task_position_sample_count == 0)
  {
    g_motor_debug_mailbox.task_min_position_cm = measurement.position_cm;
    g_motor_debug_mailbox.task_max_position_cm = measurement.position_cm;
    g_motor_debug_mailbox.task_best_abs_error_cm = absolute_error_cm;
    g_motor_debug_mailbox.task_best_position_cm = measurement.position_cm;
  }
  else
  {
    const float previous_min_position_cm =
        g_motor_debug_mailbox.task_min_position_cm;
    const float previous_max_position_cm =
        g_motor_debug_mailbox.task_max_position_cm;
    g_motor_debug_mailbox.task_min_position_cm =
        std::min(previous_min_position_cm, measurement.position_cm);
    g_motor_debug_mailbox.task_max_position_cm =
        std::max(previous_max_position_cm, measurement.position_cm);
    if (absolute_error_cm < g_motor_debug_mailbox.task_best_abs_error_cm)
    {
      g_motor_debug_mailbox.task_best_abs_error_cm = absolute_error_cm;
      g_motor_debug_mailbox.task_best_position_cm = measurement.position_cm;
    }
  }
  g_motor_debug_mailbox.task_position_sample_count =
      g_motor_debug_mailbox.task_position_sample_count + 1U;
  if (absolute_error_cm <= config_.position_tolerance_cm)
  {
    g_motor_debug_mailbox.task_in_tolerance_sample_count =
        g_motor_debug_mailbox.task_in_tolerance_sample_count + 1U;
  }

  UpdateTaskTarget(now_ms);
  if (GetStatus().run_state != BalanceRunState::Running)
  {
    return;
  }

  const BalanceStatus status = GetStatus();
  const BalanceControlProfile& active_profile =
      ControlProfileFor(status.selected_task, status.task_phase);
  const float position_error_cm =
      measurement.position_cm - status.target_position_cm;
  const uint32_t control_elapsed_ms =
      last_control_time_ms_ == 0
          ? 0U
          : static_cast<uint32_t>(now_ms - last_control_time_ms_);
  last_control_time_ms_ = now_ms;

  if (std::fabs(active_profile.integral_gain_degrees_per_cm_s) > 0.000001F)
  {
    position_error_integral_cm_s_ +=
        position_error_cm * static_cast<float>(control_elapsed_ms) * 0.001F;
    if (active_profile.integral_limit_degrees >= 0.0F)
    {
      const float max_integral_cm_s =
          active_profile.integral_limit_degrees /
          std::fabs(active_profile.integral_gain_degrees_per_cm_s);
      position_error_integral_cm_s_ =
          std::clamp(position_error_integral_cm_s_, -max_integral_cm_s,
                     max_integral_cm_s);
    }
  }
  else
  {
    position_error_integral_cm_s_ = 0.0F;
  }

  const float integral_angle =
      active_profile.integral_gain_degrees_per_cm_s *
      position_error_integral_cm_s_;
  const float unconstrained_angle = config_.angle_offset_degrees +
                                    active_profile.position_gain_degrees_per_cm *
                                        position_error_cm +
                                    integral_angle +
                                    active_profile.velocity_gain_degrees_per_pixel_s *
                                        measurement.velocity_pixel_s;
  if (!std::isfinite(unconstrained_angle))
  {
    g_motor_debug_mailbox.task_start_stage = 0xE4;  // Control configuration invalid.
    AbortTask(BalanceFault::Configuration);
    return;
  }

  const float target_angle = std::clamp(unconstrained_angle,
                                         active_profile.min_angle_degrees,
                                         active_profile.max_angle_degrees);
  bool send_command = false;
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    status_.target_angle_degrees = target_angle;
    send_command = !motion_command_active_ ||
                   std::fabs(target_angle - last_command_angle_degrees_) >=
                       active_profile.minimum_command_delta_degrees;
  }
  if (send_command)
  {
    // X firmware supports the FB direct-position command (immediate target
    // tracking); Emm firmware does not and rejects it with NOT_SUPPORT, so it
    // must fall back to the FD absolute-position command instead.
    const auto result =
        config_.motor->SupportsDirectPosition()
            ? config_.motor->MoveToAbsoluteAngleDirect(target_angle,
                                                        config_.motor_speed_rpm)
            : config_.motor->MoveToAbsoluteAngle(target_angle,
                                                 config_.motor_speed_rpm,
                                                 config_.motor_acceleration);
    if (result != LibXR::ErrorCode::OK)
    {
      g_motor_debug_mailbox.task_start_stage =
          0xE5;  // Position command failed.
      g_motor_debug_mailbox.control_last_command_result = static_cast<int32_t>(result);
      AbortTask(BalanceFault::MotorCommand);
      return;
    }
    LibXR::Mutex::LockGuard lock(mutex_);
    last_command_angle_degrees_ = target_angle;
    motion_command_active_ = true;
    ++control_command_count_;
    status_.motor_error = static_cast<int32_t>(LibXR::ErrorCode::OK);
    g_motor_debug_mailbox.control_last_command_angle_degrees = target_angle;
    g_motor_debug_mailbox.control_last_command_result =
        static_cast<int32_t>(LibXR::ErrorCode::OK);
  }
  UpdateMotorPosition(now_ms);
  PublishStatus();
}

void BalanceController::UpdateViewMotorTest(uint32_t now_ms)
{
  const BalanceStatus snapshot = GetStatus();
  if (snapshot.run_state != BalanceRunState::Running ||
      snapshot.selected_task != ContestTask::Monitor)
  {
    return;
  }

  const float target_degrees = snapshot.task_phase == kViewTestMovePositive
                                   ? kViewTestPositiveDegrees
                                   : kViewTestNegativeDegrees;
  if (snapshot.task_phase != kViewTestMovePositive &&
      snapshot.task_phase != kViewTestMoveNegative)
  {
    AbortTask(BalanceFault::Configuration);
    return;
  }

  bool reverse_requested = false;
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    reverse_requested = view_test_reverse_requested_;
    view_test_reverse_requested_ = false;
  }
  if (reverse_requested && snapshot.task_phase != kViewTestMoveNegative)
  {
    const auto move_result = config_.motor->MoveToAbsoluteAngle(
        kViewTestNegativeDegrees, kViewTestSpeedRpm, kViewTestAcceleration);
    if (move_result != LibXR::ErrorCode::OK)
    {
      AbortTask(BalanceFault::MotorCommand);
      return;
    }

    {
      LibXR::Mutex::LockGuard lock(mutex_);
      status_.task_phase = kViewTestMoveNegative;
      status_.target_angle_degrees = kViewTestNegativeDegrees;
      status_.motor_error = static_cast<int32_t>(move_result);
      view_test_phase_start_ms_ = now_ms;
    }
    PublishStatus();
    return;
  }

  bool phase_timeout = false;
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    status_.elapsed_ms = static_cast<uint32_t>(now_ms - task_start_time_ms_);
    phase_timeout = static_cast<uint32_t>(now_ms - view_test_phase_start_ms_) >=
                    kViewTestPhaseTimeoutMs;
    if (!phase_timeout &&
        static_cast<uint32_t>(now_ms - view_test_last_position_poll_ms_) >=
            kViewTestPositionPollPeriodMs)
    {
      view_test_last_position_poll_ms_ = now_ms;
    }
    else if (!phase_timeout)
    {
      return;
    }
  }
  if (phase_timeout)
  {
    AbortTask(BalanceFault::TaskTimeout);
    return;
  }

  float position_degrees = 0.0F;
  const auto read_result = config_.motor->ReadRealtimeAngle(position_degrees);
  if (read_result != LibXR::ErrorCode::OK)
  {
    AbortTask(BalanceFault::MotorCommand);
    return;
  }

  {
    LibXR::Mutex::LockGuard lock(mutex_);
    status_.motor_position_degrees = position_degrees;
    status_.motor_position_valid = true;
    status_.motor_error = static_cast<int32_t>(read_result);
  }

  if (std::fabs(ShortestAngleError(position_degrees, target_degrees)) <=
      kViewTestPositionToleranceDegrees)
  {
    if (snapshot.task_phase == kViewTestMoveNegative)
    {
      CompleteTask();
      return;
    }

    const auto move_result = config_.motor->MoveToAbsoluteAngle(
        kViewTestNegativeDegrees, kViewTestSpeedRpm, kViewTestAcceleration);
    if (move_result != LibXR::ErrorCode::OK)
    {
      AbortTask(BalanceFault::MotorCommand);
      return;
    }

    {
      LibXR::Mutex::LockGuard lock(mutex_);
      status_.task_phase = kViewTestMoveNegative;
      status_.target_angle_degrees = kViewTestNegativeDegrees;
      status_.motor_error = static_cast<int32_t>(move_result);
      view_test_phase_start_ms_ = now_ms;
    }
    PublishStatus();
    return;
  }

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
  g_motor_debug_mailbox.motor_position_read_result =
      static_cast<int32_t>(result);
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

void BalanceController::FinishMotorDebugRequest(uint32_t sequence,
                                                LibXR::ErrorCode result)
{
  g_motor_debug_mailbox.result = static_cast<int32_t>(result);
  g_motor_debug_mailbox.completed_sequence = sequence;
  g_motor_debug_mailbox.last_update_ms = LibXR::Thread::GetTime();
}

LibXR::ErrorCode BalanceController::RefreshMotorConfiguration(uint32_t now_ms)
{
  BujinMotor::ZdtX42s::MotorConfigReadback readback;
  last_motor_configuration_attempt_ms_ = now_ms;
  g_motor_debug_mailbox.config_valid = 0;

  const auto result = config_.motor->ReadMotorConfig(readback);
  g_motor_debug_mailbox.config_firmware_type = readback.firmware_type;
  g_motor_debug_mailbox.config_total_bytes = readback.total_bytes;
  g_motor_debug_mailbox.config_parameter_count = readback.parameter_count;
  if (result == LibXR::ErrorCode::OK)
  {
    g_motor_debug_mailbox.config_motor_type = readback.motor_type;
    g_motor_debug_mailbox.config_microstep = readback.microstep;
    g_motor_debug_mailbox.config_pulses_per_revolution =
        readback.pulses_per_revolution;
    g_motor_debug_mailbox.config_address = readback.address;
    g_motor_debug_mailbox.config_serial_baud_rate = readback.serial_baud_rate;
    g_motor_debug_mailbox.config_checksum_mode = readback.checksum_mode;
    g_motor_debug_mailbox.config_response_mode = readback.response_mode;
    g_motor_debug_mailbox.config_position_window_tenths_degree =
        readback.position_window_tenths_degree;
    g_motor_debug_mailbox.config_valid = 1;
    motor_configuration_valid_ = true;
    motor_configuration_checked_ = true;
  }
  else
  {
    motor_configuration_valid_ = false;
    // X42S X firmware returns a complete, intentionally unsupported
    // configuration payload.  The transport is proven healthy and the
    // position/enable protocol remains supported, so do not hold the whole
    // controller in its configuration gate.  Any other error stays retryable
    // and is still a hard failure when a task is started.
    motor_configuration_checked_ = result == LibXR::ErrorCode::NOT_SUPPORT;
  }
  g_motor_debug_mailbox.last_update_ms = LibXR::Thread::GetTime();

  {
    LibXR::Mutex::LockGuard lock(mutex_);
    status_.motor_error = static_cast<int32_t>(result);
  }
  return result;
}

void BalanceController::HandleMotorDebugMailbox(uint32_t now_ms)
{
  const uint32_t request_sequence = g_motor_debug_mailbox.request_sequence;
  if (request_sequence != motor_debug_seen_sequence_)
  {
    motor_debug_seen_sequence_ = request_sequence;
    const uint32_t unlock_key = g_motor_debug_mailbox.unlock_key;
    const auto operation =
        static_cast<MotorDebugOperation>(g_motor_debug_mailbox.operation);
    const float target_degrees = g_motor_debug_mailbox.target_degrees;
    const uint32_t speed_rpm = g_motor_debug_mailbox.speed_rpm;
    const uint32_t acceleration = g_motor_debug_mailbox.acceleration;
    const float control_angle_offset_degrees =
        g_motor_debug_mailbox.control_angle_offset_degrees;
    const uint32_t selected_control_profile =
        g_motor_debug_mailbox.control_profile;
    const float control_position_gain_degrees_per_cm =
        g_motor_debug_mailbox.control_position_gain_degrees_per_cm;
    const float control_integral_gain_degrees_per_cm_s =
        g_motor_debug_mailbox.control_integral_gain_degrees_per_cm_s;
    const float control_integral_limit_degrees =
        g_motor_debug_mailbox.control_integral_limit_degrees;
    const float control_velocity_gain_degrees_per_pixel_s =
        g_motor_debug_mailbox.control_velocity_gain_degrees_per_pixel_s;
    const float control_angle_limit_degrees =
        g_motor_debug_mailbox.control_angle_limit_degrees;
    const float control_minimum_command_delta_degrees =
        g_motor_debug_mailbox.control_minimum_command_delta_degrees;
    g_motor_debug_mailbox.unlock_key = 0;
    g_motor_debug_mailbox.result = kMotorDebugPendingResult;
    g_motor_debug_mailbox.last_update_ms = now_ms;

    const BalanceStatus status = GetStatus();
    if (request_sequence == 0 || unlock_key != kMotorDebugUnlockKey)
    {
      FinishMotorDebugRequest(request_sequence, LibXR::ErrorCode::ARG_ERR);
    }
    else if (status.selected_task != ContestTask::Monitor ||
             status.run_state == BalanceRunState::Running)
    {
      FinishMotorDebugRequest(request_sequence, LibXR::ErrorCode::STATE_ERR);
    }
    else if (motor_debug_motion_active_ && operation != MotorDebugOperation::Stop &&
             operation != MotorDebugOperation::Disable)
    {
      FinishMotorDebugRequest(request_sequence, LibXR::ErrorCode::STATE_ERR);
    }
    else if (operation == MotorDebugOperation::ReadPosition)
    {
      float position_degrees = 0.0F;
      const auto result = config_.motor->ReadRealtimeAngle(position_degrees);
      g_motor_debug_mailbox.position_valid = result == LibXR::ErrorCode::OK;
      if (result == LibXR::ErrorCode::OK)
      {
        g_motor_debug_mailbox.position_degrees = position_degrees;
        g_motor_debug_mailbox.position_error_degrees = 0.0F;
        LibXR::Mutex::LockGuard lock(mutex_);
        status_.motor_position_degrees = position_degrees;
        status_.motor_position_valid = true;
        status_.motor_error = static_cast<int32_t>(result);
      }
      FinishMotorDebugRequest(request_sequence, result);
    }
    else if (operation == MotorDebugOperation::ReadMotorConfig)
    {
      const auto result = RefreshMotorConfiguration(now_ms);
      FinishMotorDebugRequest(request_sequence, result);
    }
    else if (operation == MotorDebugOperation::ApplyControlTuning)
    {
      BalanceControlProfile* selected_profile = nullptr;
      switch (static_cast<ControlProfileSlot>(selected_control_profile))
      {
        case ControlProfileSlot::Task3Positive:
          selected_profile = &config_.task3_positive_pid;
          break;
        case ControlProfileSlot::Task3Negative:
          selected_profile = &config_.task3_negative_pid;
          break;
        case ControlProfileSlot::Task4:
          selected_profile = &config_.task4_pid;
          break;
        case ControlProfileSlot::Task5:
          selected_profile = &config_.task5_pid;
          break;
        case ControlProfileSlot::Task6:
          selected_profile = &config_.task6_pid;
          break;
        default:
          break;
      }

      if (selected_profile == nullptr ||
          !std::isfinite(control_angle_offset_degrees) ||
          !std::isfinite(control_position_gain_degrees_per_cm) ||
          !std::isfinite(control_integral_gain_degrees_per_cm_s) ||
          !std::isfinite(control_integral_limit_degrees) ||
          !std::isfinite(control_velocity_gain_degrees_per_pixel_s) ||
          !std::isfinite(control_angle_limit_degrees) ||
          !std::isfinite(control_minimum_command_delta_degrees) ||
          std::fabs(control_angle_offset_degrees) >
              kControlTuningMaxAbsOffsetDegrees ||
          std::fabs(control_position_gain_degrees_per_cm) >
              kControlTuningMaxAbsPositionGain ||
          std::fabs(control_integral_gain_degrees_per_cm_s) >
              kControlTuningMaxAbsIntegralGain ||
          control_integral_limit_degrees < 0.0F ||
          control_integral_limit_degrees >
              kControlTuningMaxIntegralLimitDegrees ||
          std::fabs(control_velocity_gain_degrees_per_pixel_s) >
              kControlTuningMaxAbsVelocityGain ||
          control_angle_limit_degrees <
              kControlTuningMinAngleLimitDegrees ||
          control_angle_limit_degrees >
              kControlTuningMaxAngleLimitDegrees ||
          control_minimum_command_delta_degrees < 0.0F ||
          control_minimum_command_delta_degrees >
              kControlTuningMaxCommandDeltaDegrees)
      {
        g_motor_debug_mailbox.control_tuning_valid = 0;
        FinishMotorDebugRequest(request_sequence,
                                LibXR::ErrorCode::OUT_OF_RANGE);
      }
      else
      {
        config_.angle_offset_degrees = control_angle_offset_degrees;
        selected_profile->position_gain_degrees_per_cm =
            control_position_gain_degrees_per_cm;
        selected_profile->integral_gain_degrees_per_cm_s =
            control_integral_gain_degrees_per_cm_s;
        selected_profile->integral_limit_degrees =
            control_integral_limit_degrees;
        selected_profile->velocity_gain_degrees_per_pixel_s =
            control_velocity_gain_degrees_per_pixel_s;
        selected_profile->min_angle_degrees = -control_angle_limit_degrees;
        selected_profile->max_angle_degrees = control_angle_limit_degrees;
        selected_profile->minimum_command_delta_degrees =
            control_minimum_command_delta_degrees;
        position_error_integral_cm_s_ = 0.0F;
        last_control_time_ms_ = 0;
        g_motor_debug_mailbox.control_tuning_valid = 1;
        FinishMotorDebugRequest(request_sequence, LibXR::ErrorCode::OK);
      }
    }
    else if (operation == MotorDebugOperation::SetCurrentPositionZero)
    {
      auto result = config_.motor->Enable(true);
      if (result == LibXR::ErrorCode::OK)
      {
        result = config_.motor->SetCurrentPositionAsZero();
      }
      if (result == LibXR::ErrorCode::OK)
      {
        motor_debug_zeroed_ = true;
        g_motor_debug_mailbox.zeroed = 1;
        g_motor_debug_mailbox.position_valid = 1;
        g_motor_debug_mailbox.position_degrees = 0.0F;
        g_motor_debug_mailbox.position_error_degrees = 0.0F;
        LibXR::Mutex::LockGuard lock(mutex_);
        status_.motor_enabled = true;
        status_.motor_position_valid = true;
        status_.motor_position_degrees = 0.0F;
        status_.motor_error = static_cast<int32_t>(result);
      }
      FinishMotorDebugRequest(request_sequence, result);
    }
    else if (operation == MotorDebugOperation::MoveAbsolute)
    {
      if (!motor_debug_zeroed_)
      {
        FinishMotorDebugRequest(request_sequence, LibXR::ErrorCode::STATE_ERR);
      }
      else if (!std::isfinite(target_degrees) ||
               std::fabs(target_degrees) > kMotorDebugMaxAbsTargetDegrees ||
               speed_rpm == 0 || speed_rpm > kMotorDebugMaxSpeedRpm ||
               acceleration == 0 || acceleration > kMotorDebugMaxAcceleration)
      {
        FinishMotorDebugRequest(request_sequence, LibXR::ErrorCode::OUT_OF_RANGE);
      }
      else
      {
        auto result = config_.motor->Enable(true);
        if (result == LibXR::ErrorCode::OK)
        {
          result = config_.motor->MoveToAbsoluteAngle(
              target_degrees, static_cast<uint16_t>(speed_rpm),
              static_cast<uint8_t>(acceleration));
        }
        if (result == LibXR::ErrorCode::OK)
        {
          motor_debug_active_sequence_ = request_sequence;
          motor_debug_motion_start_ms_ = now_ms;
          motor_debug_last_poll_ms_ = now_ms - kMotorDebugPollPeriodMs;
          motor_debug_active_target_degrees_ = target_degrees;
          motor_debug_settled_samples_ = 0;
          motor_debug_motion_active_ = true;
          g_motor_debug_mailbox.motion_active = 1;
          g_motor_debug_mailbox.sample_count = 0;
          g_motor_debug_mailbox.position_valid = 0;
          g_motor_debug_mailbox.position_error_degrees = 0.0F;
          LibXR::Mutex::LockGuard lock(mutex_);
          status_.motor_enabled = true;
          status_.target_angle_degrees = target_degrees;
          status_.motor_error = static_cast<int32_t>(result);
        }
        else
        {
          FinishMotorDebugRequest(request_sequence, result);
        }
      }
    }
    else if (operation == MotorDebugOperation::Stop ||
             operation == MotorDebugOperation::Disable)
    {
      auto result = config_.motor->StopImmediately();
      if (result == LibXR::ErrorCode::OK &&
          operation == MotorDebugOperation::Disable)
      {
        result = config_.motor->Enable(false);
      }
      motor_debug_motion_active_ = false;
      g_motor_debug_mailbox.motion_active = 0;
      {
        LibXR::Mutex::LockGuard lock(mutex_);
        status_.motor_enabled = operation != MotorDebugOperation::Disable &&
                                result == LibXR::ErrorCode::OK;
        status_.motor_error = static_cast<int32_t>(result);
      }
      FinishMotorDebugRequest(request_sequence, result);
    }
    else
    {
      FinishMotorDebugRequest(request_sequence, LibXR::ErrorCode::ARG_ERR);
    }
  }

  if (!motor_debug_motion_active_ ||
      static_cast<uint32_t>(now_ms - motor_debug_last_poll_ms_) <
          kMotorDebugPollPeriodMs)
  {
    return;
  }
  motor_debug_last_poll_ms_ = now_ms;

  float position_degrees = 0.0F;
  const auto read_result = config_.motor->ReadRealtimeAngle(position_degrees);
  g_motor_debug_mailbox.last_update_ms = now_ms;
  g_motor_debug_mailbox.sample_count =
      g_motor_debug_mailbox.sample_count + 1U;
  if (read_result != LibXR::ErrorCode::OK)
  {
    (void)config_.motor->StopImmediately();
    motor_debug_motion_active_ = false;
    g_motor_debug_mailbox.motion_active = 0;
    g_motor_debug_mailbox.position_valid = 0;
    FinishMotorDebugRequest(motor_debug_active_sequence_, read_result);
    return;
  }

  const float position_error_degrees =
      ShortestAngleError(position_degrees, motor_debug_active_target_degrees_);
  g_motor_debug_mailbox.position_valid = 1;
  g_motor_debug_mailbox.position_degrees = position_degrees;
  g_motor_debug_mailbox.position_error_degrees = position_error_degrees;
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    status_.motor_position_degrees = position_degrees;
    status_.motor_position_valid = true;
    status_.motor_error = static_cast<int32_t>(read_result);
  }

  if (std::fabs(position_error_degrees) <= kMotorDebugPositionToleranceDegrees)
  {
    ++motor_debug_settled_samples_;
  }
  else
  {
    motor_debug_settled_samples_ = 0;
  }

  if (motor_debug_settled_samples_ >= kMotorDebugRequiredSettledSamples)
  {
    const auto stop_result = config_.motor->StopImmediately();
    motor_debug_motion_active_ = false;
    g_motor_debug_mailbox.motion_active = 0;
    FinishMotorDebugRequest(motor_debug_active_sequence_, stop_result);
  }
  else if (static_cast<uint32_t>(now_ms - motor_debug_motion_start_ms_) >=
           kMotorDebugMotionTimeoutMs)
  {
    (void)config_.motor->StopImmediately();
    motor_debug_motion_active_ = false;
    g_motor_debug_mailbox.motion_active = 0;
    FinishMotorDebugRequest(motor_debug_active_sequence_, LibXR::ErrorCode::TIMEOUT);
  }
}

void BalanceController::UpdateIdleStatus(uint32_t now_ms)
{
  if (!motor_configuration_checked_ &&
      static_cast<uint32_t>(now_ms - last_motor_configuration_attempt_ms_) >=
          kMotorConfigurationRetryPeriodMs)
  {
    (void)RefreshMotorConfiguration(now_ms);
  }

  Maxican::BallMeasurement measurement;
  if (config_.mailbox->WaitForUpdate(measurement, config_.control_period_ms))
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    latest_measurement_ = measurement;
    has_measurement_ = true;
  }

  {
    LibXR::Mutex::LockGuard lock(mutex_);
    const Maxican::BallMeasurement latest = latest_measurement_;
    const BalanceControlProfile& profile =
        ControlProfileFor(status_.selected_task, status_.task_phase);
    if (status_.run_state == BalanceRunState::Ready)
    {
      // Show the next task's reference while the operator is checking the
      // live camera feed.  This is display-only; motion starts exclusively
      // from StartTask after RUN has been requested.
      switch (status_.selected_task)
      {
        case ContestTask::Task3:
          status_.target_position_cm = config_.task3_positive_cm;
          break;
        case ContestTask::Task6:
          status_.target_position_cm = config_.task6_target_cm;
          break;
        case ContestTask::Monitor:
        case ContestTask::Task4:
        case ContestTask::Task5:
        default:
          status_.target_position_cm = 0.0F;
          break;
      }
      status_.elapsed_ms = 0;
      status_.target_angle_degrees = 0.0F;
    }
    status_.vision_age_ms = has_measurement_
                                ? static_cast<uint32_t>(now_ms - latest.received_time_ms)
                                : UINT32_MAX;
    status_.position_cm = latest.position_cm;
    status_.velocity_pixel_s = latest.velocity_pixel_s;
    status_.confidence = latest.confidence;
    status_.source_frame_time_ms = latest.frame_time_ms;
    status_.vision_valid = has_measurement_ &&
                           IsFreshMeasurement(latest, now_ms,
                                              config_.vision_timeout_ms) &&
                           latest.confidence >= profile.min_confidence &&
                           std::fabs(latest.position_cm) <=
                               config_.max_abs_position_cm;
  }

  UpdateMotorPosition(now_ms);
  PublishStatus();
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
    g_motor_debug_mailbox.task_target_settled = 1;
    g_motor_debug_mailbox.task_settled_position_cm = snapshot.position_cm;
    {
      LibXR::Mutex::LockGuard lock(mutex_);
      status_.task_phase = kTask3MoveNegative;
      status_.target_position_cm = config_.task3_negative_cm;
      target_settle_start_ms_ = 0;
      motion_command_active_ = false;
      position_error_integral_cm_s_ = 0.0F;
      last_control_time_ms_ = now_ms;
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
    position_error_integral_cm_s_ = 0.0F;
    last_control_time_ms_ = 0;
  }
  PublishStatus();
}

void BalanceController::PublishStatus()
{
  BalanceStatus snapshot = GetStatus();
  g_motor_debug_mailbox.task_selected = static_cast<uint32_t>(snapshot.selected_task);
  g_motor_debug_mailbox.task_run_state = static_cast<uint32_t>(snapshot.run_state);
  g_motor_debug_mailbox.task_fault = static_cast<uint32_t>(snapshot.fault);
  g_motor_debug_mailbox.task_phase = snapshot.task_phase;
  g_motor_debug_mailbox.task_motor_enabled = snapshot.motor_enabled ? 1U : 0U;
  g_motor_debug_mailbox.task_vision_valid = snapshot.vision_valid ? 1U : 0U;
  g_motor_debug_mailbox.task_elapsed_ms = snapshot.elapsed_ms;
  g_motor_debug_mailbox.task_vision_age_ms = snapshot.vision_age_ms;
  g_motor_debug_mailbox.task_position_cm = snapshot.position_cm;
  g_motor_debug_mailbox.task_confidence = snapshot.confidence;
  g_motor_debug_mailbox.task_reference_cm = snapshot.target_position_cm;
  g_motor_debug_mailbox.task_target_angle_degrees = snapshot.target_angle_degrees;
  g_motor_debug_mailbox.task_motor_position_valid =
      snapshot.motor_position_valid ? 1U : 0U;
  g_motor_debug_mailbox.task_motor_position_degrees =
      snapshot.motor_position_degrees;
  g_motor_debug_mailbox.task_motor_error = snapshot.motor_error;
  {
    LibXR::Mutex::LockGuard lock(mutex_);
    g_motor_debug_mailbox.control_command_active = motion_command_active_ ? 1U : 0U;
    g_motor_debug_mailbox.control_command_count = control_command_count_;
    g_motor_debug_mailbox.control_last_command_angle_degrees =
        last_command_angle_degrees_;
  }
  state_topic_.Publish(snapshot, LibXR::Timebase::GetMicroseconds());
}

const BalanceControlProfile& BalanceController::ControlProfileFor(
    ContestTask task, uint8_t task_phase) const
{
  switch (task)
  {
    case ContestTask::Task3:
      return task_phase == kTask3MoveNegative ? config_.task3_negative_pid
                                               : config_.task3_positive_pid;
    case ContestTask::Task4:
      return config_.task4_pid;
    case ContestTask::Task5:
      return config_.task5_pid;
    case ContestTask::Task6:
      return config_.task6_pid;
    case ContestTask::Monitor:
    default:
      return config_.task3_positive_pid;
  }
}

bool BalanceController::IsControlProfileValid(
    const BalanceControlProfile& profile)
{
  return std::isfinite(profile.position_gain_degrees_per_cm) &&
         std::isfinite(profile.integral_gain_degrees_per_cm_s) &&
         std::isfinite(profile.integral_limit_degrees) &&
         profile.integral_limit_degrees >= 0.0F &&
         std::isfinite(profile.velocity_gain_degrees_per_pixel_s) &&
         std::isfinite(profile.min_angle_degrees) &&
         std::isfinite(profile.max_angle_degrees) &&
         profile.min_angle_degrees <= profile.max_angle_degrees &&
         std::isfinite(profile.minimum_command_delta_degrees) &&
         profile.minimum_command_delta_degrees >= 0.0F &&
         std::isfinite(profile.min_confidence) &&
         profile.min_confidence >= 0.0F && profile.min_confidence <= 1.0F;
}

bool BalanceController::IsConfigurationValid() const
{
  return config_.motor != nullptr && config_.mailbox != nullptr &&
         std::isfinite(config_.angle_offset_degrees) &&
         IsControlProfileValid(config_.task3_positive_pid) &&
         IsControlProfileValid(config_.task3_negative_pid) &&
         IsControlProfileValid(config_.task4_pid) &&
         IsControlProfileValid(config_.task5_pid) &&
         IsControlProfileValid(config_.task6_pid) &&
         std::isfinite(config_.task3_positive_cm) &&
         std::isfinite(config_.task3_negative_cm) &&
         std::isfinite(config_.task6_target_cm) &&
         std::isfinite(config_.position_tolerance_cm) &&
         config_.position_tolerance_cm > 0.0F &&
         std::isfinite(config_.max_abs_position_cm) &&
         config_.max_abs_position_cm > config_.position_tolerance_cm &&
         std::fabs(config_.task6_target_cm) <=
             config_.max_abs_position_cm - config_.position_tolerance_cm &&
         config_.motor_speed_rpm > 0 &&
         config_.control_period_ms > 0 && config_.vision_timeout_ms > 0 &&
         config_.invalid_measurement_grace_ms > 0 &&
         config_.invalid_measurement_neutralize_ms > 0 &&
         config_.invalid_measurement_neutralize_ms <=
             config_.invalid_measurement_grace_ms &&
         config_.settle_time_ms > 0 && config_.motor_position_poll_period_ms > 0;
}

ContestTask BalanceController::NextTask(ContestTask task)
{
  switch (task)
  {
    case ContestTask::Monitor:
      return ContestTask::Task3;
    case ContestTask::Task3:
      return ContestTask::Task4;
    case ContestTask::Task4:
      return ContestTask::Task5;
    case ContestTask::Task5:
      return ContestTask::Task6;
    case ContestTask::Task6:
    default:
      return ContestTask::Monitor;
  }
}

uint32_t BalanceController::TimeLimitMs(ContestTask task)
{
  switch (task)
  {
    case ContestTask::Monitor:
      return 0;
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
