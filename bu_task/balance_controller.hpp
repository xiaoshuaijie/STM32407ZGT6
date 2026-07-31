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

struct BalanceControllerConfig
{
  BujinMotor::ZdtX42s* motor = nullptr;
  Maxican::BallMailbox* mailbox = nullptr;
  float angle_offset_degrees = 0.0F;
  float position_gain_degrees_per_cm = 1.0F;
  float velocity_gain_degrees_per_pixel_s = 0.0F;
  float min_angle_degrees = -20.0F;
  float max_angle_degrees = 20.0F;
  float minimum_command_delta_degrees = 0.2F;
  float min_confidence = 0.50F;
  float task3_positive_cm = 5.0F;
  float task3_negative_cm = -5.0F;
  float task6_target_cm = 3.0F;
  float position_tolerance_cm = 1.0F;
  float max_abs_position_cm = 12.5F;
  uint16_t motor_speed_rpm = 300;
  uint8_t motor_acceleration = 100;
  uint32_t control_period_ms = 20;
  uint32_t vision_timeout_ms = 200;
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
  [[nodiscard]] BalanceStatus GetStatus() const;
  void Run();

 private:
  void HandleRequests();
  void StartTask();
  void StartViewMotorTest();
  void AbortTask(BalanceFault fault);
  void CompleteTask();
  void UpdateControl(uint32_t now_ms);
  void UpdateViewMotorTest(uint32_t now_ms);
  void UpdateMonitorStatus(uint32_t now_ms);
  void UpdateMotorPosition(uint32_t now_ms);
  void HandleMotorDebugMailbox(uint32_t now_ms);
  void FinishMotorDebugRequest(uint32_t sequence, LibXR::ErrorCode result);
  void UpdateTaskTarget(uint32_t now_ms);
  void SetFault(BalanceFault fault, LibXR::ErrorCode motor_error);
  void PublishStatus();
  [[nodiscard]] bool IsConfigurationValid() const;
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
  bool motion_command_active_ = false;
  uint32_t task_start_time_ms_ = 0;
  uint32_t target_settle_start_ms_ = 0;
  uint32_t view_test_phase_start_ms_ = 0;
  uint32_t view_test_last_position_poll_ms_ = 0;
  bool view_test_reverse_requested_ = false;
  uint32_t last_motor_position_poll_ms_ = 0;
  float last_command_angle_degrees_ = 0.0F;
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
