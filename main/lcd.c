#include "lcd.h"

#include "driver/i2c_master.h"
#include "esp_lcd_panel_dev.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_ssd1306.h"
#include "render.h"

#define TAG "LCD"
#define CHECK_ALLOC(ptr)                                    \
  do {                                                      \
    if (ptr == NULL) {                                      \
      ESP_LOGE(TAG, "Failed to allocate memory: %s", #ptr); \
      abort();                                              \
    }                                                       \
  } while (0)

static bool lv_tick_timer_cb(gptimer_handle_t timer,
                             const gptimer_alarm_event_data_t *edata,
                             void *user_ctx);
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area,
                          uint8_t *px_map);

ssd1306_lcd_panel_t *lcd_setup(void) {
  ssd1306_lcd_panel_t *lcd =
      heap_caps_calloc(1, sizeof(ssd1306_lcd_panel_t), MALLOC_CAP_8BIT);
  CHECK_ALLOC(lcd);
  lcd->lcd_buf = heap_caps_calloc(LCD_BUF_SIZE, sizeof(uint8_t),
                                  MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
  CHECK_ALLOC(lcd->lcd_buf);
  lcd->draw_buf0 =
      heap_caps_calloc(LVGL_DRAW_BUF_SIZE, sizeof(uint8_t), MALLOC_CAP_8BIT);
  CHECK_ALLOC(lcd->draw_buf0);
  lcd->draw_buf1 =
      heap_caps_calloc(LVGL_DRAW_BUF_SIZE, sizeof(uint8_t), MALLOC_CAP_8BIT);
  CHECK_ALLOC(lcd->draw_buf1);
  lcd->canvas_buf =
      heap_caps_calloc(lv_color_format_get_size(LV_COLOR_FORMAT_ARGB8888) *
                           LCD_WIDTH / sizeof(uint8_t) * LCD_HEIGHT,
                       sizeof(uint8_t), MALLOC_CAP_8BIT);
  CHECK_ALLOC(lcd->canvas_buf);
  ESP_LOGD(TAG, "LCD struct allocated");

  lcd->lcd_buf_mutex = xSemaphoreCreateMutex();
  CHECK_ALLOC(lcd->lcd_buf_mutex);
  lcd->lvgl_mutex = xSemaphoreCreateMutex();
  CHECK_ALLOC(lcd->lvgl_mutex);
  ESP_LOGD(TAG, "LCD mutex created");

  temperature_sensor_config_t temp_sensor_conf = {
      .range_min = 20,
      .range_max = 50,
      .clk_src = TEMPERATURE_SENSOR_CLK_SRC_DEFAULT,
  };
  ESP_ERROR_CHECK(
      temperature_sensor_install(&temp_sensor_conf, &lcd->temp_handle));
  ESP_ERROR_CHECK(temperature_sensor_enable(lcd->temp_handle));
  ESP_LOGD(TAG, "Temperature sensor installed and enabled");

  i2c_master_bus_config_t bus_config = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .sda_io_num = LCD_PIN_NUM_SDA,
      .scl_io_num = LCD_PIN_NUM_SCL,
      .flags.enable_internal_pullup = true,
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &lcd->i2c_bus));
  ESP_LOGD(TAG, "I2C master bus created");

  esp_lcd_panel_io_i2c_config_t io_config = {
      .dev_addr = LCD_I2C_HW_ADDR,
      .scl_speed_hz = LCD_PIXEL_CLOCK_HZ,
      .control_phase_bytes = 1,
      .lcd_cmd_bits = LCD_CMD_BITS,
      .lcd_param_bits = LCD_PARAM_BITS,
      .dc_bit_offset = 6,
  };
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_io_i2c(lcd->i2c_bus, &io_config, &lcd->io_handle));
  ESP_LOGD(TAG, "LCD panel IO handle created");

  esp_lcd_panel_dev_config_t dev_config = {
      .bits_per_pixel = 1,
      .reset_gpio_num = GPIO_NUM_NC,
  };
  esp_lcd_panel_ssd1306_config_t ssd1306_config = {
      .height = LCD_HEIGHT,
  };
  dev_config.vendor_config = &ssd1306_config;
  ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(lcd->io_handle, &dev_config,
                                            &lcd->panel_handle));
  ESP_LOGD(TAG, "LCD panel device handle created");

  ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd->panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(lcd->panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd->panel_handle, true));
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(lcd->panel_handle, false));
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(lcd->panel_handle, true, true));
  ESP_LOGD(TAG, "LCD panel initialized");

  lv_init();
  lcd->lv_disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
  lv_display_set_user_data(lcd->lv_disp, lcd);
  lv_display_set_color_format(lcd->lv_disp, LV_COLOR_FORMAT_I1);
  lv_display_set_buffers(lcd->lv_disp, lcd->draw_buf0, lcd->draw_buf1,
                         LVGL_DRAW_BUF_SIZE, LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(lcd->lv_disp, lvgl_flush_cb);
  ESP_LOGD(TAG, "LVGL display created");

  return lcd;
}

void setup_lv_timer(ssd1306_lcd_panel_t *lcd) {
  gptimer_config_t timer_config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1 * 1000 * 1000,
  };
  ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &lcd->lv_tick_timer));
  gptimer_alarm_config_t alarm_config = {
      .reload_count = 0,  // counter will reload with 0 on alarm event
      .alarm_count = LV_TICK_PERIOD_MS * 1000,  // alarm period in us
      .flags.auto_reload_on_alarm = true,       // enable auto-reload
  };
  ESP_ERROR_CHECK(gptimer_set_alarm_action(lcd->lv_tick_timer, &alarm_config));
  gptimer_event_callbacks_t timer_cbs = {
      .on_alarm = lv_tick_timer_cb,
  };
  ESP_ERROR_CHECK(
      gptimer_register_event_callbacks(lcd->lv_tick_timer, &timer_cbs, lcd));
  ESP_ERROR_CHECK(gptimer_enable(lcd->lv_tick_timer));
  ESP_ERROR_CHECK(gptimer_start(lcd->lv_tick_timer));
}

static void update_label_cb(lv_timer_t *timer);

void setup_lv_ui(ssd1306_lcd_panel_t *lcd) {
  lv_obj_t *scr = lv_display_get_screen_active(lcd->lv_disp);

  lv_obj_t *stat = lv_label_create(scr);
  lv_obj_set_user_data(stat, lcd);
  lv_obj_align(stat, LV_ALIGN_TOP_LEFT, 2, 2);
  lv_obj_set_style_text_color(stat, lv_color_white(), 0);
  lv_label_set_text(stat, "M: -% T: -C");
  lv_timer_create(update_label_cb, 1000, stat);

  lv_obj_t *canvas = lv_canvas_create(scr);
  render_data_t *data = setup_render_data(lcd);
  lv_obj_set_user_data(canvas, data);
  lv_obj_set_size(canvas, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_pos(canvas, 0, 0);
  lv_canvas_set_buffer(canvas, lcd->canvas_buf, LCD_WIDTH, LCD_HEIGHT,
                       LV_COLOR_FORMAT_ARGB8888);
  lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);
  lv_timer_create(canvas_render_cb, 16, canvas);

  lv_obj_move_foreground(stat);
}

static void update_label_cb(lv_timer_t *timer) {
  char buf[16];

  lv_obj_t *label = lv_timer_get_user_data(timer);
  ssd1306_lcd_panel_t *lcd = lv_obj_get_user_data(label);

  const size_t total_mem = heap_caps_get_total_size(MALLOC_CAP_8BIT);
  const size_t free_mem = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  float temperature;
  ESP_ERROR_CHECK(
      temperature_sensor_get_celsius(lcd->temp_handle, &temperature));

  snprintf(buf, sizeof(buf), "M: %d%% T: %.0fC",
           ((total_mem - free_mem) * 100) / (total_mem), temperature);

  if (xSemaphoreTake(lcd->lvgl_mutex, pdMS_TO_TICKS(LV_UI_REFRESH_PERIOD_MS)) ==
      pdTRUE) {
    lv_label_set_text(label, buf);
    xSemaphoreGive(lcd->lvgl_mutex);
  } else {
    ESP_LOGW(TAG, "Failed to take LVGL mutex");
  }
}

void lv_timer_handler_task(void *pvParameters) {
  __unused ssd1306_lcd_panel_t *lcd = (ssd1306_lcd_panel_t *)pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();

  while (true) {
    lv_timer_handler();  // LVGL 메인 루프 호출
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(LV_UI_REFRESH_PERIOD_MS));
  }
}

static bool IRAM_ATTR lv_tick_timer_cb(gptimer_handle_t timer,
                                       const gptimer_alarm_event_data_t *edata,
                                       void *user_ctx) {
  lv_tick_inc(LV_TICK_PERIOD_MS);
  return true;
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area,
                          uint8_t *px_map) {
  ssd1306_lcd_panel_t *lcd = lv_display_get_user_data(disp);

  if (xSemaphoreTake(lcd->lvgl_mutex, pdMS_TO_TICKS(LV_UI_REFRESH_PERIOD_MS)) !=
      pdTRUE) {
    lv_display_flush_ready(disp);
    ESP_LOGW(TAG, "Failed to take LVGL mutex");
    return;
  }

  // skip palette
  px_map += LV_PALETTE_SIZE;

  uint16_t hor_res = lv_display_get_horizontal_resolution(disp);
  int x1 = area->x1;
  int x2 = area->x2;
  int y1 = area->y1;
  int y2 = area->y2;

  // copy example code
  for (int y = y1; y <= y2; y++) {
    for (int x = x1; x <= x2; x++) {
      /* The order of bits is MSB first
                  MSB           LSB
         bits      7 6 5 4 3 2 1 0
         pixels    0 1 2 3 4 5 6 7
                  Left         Right
      */
      bool chroma_color =
          (px_map[(hor_res >> 3) * y + (x >> 3)] & 1 << (7 - x % 8));

      /* Write to the buffer as required for the display.
       * It writes only 1-bit for monochrome displays mapped vertically.*/
      uint8_t *buf = lcd->lcd_buf + hor_res * (y >> 3) + (x);
      // invert color
      if (!chroma_color) {
        (*buf) &= ~(1 << (y % 8));
      } else {
        (*buf) |= (1 << (y % 8));
      }
    }
  }
  // draw with esp driver
  ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
      lcd->panel_handle, x1, y1, x2 - x1 + 1, y2 - y1 + 1, lcd->lcd_buf));
  // unlock ui and lcd mutex
  xSemaphoreGive(lcd->lvgl_mutex);
  // notify flush done
  lv_display_flush_ready(disp);
}
