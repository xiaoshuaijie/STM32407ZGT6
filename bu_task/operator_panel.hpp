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
    [[nodiscard]] bool Update(bool raw_state);

    bool candidate = false;
    bool stable = false;
    uint8_t samples = 0;
  };

  void Render(const BalanceStatus& status);
  [[nodiscard]] static const char* TaskName(ContestTask task);
  [[nodiscard]] static const char* StateName(BalanceRunState state);
  [[nodiscard]] static const char* FaultName(BalanceFault fault);

  OperatorPanelConfig config_;
  DebouncedButton select_button_;
  DebouncedButton run_button_;
};

void OperatorPanelThread(OperatorPanel* panel);

}  // namespace BuTask
