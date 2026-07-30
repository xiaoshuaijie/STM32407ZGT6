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
  STM32GPIO LCD_DC(LCD_DC_GPIO_Port, LCD_DC_Pin);
  STM32GPIO PC0(GPIOC, GPIO_PIN_0);
  STM32GPIO PC1(GPIOC, GPIO_PIN_1);
  STM32GPIO PC2(GPIOC, GPIO_PIN_2);
  STM32GPIO LCD_PWR(LCD_PWR_GPIO_Port, LCD_PWR_Pin);
  STM32GPIO LCD_RST(LCD_RST_GPIO_Port, LCD_RST_Pin);
  STM32GPIO LCD_CS(LCD_CS_GPIO_Port, LCD_CS_Pin);
  PA4.SetConfig({GPIO::Direction::INPUT, GPIO::Pull::DOWN});
  PA5.SetConfig({GPIO::Direction::INPUT, GPIO::Pull::DOWN});


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

  // Tune these mechanical gains on the assembled car before contest runs.
  BuTask::BalanceControllerConfig balance_config;
  balance_config.motor = &motor;
  balance_config.mailbox = &ball_mailbox;
  balance_config.angle_offset_degrees = 0.0F;
  balance_config.position_gain_degrees_per_cm = 1.0F;
  balance_config.velocity_gain_degrees_per_pixel_s = 0.0F;
  balance_config.min_angle_degrees = -20.0F;
  balance_config.max_angle_degrees = 20.0F;
  balance_config.minimum_command_delta_degrees = 0.2F;
  balance_config.min_confidence = 0.50F;
  balance_config.task6_target_cm = 3.0F;
  balance_config.motor_speed_rpm = 300;
  balance_config.motor_acceleration = 100;
  balance_config.control_period_ms = 20;
  balance_config.vision_timeout_ms = 200;
  BuTask::BalanceController balance_controller(balance_config);

  BuTask::OperatorPanelConfig panel_config;
  panel_config.controller = &balance_controller;
  panel_config.select_button = &PA4;
  panel_config.run_button = &PA5;
  panel_config.display = &lcd1;
  BuTask::OperatorPanel operator_panel(panel_config);

  LibXR::Thread balance_control_thread;
  balance_control_thread.Create(&balance_controller, BuTask::BalanceControlThread,
                                "balance_ctrl", 1536,
                                LibXR::Thread::Priority::MEDIUM);
  LibXR::Thread operator_panel_thread;
  operator_panel_thread.Create(&operator_panel, BuTask::OperatorPanelThread,
                               "balance_ui", 1024,
                               LibXR::Thread::Priority::LOW);
  while (true) {
    Thread::Sleep(UINT32_MAX);
  }
  /* User Code End 3 */
}
