#ifndef __LCD_H__
#define __LCD_H__

#include "driver/gptimer.h"
#include "driver/i2c_master.h"
#include "driver/temperature_sensor.h"
#include "esp_lcd_types.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#define LCD_PIXEL_CLOCK_HZ (400 * 1000)
#define LCD_PIN_NUM_SDA GPIO_NUM_8
#define LCD_PIN_NUM_SCL GPIO_NUM_9
#define LCD_CONTROLLER "SSD1306"
#define LCD_I2C_HW_ADDR 0x3C
#define LCD_WIDTH 128
#define LCD_HEIGHT 64
#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8
#define LCD_BUF_SIZE (LCD_WIDTH * LCD_HEIGHT / 8)

#define LV_PALETTE_SIZE 8
#define LVGL_DRAW_BUF_SIZE (LCD_BUF_SIZE + LV_PALETTE_SIZE)

#define LV_TICK_PERIOD_MS 8
#define LV_TIMER_HANDLER_TASK_STACK_SIZE 8192
#define LV_TIMER_HANDLER_TASK_PRIORITY 5
#define LV_UI_REFRESH_PERIOD_MS 16

typedef struct ssd1306_lcd_panel_s {
  temperature_sensor_handle_t temp_handle;
  i2c_master_bus_handle_t i2c_bus;
  esp_lcd_panel_io_handle_t io_handle;
  esp_lcd_panel_handle_t panel_handle;
  gptimer_handle_t lv_tick_timer;
  lv_display_t *lv_disp;
  lv_theme_t *theme;
  uint8_t *lcd_buf;    // do not directly access this buffer except in flush_cb
  uint8_t *draw_buf0;  // do not directly access this buffer
  uint8_t *draw_buf1;  // do not directly access this buffer
  void *canvas_buf;    // do not directly access this buffer
  SemaphoreHandle_t lcd_buf_mutex;
  SemaphoreHandle_t lvgl_mutex;
} ssd1306_lcd_panel_t;

ssd1306_lcd_panel_t *lcd_setup(void);
void setup_lv_timer(ssd1306_lcd_panel_t *lcd);
void setup_lv_ui(ssd1306_lcd_panel_t *lcd);
void lv_timer_handler_task(void *pvParameters);

#endif  // __LCD_H__