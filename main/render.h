#ifndef __RENDER_C__
#define __RENDER_C__

#include <math.h>

#include "lcd.h"

typedef struct {
  float x, y, z;
} vec3_t;

typedef struct {
  vec3_t *vertices;
  uint32_t vertex_count;
  uint8_t (*edges)[2];
  uint32_t edge_count;
  vec3_t offset;
  vec3_t rotation;
} object3d_t;

typedef struct {
  ssd1306_lcd_panel_t *lcd;
  vec3_t camera_pos;
  vec3_t camera_dir;
  float fov;
  object3d_t *objects;
  uint8_t object_count;
} render_data_t;

#define FOV 75
#define CUBE_VERTEX_COUNT 8
#define CUBE_EDGE_COUNT 12
#define CONE_SIDES 12
#define CYLINDER_SIDES 12
#define SPHERE_LATITUDE_COUNT 6
#define SPHERE_LONGITUDE_COUNT 12
#define OBJECT_COUNT 4

render_data_t *setup_render_data(ssd1306_lcd_panel_t *lcd);
void canvas_render_cb(lv_timer_t *timer);

#endif  // __RENDER_C__