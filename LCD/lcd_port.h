#ifndef __LCD_PORT_H
#define __LCD_PORT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __gpio_io {
    void* gpio;
    bool invert;
} gpio_io;

typedef struct __lcd_io {
    void* spi;
    gpio_io rst;
    gpio_io bl;
    gpio_io cs;
    gpio_io dc;
    gpio_io te;
} lcd_io;

void lcd_delay(uint32_t delay);

/* C-compatible LibXR bridge functions, implemented in lcd_libxr_port.cpp. */
void lcd_libxr_delay(uint32_t delay);
void lcd_libxr_gpio_write(void* gpio, bool value);
bool lcd_libxr_spi_transmit(void* spi, const uint8_t* data, uint32_t len);

void lcd_io_rst(lcd_io* lcdio, bool flag);
void lcd_io_bl(lcd_io* lcdio, bool flag);
void lcd_io_cs(lcd_io* lcdio, bool flag);
void lcd_io_dc(lcd_io* lcdio, bool flag);
/****** 底层接口 SPI  ******/
void lcd_write_byte(lcd_io* lcdio, uint8_t data);
void lcd_write_halfword(lcd_io* lcdio, uint16_t data);
void lcd_write_bulk(lcd_io* lcdio, uint8_t* data, uint32_t len);
void lcd_write_reg(lcd_io* lcdio, uint8_t data);

#ifdef __cplusplus
}
#endif

#endif
