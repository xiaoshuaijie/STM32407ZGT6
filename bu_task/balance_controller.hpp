#pragma once

#include <cstdint>

#include "libxr.hpp"
#include "maxican.hpp"
#include "zdt_x42s.hpp"

namespace BuTask
{

enum class ContestTask : uint8_t
{
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

struct BalanceStatus
{
  ContestTask selected_task = ContestTask::Task3;
  BalanceRunState run_state = BalanceRunState::Ready;
  BalanceFault fault = BalanceFault::None;
  bool motor_enabled = false;
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
  void AbortTask(BalanceFault fault);
  void CompleteTask();
  void UpdateControl(uint32_t now_ms);
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
  float last_command_angle_degrees_ = 0.0F;
};

void BalanceControlThread(BalanceController* controller);

}  // namespace BuTask
