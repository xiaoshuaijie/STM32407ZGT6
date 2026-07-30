#include "app_main.h"

#include "bu_task.hpp"
#include "cdc_uart.hpp"
#include "flash_map.hpp"
#include "libxr.hpp"
#include "main.h"
#include "maxican.hpp"
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
extern UART_HandleTypeDef huart3;

/* DMA Resources */
static uint8_t spi1_tx_buf[32];
static uint8_t spi1_rx_buf[32];
static uint8_t usart1_tx_buf[128];
static uint8_t usart1_rx_buf[128];
static uint8_t usart3_tx_buf[128];
static uint8_t usart3_rx_buf[128];
static uint8_t usb_otg_fs_ep0_in_buf[8];
static uint8_t usb_otg_fs_ep0_out_buf[8];
static uint8_t usb_otg_fs_ep1_in_buf[128];
static uint8_t usb_otg_fs_ep1_out_buf[128];
static uint8_t usb_otg_fs_ep2_in_buf[16];

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
  STM32GPIO LCD_CS(LCD_CS_GPIO_Port, LCD_CS_Pin, EXTI9_5_IRQn);



  STM32SPI spi1(&hspi1, spi1_rx_buf, spi1_tx_buf, 3);

  lcd_io lcd1_io = {
      &spi1,
      {&LCD_RST, false},
      {&LCD_PWR, false},
      {&LCD_CS, false},
      {&LCD_DC, false},
      {nullptr, false},
  };
  lcd lcd1{};
  lcd1.io = &lcd1_io;
  lcd_init_dev(&lcd1, LCD_1_14_INCH, LCD_ROTATE_0);
  lcd_show_string(&lcd1, 0, 0,
                  reinterpret_cast<const uint8_t *>("jie"));

  STM32UART usart1(&huart1,
              usart1_rx_buf, usart1_tx_buf, 5);

  STM32UART usart3(&huart3,
              usart3_rx_buf, usart3_tx_buf, 5);

  BujinMotor::ZdtX42s motor(usart3);
  Maxican::BallMailbox ball_mailbox;
  Maxican::BallReceiver maxican_receiver(usart1, ball_mailbox);
  LibXR::Thread maxican_receive_thread;
  maxican_receive_thread.Create(&maxican_receiver, Maxican::ReceiveThread,
                                "maxican_rx", 1024,
                                LibXR::Thread::Priority::HIGH);

  // Tune the gain signs, center, and angle limits for the actual mechanism.
  BuTask::BallTrackingConfig ball_tracking_config = {
      .motor = &motor,
      .mailbox = &ball_mailbox,
      .center_position_cm = 0.0F,
      .angle_offset_degrees = 0.0F,
      .position_gain_degrees_per_cm = 1.0F,
      .velocity_gain_degrees_per_pixel_s = 0.0F,
      .min_angle_degrees = -30.0F,
      .max_angle_degrees = 30.0F,
      .min_confidence = 0.50F,
      .minimum_command_delta_degrees = 0.2F,
      .motor_speed_rpm = 300,
      .motor_acceleration = 100,
      .input_timeout_ms = 200,
  };
  LibXR::Thread ball_tracking_thread;
  ball_tracking_thread.Create(&ball_tracking_config, BuTask::BallTrackingThread,
                              "ball_motor", 1024,
                              LibXR::Thread::Priority::MEDIUM);

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
   STDIO::read_ = usb_otg_fs_cdc.read_port_;
   STDIO::write_ = usb_otg_fs_cdc.write_port_;
   LibXR::RamFS ramfs;
   LibXR::Terminal<> terminal(ramfs);
   LibXR::Thread term_thread;
   term_thread.Create(&terminal, terminal.ThreadFun, "terminal", 1024,
                     LibXR::Thread::Priority::MEDIUM);
  // clang-format on
  // NOLINTEND
  /* User Code Begin 3 */
  while (true) {
    LibXR::STDIO::Printf<"Hello, %d">(123);
    Thread::Sleep(UINT32_MAX);
  }
  /* User Code End 3 */
}
