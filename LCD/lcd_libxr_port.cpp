#include "lcd_port.h"

#include "gpio.hpp"
#include "libxr.hpp"
#include "stm32_spi.hpp"

extern "C" void lcd_libxr_delay(uint32_t delay)
{
  LibXR::Thread::Sleep(delay);
}

extern "C" void lcd_libxr_gpio_write(void* gpio, bool value)
{
  if (gpio != nullptr)
  {
    static_cast<LibXR::GPIO*>(gpio)->Write(value);
  }
}

extern "C" bool lcd_libxr_spi_transmit(void* spi, const uint8_t* data, uint32_t len)
{
  if (spi == nullptr || (data == nullptr && len > 0u))
  {
    return false;
  }

  auto* const bus = static_cast<LibXR::STM32SPI*>(spi);
  return bus->TransmitBlocking({data, len}) == LibXR::ErrorCode::OK;
}
