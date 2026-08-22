#pragma once

#include "balance_controller.hpp"
#include "stm32_gpio.hpp"

extern "C"
{
#include "lcd.h"
}

namespace BuTask
{

struct OperatorPanelConfig
{
  BalanceController* controller = nullptr;
  LibXR::STM32GPIO* select_button = nullptr;
  LibXR::STM32GPIO* run_button = nullptr;
  LibXR::STM32GPIO* homing_button = nullptr;
  lcd* display = nullptr;
  uint32_t poll_period_ms = 20;
  uint32_t display_period_ms = 500;
};

class OperatorPanel
{
 public:
  explicit OperatorPanel(OperatorPanelConfig config);

  void Run();

 private:
  struct DebouncedButton
  {
    struct Events
    {
      bool pressed = false;
      bool released = false;
      bool long_pressed = false;
      bool short_click = false;
    };

    [[nodiscard]] Events Update(bool raw_state, uint32_t now_ms);
    void Reset();

    bool candidate = false;
    bool stable = false;
    uint8_t samples = 0;
    uint32_t press_start_ms = 0;
    bool long_press_reported = false;
  };

  void Render(const BalanceStatus& status);
  [[nodiscard]] static const char* TaskName(ContestTask task);
  [[nodiscard]] static const char* StateName(BalanceRunState state);
  [[nodiscard]] static const char* FaultName(BalanceFault fault);

  OperatorPanelConfig config_;
  DebouncedButton select_button_;
  DebouncedButton run_button_;
  DebouncedButton homing_button_;
};

void OperatorPanelThread(OperatorPanel* panel);

}  // namespace BuTask
