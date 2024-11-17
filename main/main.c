#include <math.h>
#include <stdio.h>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd.h"
#include "render.h"

void app_main(void) {
  ssd1306_lcd_panel_t *lcd = lcd_setup();
  setup_lv_timer(lcd);
  setup_lv_ui(lcd);
  xTaskCreate(lv_timer_handler_task, "lv_timer_handler_task",
              LV_TIMER_HANDLER_TASK_STACK_SIZE, lcd,
              LV_TIMER_HANDLER_TASK_PRIORITY, NULL);
  vTaskDelete(NULL);
}
