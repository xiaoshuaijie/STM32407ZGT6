#pragma once

#include <cstdint>

#include "libxr.hpp"
#include "maxican.hpp"
#include "zdt_x42s.hpp"

namespace BuTask
{

enum class ContestTask : uint8_t
{
  Monitor = 0,
  Task3 = 3,
  Task4 = 4,
  Task5 = 5,
  Task6 = 6,
};

enum class BalanceRunState : uint8_t
{
  Ready,
  Running,
  Completed,
  Fault,
};

enum class BalanceFault : uint8_t
{
  None,
  VisionTimeout,
  VisionInvalid,
  MotorCommand,
  TaskTimeout,
  Configuration,
};

enum class MotorDebugOperation : uint32_t
{
  Idle = 0,
  ReadPosition = 1,
  SetCurrentPositionZero = 2,
  MoveAbsolute = 3,
  Stop = 4,
  Disable = 5,
  ReadMotorConfig = 6,
  ApplyControlTuning = 7,
};

enum class ControlProfileSlot : uint32_t
{
  Task3Positive = 0,
  Task3Negative = 1,
  Task4 = 2,
  Task5 = 3,
  Task6 = 4,
};

inline constexpr uint32_t kMotorDebugUnlockKey = 0x58423432U;
inline constexpr int32_t kMotorDebugPendingResult = (-2147483647 - 1);

struct MotorDebugMailbox
{
  volatile uint32_t unlock_key = 0;
  volatile uint32_t request_sequence = 0;
  volatile uint32_t completed_sequence = 0;
  volatile uint32_t operation = static_cast<uint32_t>(MotorDebugOperation::Idle);
  volatile float target_degrees = 0.0F;
  volatile uint32_t speed_rpm = 30;
  volatile uint32_t acceleration = 20;
  volatile int32_t result = kMotorDebugPendingResult;
  volatile uint32_t position_valid = 0;
  volatile float position_degrees = 0.0F;
  volatile float position_error_degrees = 0.0F;
  volatile uint32_t sample_count = 0;
  volatile uint32_t zeroed = 0;
  volatile uint32_t motion_active = 0;
  volatile uint32_t config_valid = 0;
  volatile uint32_t config_firmware_type = 0xFF;
  volatile uint32_t config_total_bytes = 0;
  volatile uint32_t config_parameter_count = 0;
  volatile uint32_t config_motor_type = 0;
  volatile uint32_t config_microstep = 0;
  volatile uint32_t config_pulses_per_revolution = 0;
  volatile uint32_t config_address = 0;
  volatile uint32_t config_serial_baud_rate = 0;
  volatile uint32_t config_checksum_mode = 0;
  volatile uint32_t config_response_mode = 0;
  volatile uint32_t config_position_window_tenths_degree = 0;
  volatile uint32_t control_tuning_valid = 0;
  volatile uint32_t control_profile =
      static_cast<uint32_t>(ControlProfileSlot::Task3Positive);
  volatile float control_angle_offset_degrees = 0.0F;
  volatile float control_position_gain_degrees_per_cm = 1.0F;
  volatile float control_integral_gain_degrees_per_cm_s = 0.0F;
  volatile float control_integral_limit_degrees = 3.0F;
  volatile float control_velocity_gain_degrees_per_pixel_s = 0.0F;
  volatile float control_angle_limit_degrees = 20.0F;
  volatile float control_minimum_command_delta_degrees = 0.2F;
  // Live task-path snapshot for Ozone. These fields are telemetry only and
  // are never read by the controller, so they cannot alter motor behavior.
  volatile uint32_t task_selected = 0;
  volatile uint32_t task_run_state = 0;
  volatile uint32_t task_fault = 0;
  volatile uint32_t task_phase = 0;
  volatile uint32_t task_motor_enabled = 0;
  volatile uint32_t task_vision_valid = 0;
  volatile uint32_t task_elapsed_ms = 0;
  volatile uint32_t task_vision_age_ms = UINT32_MAX;
  volatile float task_position_cm = 0.0F;
  volatile float task_confidence = 0.0F;
  volatile float task_reference_cm = 0.0F;
  volatile uint32_t task_position_sample_count = 0;
  volatile float task_min_position_cm = 0.0F;
  volatile float task_max_position_cm = 0.0F;
  volatile float task_best_abs_error_cm = 0.0F;
  volatile float task_best_position_cm = 0.0F;
  volatile uint32_t task_in_tolerance_sample_count = 0;
  volatile uint32_t task_target_settled = 0;
  volatile float task_settled_position_cm = 0.0F;
  // Latched details for transient/repeated vision rejection while RUNNING.
  // Unlike the live task_* fields, these are not overwritten by idle
  // telemetry after a task aborts, so Ozone can identify the rejected frame.
  volatile uint32_t task_vision_reject_count = 0;
  volatile uint32_t task_vision_reject_reason = 0;
  volatile uint32_t task_rejected_measurement_valid = 0;
  volatile float task_rejected_position_cm = 0.0F;
  volatile float task_rejected_confidence = 0.0F;
  volatile uint32_t task_rejected_duration_ms = 0;
  volatile float task_target_angle_degrees = 0.0F;
  volatile uint32_t task_motor_position_valid = 0;
  volatile float task_motor_position_degrees = 0.0F;
  volatile int32_t motor_position_read_result = 0;
  volatile int32_t task_motor_error = 0;
  volatile uint32_t task_start_stage = 0;
  volatile int32_t task_start_result = 0;
  volatile uint32_t control_command_active = 0;
  volatile uint32_t control_command_count = 0;
  volatile float control_last_command_angle_degrees = 0.0F;
  volatile int32_t control_last_command_result = 0;
  volatile uint32_t last_update_ms = 0;
};

extern "C"
{
extern volatile MotorDebugMailbox g_motor_debug_mailbox;
}

struct BalanceStatus
{
  ContestTask selected_task = ContestTask::Monitor;
  BalanceRunState run_state = BalanceRunState::Ready;
  BalanceFault fault = BalanceFault::None;
  bool motor_enabled = false;
  bool motor_position_valid = false;
  bool vision_valid = false;
  uint8_t task_phase = 0;
  uint32_t elapsed_ms = 0;
  uint32_t vision_age_ms = UINT32_MAX;
  uint32_t source_frame_time_ms = 0;
  float position_cm = 0.0F;
  float velocity_pixel_s = 0.0F;
  float confidence = 0.0F;
  float target_position_cm = 0.0F;
  float target_angle_degrees = 0.0F;
  float motor_position_degrees = 0.0F;
  int32_t motor_error = 0;
};

struct BalanceControlProfile
{
  float position_gain_degrees_per_cm = 1.0F;
  float integral_gain_degrees_per_cm_s = 0.0F;
  float integral_limit_degrees = 3.0F;
  float velocity_gain_degrees_per_pixel_s = 0.0F;
  float min_angle_degrees = -20.0F;
  float max_angle_degrees = 20.0F;
  float minimum_command_delta_degrees = 0.2F;
  float min_confidence = 0.50F;
};

struct BalanceControllerConfig
{
  BujinMotor::ZdtX42s* motor = nullptr;
  Maxican::BallMailbox* mailbox = nullptr;
  float angle_offset_degrees = 0.0F;
  BalanceControlProfile task3_positive_pid;
  BalanceControlProfile task3_negative_pid;
  BalanceControlProfile task4_pid;
  BalanceControlProfile task5_pid;
  BalanceControlProfile task6_pid;
  float task3_positive_cm = 5.0F;
  float task3_negative_cm = -5.0F;
  float task6_target_cm = 3.0F;
  float position_tolerance_cm = 1.0F;
  float max_abs_position_cm = 12.5F;
  uint16_t motor_speed_rpm = 300;
  uint8_t motor_acceleration = 100;
  uint32_t control_period_ms = 20;
  uint32_t vision_timeout_ms = 200;
  // A decoded frame may briefly carry valid=false while the camera reacquires
  // the ball. Keep that grace separate from the stale-input timeout so one
  // short invalid burst does not abort an otherwise active task.
  uint32_t invalid_measurement_grace_ms = 350;
  // Return to the task zero after a sustained invalid burst instead of
  // holding a saturated tilt without visual feedback.
  uint32_t invalid_measurement_neutralize_ms = 100;
  uint32_t settle_time_ms = 300;
  uint32_t motor_position_poll_period_ms = 500;
  bool zero_motor_on_task_start = true;
};

class BalanceController
{
 public:
  static constexpr const char* kStateTopicName = "balance_state";

  explicit BalanceController(BalanceControllerConfig config);

  void SelectNextTask();
  void ToggleRun();
  void StepTask6Target(int8_t direction);
  void RequestSetSingleTurnHomingZero();
  void RequestHoming();
  [[nodiscard]] BalanceStatus GetStatus() const;
  void Run();

 private:
  void HandleRequests();
  void HandleViewKeyRequests();
  void StartTask();
  void StartViewMotorTest();
  void AbortTask(BalanceFault fault);
  void CompleteTask();
  void UpdateControl(uint32_t now_ms);
  void UpdateViewMotorTest(uint32_t now_ms);
  // Refreshes the LCD/telemetry view whenever no task is driving the motor.
  // This deliberately applies to VIEW and every selected task in READY,
  // COMPLETED or FAULT state, so changing modes never freezes vision data.
  void UpdateIdleStatus(uint32_t now_ms);
  void UpdateMotorPosition(uint32_t now_ms);
  [[nodiscard]] LibXR::ErrorCode RefreshMotorConfiguration(uint32_t now_ms);
  void HandleMotorDebugMailbox(uint32_t now_ms);
  void FinishMotorDebugRequest(uint32_t sequence, LibXR::ErrorCode result);
  void UpdateTaskTarget(uint32_t now_ms);
  void SetFault(BalanceFault fault, LibXR::ErrorCode motor_error);
  void PublishStatus();
  [[nodiscard]] bool IsConfigurationValid() const;
  [[nodiscard]] const BalanceControlProfile& ControlProfileFor(
      ContestTask task, uint8_t task_phase) const;
  [[nodiscard]] static bool IsControlProfileValid(
      const BalanceControlProfile& profile);
  [[nodiscard]] static ContestTask NextTask(ContestTask task);
  [[nodiscard]] static uint32_t TimeLimitMs(ContestTask task);

  BalanceControllerConfig config_;
  LibXR::Topic state_topic_;
  mutable LibXR::Mutex mutex_;
  BalanceStatus status_;
  Maxican::BallMeasurement latest_measurement_;
  bool has_measurement_ = false;
  bool start_requested_ = false;
  bool abort_requested_ = false;
  bool set_homing_zero_requested_ = false;
  bool homing_requested_ = false;
  bool motion_command_active_ = false;
  uint32_t task_start_time_ms_ = 0;
  uint32_t last_accepted_measurement_time_ms_ = 0;
  uint32_t target_settle_start_ms_ = 0;
  uint32_t view_test_phase_start_ms_ = 0;
  uint32_t view_test_last_position_poll_ms_ = 0;
  bool view_test_reverse_requested_ = false;
  uint32_t last_motor_position_poll_ms_ = 0;
  uint32_t last_motor_configuration_attempt_ms_ = 0;
  // An X-firmware motor replies to the configuration query with a valid
  // frame, but does not expose the EMM configuration payload.  Keep the
  // distinction between "checked" and "readback available" so that this
  // compatible response does not prevent all subsequent motion commands.
  bool motor_configuration_checked_ = false;
  bool motor_configuration_valid_ = false;
  float last_command_angle_degrees_ = 0.0F;
  float position_error_integral_cm_s_ = 0.0F;
  uint32_t last_control_time_ms_ = 0;
  uint32_t control_command_count_ = 0;
  uint32_t motor_debug_seen_sequence_ = 0;
  uint32_t motor_debug_active_sequence_ = 0;
  uint32_t motor_debug_motion_start_ms_ = 0;
  uint32_t motor_debug_last_poll_ms_ = 0;
  float motor_debug_active_target_degrees_ = 0.0F;
  uint8_t motor_debug_settled_samples_ = 0;
  bool motor_debug_zeroed_ = false;
  bool motor_debug_motion_active_ = false;
};

void BalanceControlThread(BalanceController* controller);

}  // namespace BuTask
