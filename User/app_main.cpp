#include "app_main.h"

#include "balance_controller.hpp"
#include "cdc_uart.hpp"
#include "flash_map.hpp"
#include "libxr.hpp"
#include "main.h"
#include "maxican.hpp"
#include "operator_panel.hpp"
#include "stm32_adc.hpp"
#include "stm32_can.hpp"
#include "stm32_canfd.hpp"
#include "stm32_dac.hpp"
#include "stm32_flash.hpp"
#include "stm32_gpio.hpp"
#include "stm32_i2c.hpp"
#include "stm32_power.hpp"
#include "stm32_pwm.hpp"
#include "stm32_spi.hpp"
#include "stm32_timebase.hpp"
#include "stm32_uart.hpp"
#include "stm32_usb_dev.hpp"
#include "stm32_watchdog.hpp"

extern "C" {
#include "lcd.h"
}

using namespace LibXR;

/* User Code Begin 1 */
/* User Code End 1 */
// NOLINTBEGIN
// clang-format off
/* External HAL Declarations */
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
extern SPI_HandleTypeDef hspi1;
extern TIM_HandleTypeDef htim6;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

/* DMA Resources */
static uint8_t spi1_tx_buf[32];
static uint8_t spi1_rx_buf[32];
static uint8_t usart1_tx_buf[128];
static uint8_t usart1_rx_buf[128];
static uint8_t usart2_tx_buf[128];
static uint8_t usart2_rx_buf[128];
static uint8_t usart3_tx_buf[128];
static uint8_t usart3_rx_buf[128];
static uint8_t usb_otg_fs_ep0_in_buf[8];
static uint8_t usb_otg_fs_ep0_out_buf[8];
static uint8_t usb_otg_fs_ep1_in_buf[128];
static uint8_t usb_otg_fs_ep1_out_buf[128];
static uint8_t usb_otg_fs_ep2_in_buf[16];
static uint16_t lcd1_line_buffer[320];

extern "C" void app_main(void) {
  // clang-format on
  // NOLINTEND
  /* User Code Begin 2 */

  /* User Code End 2 */
  // clang-format off
  // NOLINTBEGIN
  STM32TimerTimebase timebase(&htim6);
  PlatformInit(2, 1024);
  STM32PowerManager power_manager;

  /* GPIO Configuration */
  STM32GPIO PA4(GPIOA, GPIO_PIN_4);
  STM32GPIO PA5(GPIOA, GPIO_PIN_5);
  STM32GPIO PC13(GPIOC, GPIO_PIN_13);
  STM32GPIO LCD_DC(LCD_DC_GPIO_Port, LCD_DC_Pin);
  STM32GPIO PC0(GPIOC, GPIO_PIN_0);
  STM32GPIO PC1(GPIOC, GPIO_PIN_1);
  STM32GPIO PC2(GPIOC, GPIO_PIN_2);
  STM32GPIO LCD_PWR(LCD_PWR_GPIO_Port, LCD_PWR_Pin);
  STM32GPIO LCD_RST(LCD_RST_GPIO_Port, LCD_RST_Pin);
  STM32GPIO LCD_CS(LCD_CS_GPIO_Port, LCD_CS_Pin);
  PA4.SetConfig({GPIO::Direction::INPUT, GPIO::Pull::UP});
  PA5.SetConfig({GPIO::Direction::INPUT, GPIO::Pull::UP});
  PC13.SetConfig({GPIO::Direction::INPUT, GPIO::Pull::UP});


  STM32SPI spi1(&hspi1, spi1_rx_buf, spi1_tx_buf, 3);

  STM32UART usart1(&huart1, usart1_rx_buf, usart1_tx_buf, 5);

  STM32UART usart2(&huart2, usart2_rx_buf, usart2_tx_buf, 5);

  STM32UART usart3(&huart3, usart3_rx_buf, usart3_tx_buf, 5);

  static constexpr auto USB_OTG_FS_LANG_PACK = LibXR::USB::DescriptorStrings::MakeLanguagePack(LibXR::USB::DescriptorStrings::Language::EN_US, "XRobot", "STM32 XRUSB USB_OTG_FS CDC Demo", "XRUSB-DEMO-");
  LibXR::USB::CDCUart usb_otg_fs_cdc(LibXR::USB::Endpoint::EPNumber::EP1, LibXR::USB::Endpoint::EPNumber::EP1, LibXR::USB::Endpoint::EPNumber::EP2, 128, 128, 3);

  STM32USBDeviceOtgFS usb_fs(
      &hpcd_USB_OTG_FS,
      256,
      {usb_otg_fs_ep0_out_buf, usb_otg_fs_ep1_out_buf},
      {{usb_otg_fs_ep0_in_buf, 8}, {usb_otg_fs_ep1_in_buf, 128}, {usb_otg_fs_ep2_in_buf, 16}},
      USB::DeviceDescriptor::PacketSize0::SIZE_8,
      0x1D50, 0x6199, 0x100,
      {&USB_OTG_FS_LANG_PACK},
      {{&usb_otg_fs_cdc}},
      {reinterpret_cast<void *>(UID_BASE), 12}
  );
  usb_fs.Init(false);
  usb_fs.Start(false);

  /* Terminal Configuration */

  // clang-format on
  // NOLINTEND
  /* User Code Begin 3 */

  lcd_io lcd1_io = {
      &spi1,
      {&LCD_RST, false},
      {&LCD_PWR, true},
      {&LCD_CS, false},
      {&LCD_DC, false},
      {nullptr, false},
  };
  lcd lcd1{};
  lcd1.io = &lcd1_io;
  lcd1.line_buffer = lcd1_line_buffer;
  lcd_init_dev(&lcd1, LCD_1_47_INCH, LCD_ROTATE_90);

  STDIO::read_ = usart2.read_port_;
  STDIO::write_ = usart2.write_port_;
  LibXR::RamFS ramfs;
  LibXR::Terminal<> terminal(ramfs);
  Maxican::BallMailbox ball_mailbox;
  Maxican::BallReceiver maxican_receiver(usart1, ball_mailbox, ramfs);
  LibXR::Thread term_thread;
  term_thread.Create(&terminal, terminal.ThreadFun, "terminal", 1024,
                     LibXR::Thread::Priority::MEDIUM);
  LibXR::Thread maxican_receive_thread;
  maxican_receive_thread.Create(&maxican_receiver, Maxican::ReceiveThread,
                                "maxican_rx", 1024,
                                LibXR::Thread::Priority::HIGH);

  BujinMotor::ZdtX42s motor(usart3);

  // 平衡控制器的机械参数与实物装配方向、连杆尺寸和相机标定相关，
  // 正式运行前应在实机上确认增益符号、角度限幅及目标位置是否正确。
  BuTask::BalanceControllerConfig balance_config;

  // 控制器通过该电机对象发送绝对角度、速度和加速度指令。
  balance_config.motor = &motor;
  // 视觉接收线程把球的位置、像素速度和置信度写入此邮箱，控制器从中取最新测量值。
  balance_config.mailbox = &ball_mailbox;
  // 杆处于机械水平位置时对应的电机绝对角度，单位为度；0 表示当前零位即水平位。
  balance_config.angle_offset_degrees = 0.0F;

  // T3 PID：第一阶段，目标位置为 +5 cm。
  // 位置比例增益，单位为“度/厘米”。
  balance_config.task3_positive_pid.position_gain_degrees_per_cm = -2.3F;
  // 位置积分增益，单位为“度/(厘米*秒)”。
  balance_config.task3_positive_pid.integral_gain_degrees_per_cm_s = -0.0F;
  // 积分项最多贡献的角度，单位为度。
  balance_config.task3_positive_pid.integral_limit_degrees = 2.0F;
  // 速度反馈增益，乘数输入为 pixel/s。
  balance_config.task3_positive_pid.velocity_gain_degrees_per_pixel_s = -0.020F;
  // 微分增益，对位置误差求导，单位为“度/(厘米/秒)”。
  balance_config.task3_positive_pid.derivative_gain_degrees_per_cm_s =
      0.0F; // d
  // 最小允许目标角度，单位为度。
  balance_config.task3_positive_pid.min_angle_degrees = -5.0F;
  // 最大允许目标角度，单位为度。
  balance_config.task3_positive_pid.max_angle_degrees = 7.0F;
  // 相邻两次目标角度变化小于该值时不重复发送电机指令。
  balance_config.task3_positive_pid.minimum_command_delta_degrees = 0.15F;
  // 接受视觉测量的最低置信度，取值范围为 [0, 1]。
  balance_config.task3_positive_pid.min_confidence = 0.50F;

  // T3 PID：第二阶段，目标位置为 -5 cm。
  // 位置比例增益，单位为“度/厘米”。
  balance_config.task3_negative_pid.position_gain_degrees_per_cm = -0.5F;
  // 位置积分增益，单位为“度/(厘米*秒)”。
  balance_config.task3_negative_pid.integral_gain_degrees_per_cm_s = -0.0F;
  // 积分项最多贡献的角度，单位为度。
  balance_config.task3_negative_pid.integral_limit_degrees = 2.0F;
  // 速度反馈增益，乘数输入为 pixel/s。
  balance_config.task3_negative_pid.velocity_gain_degrees_per_pixel_s = -0.020F;
  // 微分增益，对位置误差求导，单位为“度/(厘米/秒)”。
  balance_config.task3_negative_pid.derivative_gain_degrees_per_cm_s =
      0.0F; // d
  // 最小允许目标角度，单位为度。
  balance_config.task3_negative_pid.min_angle_degrees = -5.0F;
  // 最大允许目标角度，单位为度。
  balance_config.task3_negative_pid.max_angle_degrees = 7.0F;
  // 相邻两次目标角度变化小于该值时不重复发送电机指令。
  balance_config.task3_negative_pid.minimum_command_delta_degrees = 0.15F;
  // 接受视觉测量的最低置信度，取值范围为 [0, 1]。
  balance_config.task3_negative_pid.min_confidence = 0.50F;

  // T4 PID：目标位置为 0 cm。
  // 位置比例增益，单位为“度/厘米”。
  balance_config.task4_pid.position_gain_degrees_per_cm = -3.0F; // p
  // 位置积分增益，单位为“度/(厘米*秒)”。
  balance_config.task4_pid.integral_gain_degrees_per_cm_s = -0.0F; // i
  // 积分项最多贡献的角度，单位为度。
  balance_config.task4_pid.integral_limit_degrees = 4.0F; // 积分限幅
  // 速度反馈增益，乘数输入为 pixel/s。
  balance_config.task4_pid.velocity_gain_degrees_per_pixel_s = -0.030F; //
  // 微分增益，对位置误差求导，单位为“度/(厘米/秒)”。
  balance_config.task4_pid.derivative_gain_degrees_per_cm_s = 0.0F; // d
  // 最小允许目标角度，单位为度。
  balance_config.task4_pid.min_angle_degrees = -5.0F; // mix
  // 最大允许目标角度，单位为度。
  balance_config.task4_pid.max_angle_degrees = 7.0F; // max
  // 相邻两次目标角度变化小于该值时不重复发送电机指令。
  balance_config.task4_pid.minimum_command_delta_degrees = 0.15F;
  // 接受视觉测量的最低置信度，取值范围为 [0, 1]。
  balance_config.task4_pid.min_confidence = 0.50F;

  // T5 PID：目标位置为 0 cm。
  // 位置比例增益，单位为“度/厘米”。
  balance_config.task5_pid.position_gain_degrees_per_cm = -1.75F;
  // 位置积分增益，单位为“度/(厘米*秒)”。
  balance_config.task5_pid.integral_gain_degrees_per_cm_s = -0.0F;
  // 积分项最多贡献的角度，单位为度。
  balance_config.task5_pid.integral_limit_degrees = 2.0F;
  // 速度反馈增益，乘数输入为 pixel/s。
  balance_config.task5_pid.velocity_gain_degrees_per_pixel_s = -0.020F;
  // 微分增益，对位置误差求导，单位为“度/(厘米/秒)”。
  balance_config.task5_pid.derivative_gain_degrees_per_cm_s = 0.0F; // d
  // 最小允许目标角度，单位为度。
  balance_config.task5_pid.min_angle_degrees = -5.0F;
  // 最大允许目标角度，单位为度。
  balance_config.task5_pid.max_angle_degrees = 7.0F;
  // 相邻两次目标角度变化小于该值时不重复发送电机指令。
  balance_config.task5_pid.minimum_command_delta_degrees = 0.15F;
  // 接受视觉测量的最低置信度，取值范围为 [0, 1]。
  balance_config.task5_pid.min_confidence = 0.50F;

  // T6 PID：目标位置为 task6_target_cm。
  // 位置比例增益，单位为“度/厘米”。
  balance_config.task6_pid.position_gain_degrees_per_cm = -1.6F;
  // 位置积分增益，单位为“度/(厘米*秒)”。
  balance_config.task6_pid.integral_gain_degrees_per_cm_s = -0.4F;
  // 积分项最多贡献的角度，单位为度。
  balance_config.task6_pid.integral_limit_degrees = 0.0F;
  // 速度反馈增益，乘数输入为 pixel/s。
  balance_config.task6_pid.velocity_gain_degrees_per_pixel_s = -0.020F;
  // 最小允许目标角度，单位为度。
  balance_config.task6_pid.min_angle_degrees = -5.0F;
  // 最大允许目标角度，单位为度。
  balance_config.task6_pid.max_angle_degrees = 7.0F;
  // 相邻两次目标角度变化小于该值时不重复发送电机指令。
  balance_config.task6_pid.minimum_command_delta_degrees = 0.15F;
  // 接受视觉测量的最低置信度，取值范围为 [0, 1]。
  balance_config.task6_pid.min_confidence = 0.50F;

  // 视觉位置绝对值的有效上限，单位为厘米。相机把杆两端标记标定为 +/-12.5 cm，
  // 但球贴住机械端点时，检测到的球心可能越过标记数毫米，因此额外保留 1.0 cm
  // 裕量。 这样 +13.14 cm 等端部读数仍可用于把球拉回目标；超过 +/-13.5 cm
  // 的异常值会被拒绝。
  balance_config.max_abs_position_cm = 13.5F;
  // 任务 6 的球位置目标，单位为厘米；正负方向由相机位置坐标系的定义决定。
  balance_config.task6_target_cm = 3.0F;
  // 正式平衡使用 FB 直通限速位置模式；该值是追赶目标角度的速度上限，单位为
  // rpm。 FD 回零/恢复动作也复用该速度值。
  balance_config.motor_speed_rpm = 120;
  // FD 回零/恢复动作的梯形曲线加速度；FB 正式平衡命令不使用该参数。
  balance_config.motor_acceleration = 240;
  // 闭环控制周期，单位为毫秒；20 ms 对应 50 Hz 的控制更新频率。
  balance_config.control_period_ms = 20;
  // 最新视觉数据超过 200 ms
  // 未更新时判定输入超时；该判定独立于下方“连续无效帧”计时。
  balance_config.vision_timeout_ms = 200;
  // 连续收到无效测量时允许相机重新捕获球的宽限时间，单位为毫秒。端点处重捕获曾略超
  // 200 ms， 因此扩展到 500 ms；宽限期内不会立即终止任务，但仍保留上面的 200 ms
  // 陈旧输入超时保护。
  balance_config.invalid_measurement_grace_ms = 500;
  // 无效测量持续 100 ms
  // 后先命令电机回到水平偏置角，避免失去视觉反馈时继续保持大倾角；
  // 若无效状态持续到上述 500 ms 宽限上限，控制器再按视觉故障中止当前任务。
  balance_config.invalid_measurement_neutralize_ms = 100;

  // 控制器按值复制上述配置；balance_config 必须在构造前完成全部参数赋值。
  BuTask::BalanceController balance_controller(balance_config);

#if defined(OZONE_RELEVEL_BEFORE_AUTOMATED_RUN)
  // A halted automated test can leave the motor at its last commanded tilt.
  // Recover the zero established by the preceding task before a new task is
  // allowed to redefine that position as zero. This is deliberately an
  // Ozone-only harness step; normal manual starts assume the operator has
  // levelled the rod as documented.
  bool ozone_relevel_ready = false;
  BujinMotor::ZdtX42s::MotorConfigReadback ozone_motor_readback;
  auto ozone_relevel_result = motor.ReadMotorConfig(ozone_motor_readback);
  if (ozone_relevel_result == LibXR::ErrorCode::NOT_SUPPORT) {
    // A complete X-firmware payload intentionally reports NOT_SUPPORT after
    // selecting the X command family; motion remains supported.
    ozone_relevel_result = LibXR::ErrorCode::OK;
  }
  if (ozone_relevel_result == LibXR::ErrorCode::OK) {
    ozone_relevel_result = motor.Enable(true);
  }
  if (ozone_relevel_result == LibXR::ErrorCode::OK) {
    ozone_relevel_result =
        motor.MoveToAbsoluteAngle(0.0F, balance_config.motor_speed_rpm,
                                  balance_config.motor_acceleration);
  }
  if (ozone_relevel_result == LibXR::ErrorCode::OK) {
    bool existing_zero_reached = false;
    for (uint32_t attempt = 0; attempt < 100U; ++attempt) {
      LibXR::Thread::Sleep(20U);
      float position_degrees = 0.0F;
      if (motor.ReadRealtimeAngle(position_degrees) == LibXR::ErrorCode::OK &&
          position_degrees >= -0.5F && position_degrees <= 0.5F) {
        existing_zero_reached = true;
        break;
      }
    }
    if (!existing_zero_reached) {
      ozone_relevel_result = LibXR::ErrorCode::TIMEOUT;
    }
  }
#if defined(OZONE_AUTOMATED_ZERO_RECOVERY_STEPS)
  // Earlier halted test images could redefine a +20-degree tilt as the next
  // run's zero. Correct a known number of accumulated offsets in bounded
  // signed 20-degree steps, confirming each step before redefining zero again.
  // A positive count unwinds toward -20 degrees; a negative count corrects
  // an over-unwound reference toward +20 degrees.
  constexpr int32_t kOzoneRecoverySteps = OZONE_AUTOMATED_ZERO_RECOVERY_STEPS;
  constexpr uint32_t kOzoneRecoveryStepCount =
      kOzoneRecoverySteps < 0 ? static_cast<uint32_t>(-kOzoneRecoverySteps)
                              : static_cast<uint32_t>(kOzoneRecoverySteps);
  constexpr float kOzoneRecoveryTargetDegrees =
      kOzoneRecoverySteps < 0 ? 20.0F : -20.0F;
  for (uint32_t recovery_step = 0; recovery_step < kOzoneRecoveryStepCount &&
                                   ozone_relevel_result == LibXR::ErrorCode::OK;
       ++recovery_step) {
    ozone_relevel_result = motor.MoveToAbsoluteAngle(
        kOzoneRecoveryTargetDegrees, balance_config.motor_speed_rpm,
        balance_config.motor_acceleration);
    bool recovery_position_reached = false;
    if (ozone_relevel_result == LibXR::ErrorCode::OK) {
      for (uint32_t attempt = 0; attempt < 100U; ++attempt) {
        LibXR::Thread::Sleep(20U);
        float position_degrees = 0.0F;
        if (motor.ReadRealtimeAngle(position_degrees) == LibXR::ErrorCode::OK &&
            position_degrees >= kOzoneRecoveryTargetDegrees - 0.5F &&
            position_degrees <= kOzoneRecoveryTargetDegrees + 0.5F) {
          recovery_position_reached = true;
          break;
        }
      }
    }
    if (!recovery_position_reached) {
      ozone_relevel_result = LibXR::ErrorCode::TIMEOUT;
      break;
    }
    ozone_relevel_result = motor.SetCurrentPositionAsZero();
  }
#endif
  if (ozone_relevel_result == LibXR::ErrorCode::OK) {
    ozone_relevel_result =
        motor.MoveToAbsoluteAngle(0.0F, balance_config.motor_speed_rpm,
                                  balance_config.motor_acceleration);
  }
  if (ozone_relevel_result == LibXR::ErrorCode::OK) {
    for (uint32_t attempt = 0; attempt < 100U; ++attempt) {
      LibXR::Thread::Sleep(20U);
      float position_degrees = 0.0F;
      if (motor.ReadRealtimeAngle(position_degrees) == LibXR::ErrorCode::OK &&
          position_degrees >= -0.5F && position_degrees <= 0.5F) {
        ozone_relevel_ready = true;
        break;
      }
    }
  }
#endif

#if defined(OZONE_AUTOMATED_T3_TEST)
  // Ozone test path: Monitor -> T3, then submit the ordinary start request
  // before the control task runs. All regular safety gates remain active.
  balance_controller.SelectNextTask();
  balance_controller.ToggleRun();
#endif

#if defined(OZONE_AUTOMATED_RUN_TASK)
  // Ozone closed-loop task test: select T3/T4/T5/T6 and use the ordinary
  // RUN request. Vision validation, enable, current-position zeroing, timeout
  // and all motor safety checks remain exactly the same as during a
  // button-started task.
  // ContestTask uses its visible task numbers (T3 == 3, …), while the first
  // SelectNextTask call transitions VIEW/Monitor to T3.  Starting the loop at
  // the enum value therefore performed zero selections for T3 and silently
  // exercised the VIEW path.  Use the number of transitions from VIEW instead.
#if defined(OZONE_RELEVEL_BEFORE_AUTOMATED_RUN)
  if (ozone_relevel_ready) {
#endif
    for (uint8_t selection = 0;
         selection < (OZONE_AUTOMATED_RUN_TASK -
                      static_cast<uint8_t>(BuTask::ContestTask::Task3) + 1U);
         ++selection) {
      balance_controller.SelectNextTask();
    }
    balance_controller.ToggleRun();
#if defined(OZONE_RELEVEL_BEFORE_AUTOMATED_RUN)
  }
#endif
#endif

#if defined(OZONE_AUTOMATED_IDLE_MODE_SELECTIONS)
  // Ozone display test path: choose a task but do not start it.  It verifies
  // that non-VIEW idle modes keep showing fresh vision telemetry without
  // issuing any motor command.
  for (uint8_t selection = 0; selection < OZONE_AUTOMATED_IDLE_MODE_SELECTIONS;
       ++selection) {
    balance_controller.SelectNextTask();
  }
#endif

  BuTask::OperatorPanelConfig panel_config;
  panel_config.controller = &balance_controller;
  panel_config.select_button = &PA4;
  panel_config.run_button = &PA5;
  panel_config.homing_button = &PC13;
  panel_config.display = &lcd1;
  BuTask::OperatorPanel operator_panel(panel_config);

  LibXR::Thread balance_control_thread;
  balance_control_thread.Create(&balance_controller,
                                BuTask::BalanceControlThread, "balance_ctrl",
                                1536, LibXR::Thread::Priority::MEDIUM);
  LibXR::Thread operator_panel_thread;
  operator_panel_thread.Create(&operator_panel, BuTask::OperatorPanelThread,
                               "balance_ui", 1024,
                               LibXR::Thread::Priority::LOW);
  while (true) {
    Thread::Sleep(UINT32_MAX);
  }
  /* User Code End 3 */
}
