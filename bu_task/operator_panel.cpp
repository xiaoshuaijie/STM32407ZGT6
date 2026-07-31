#include "operator_panel.hpp"

#include <cmath>

namespace BuTask
{
namespace
{

constexpr uint8_t kDebounceSamples = 3;
constexpr uint16_t kContentX = 80;
constexpr uint16_t kLineY[] = {0, 29, 58, 87, 116, 145};

}  // namespace

OperatorPanel::OperatorPanel(OperatorPanelConfig config) : config_(config) {}

bool OperatorPanel::DebouncedButton::Update(bool raw_state)
{
  if (raw_state != candidate)
  {
    candidate = raw_state;
    samples = 1;
    return false;
  }
  if (samples < kDebounceSamples)
  {
    ++samples;
  }
  if (samples != kDebounceSamples || stable == candidate)
  {
    return false;
  }
  stable = candidate;
  return stable;
}

void OperatorPanel::Run()
{
  if (config_.controller == nullptr || config_.select_button == nullptr ||
      config_.run_button == nullptr || config_.display == nullptr ||
      config_.poll_period_ms == 0 || config_.display_period_ms == 0)
  {
    return;
  }

  lcd_set_font(config_.display, FONT_2412, WHITE, BLACK);
  lcd_clear(config_.display, BLACK);
  uint32_t last_display_time_ms =
      LibXR::Thread::GetTime() - config_.display_period_ms;
  while (true)
  {
    // PA4/PA5 use internal pull-ups, so a pressed button reads low.
    if (select_button_.Update(!config_.select_button->Read()))
    {
      config_.controller->SelectNextTask();
    }
    if (run_button_.Update(!config_.run_button->Read()))
    {
      config_.controller->ToggleRun();
    }

    const uint32_t now_ms = LibXR::Thread::GetTime();
    if (static_cast<uint32_t>(now_ms - last_display_time_ms) >=
        config_.display_period_ms)
    {
      Render(config_.controller->GetStatus());
      last_display_time_ms = now_ms;
    }
    LibXR::Thread::Sleep(config_.poll_period_ms);
  }
}

void OperatorPanel::Render(const BalanceStatus& status)
{
  const unsigned seconds = static_cast<unsigned>(status.elapsed_ms / 1000U);
  const unsigned tenth_seconds = static_cast<unsigned>((status.elapsed_ms % 1000U) / 100U);
  const unsigned age_ms = status.vision_age_ms == UINT32_MAX
                              ? 9999U
                              : static_cast<unsigned>(status.vision_age_ms);
  const char vision = status.vision_valid ? 'Y' : 'N';
  const char motor_position = status.motor_position_valid ? 'Y' : 'N';

  lcd_print(config_.display, kContentX, kLineY[0], "%-7s %-9s",
            TaskName(status.selected_task), StateName(status.run_state));
  lcd_print(config_.display, kContentX, kLineY[1], "TIME %2u.%us", seconds,
            tenth_seconds);
  lcd_print(config_.display, kContentX, kLineY[2], "X%+5.2f R%+5.2f", status.position_cm,
            status.target_position_cm);
  lcd_print(config_.display, kContentX, kLineY[3], "V%c C%.2f A%04u", vision, status.confidence,
            age_ms);
  lcd_print(config_.display, kContentX, kLineY[4], "F:%-10s", FaultName(status.fault));
  lcd_print(config_.display, kContentX, kLineY[5], "M%c%+6.1f G%+6.1f",
            motor_position, status.motor_position_degrees,
            status.target_angle_degrees);
}

const char* OperatorPanel::TaskName(ContestTask task)
{
  switch (task)
  {
    case ContestTask::Monitor:
      return "VIEW";
    case ContestTask::Task3:
      return "T3";
    case ContestTask::Task4:
      return "T4";
    case ContestTask::Task5:
      return "T5";
    case ContestTask::Task6:
    default:
      return "T6";
  }
}

const char* OperatorPanel::StateName(BalanceRunState state)
{
  switch (state)
  {
    case BalanceRunState::Ready:
      return "READY";
    case BalanceRunState::Running:
      return "RUN";
    case BalanceRunState::Completed:
      return "DONE";
    case BalanceRunState::Fault:
    default:
      return "FAULT";
  }
}

const char* OperatorPanel::FaultName(BalanceFault fault)
{
  switch (fault)
  {
    case BalanceFault::None:
      return "NONE";
    case BalanceFault::VisionTimeout:
      return "NO FRAME";
    case BalanceFault::VisionInvalid:
      return "BAD FRAME";
    case BalanceFault::MotorCommand:
      return "MOTOR";
    case BalanceFault::TaskTimeout:
      return "TIMEOUT";
    case BalanceFault::Configuration:
    default:
      return "CONFIG";
  }
}

void OperatorPanelThread(OperatorPanel* panel)
{
  if (panel != nullptr)
  {
    panel->Run();
  }
}

}  // namespace BuTask
