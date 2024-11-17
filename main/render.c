#include "render.h"

#include <math.h>
#include <string.h>

#include "lcd.h"
#define TAG "RENDER"

static void cube_init(object3d_t *obj, vec3_t *offset, vec3_t *rotate);
static void cone_init(object3d_t *obj, vec3_t *offset, vec3_t *rotate);
static void cylinder_init(object3d_t *obj, vec3_t *offset, vec3_t *rotate);
static void sphere_init(object3d_t *obj, vec3_t *offset, vec3_t *rotate);
static void draw_object(lv_layer_t *layer, render_data_t *data,
                        object3d_t *object);
static void inline project_vertex(vec3_t *vertex, vec3_t *camera_pos, float fov,
                                  float *px, float *py);
static void inline rotate_vertex(vec3_t *vertex, vec3_t *rotation);
static void inline rotate_x(vec3_t *vertex, float angle);
static void inline rotate_y(vec3_t *vertex, float angle);
static void inline rotate_z(vec3_t *vertex, float angle);

render_data_t *setup_render_data(ssd1306_lcd_panel_t *lcd) {
  render_data_t *data =
      heap_caps_calloc(1, sizeof(render_data_t), MALLOC_CAP_8BIT);
  data->lcd = lcd;
  data->camera_pos = (vec3_t){0.0, 0.0, -8.0};
  data->camera_dir = (vec3_t){0.0, 0.0, 1.0};
  data->fov = FOV;
  data->objects =
      heap_caps_calloc(OBJECT_COUNT, sizeof(object3d_t), MALLOC_CAP_8BIT);
  data->object_count = OBJECT_COUNT;

  cube_init(data->objects, &(vec3_t){-4.0, 0.0, 0.0}, &(vec3_t){0.0, 0.0, 0.0});
  cone_init(data->objects + 1, &(vec3_t){-4.0 / 3, 0.0, 0.0},
            &(vec3_t){0.0, 0.0, 0.0});
  cylinder_init(data->objects + 2, &(vec3_t){4.0 / 3, 0.0, 0.0},
                &(vec3_t){0.0, 0.0, 0.0});
  sphere_init(data->objects + 3, &(vec3_t){4.0, 0.0, 0.0},
              &(vec3_t){0.0, 0.0, 0.0});

  return data;
}

void canvas_render_cb(lv_timer_t *timer) {
  lv_obj_t *canvas = lv_timer_get_user_data(timer);
  render_data_t *data = lv_obj_get_user_data(canvas);
  if (xSemaphoreTake(data->lcd->lvgl_mutex,
                     pdMS_TO_TICKS(LV_UI_REFRESH_PERIOD_MS)) != pdTRUE) {
    ESP_LOGW(TAG, "Failed to take LVGL mutex");
    return;
  }
  lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);

  lv_layer_t layer;
  lv_canvas_init_layer(canvas, &layer);

  for (int i = 0; i < data->object_count; i++) {
    draw_object(&layer, data, &data->objects[i]);
    data->objects[i].rotation.x += 0.1 + i * 0.01;
    data->objects[i].rotation.y += 0.03 + i * 0.01;
    data->objects[i].rotation.z += 0.02 + i * 0.01;
    if (data->objects[i].rotation.x > 2 * M_PI)
      data->objects[i].rotation.x -= 2 * M_PI;
    if (data->objects[i].rotation.y > 2 * M_PI)
      data->objects[i].rotation.y -= 2 * M_PI;
    if (data->objects[i].rotation.z > 2 * M_PI)
      data->objects[i].rotation.z -= 2 * M_PI;
  }

  lv_canvas_finish_layer(canvas, &layer);

  xSemaphoreGive(data->lcd->lvgl_mutex);
}

static void cube_init(object3d_t *obj, vec3_t *offset, vec3_t *rotate) {
  obj->vertices =
      heap_caps_calloc(CUBE_VERTEX_COUNT, sizeof(vec3_t), MALLOC_CAP_8BIT);
  obj->vertices[0] = (vec3_t){-1, -1, -1};
  obj->vertices[1] = (vec3_t){1, -1, -1};
  obj->vertices[2] = (vec3_t){1, 1, -1};
  obj->vertices[3] = (vec3_t){-1, 1, -1};
  obj->vertices[4] = (vec3_t){-1, -1, 1};
  obj->vertices[5] = (vec3_t){1, -1, 1};
  obj->vertices[6] = (vec3_t){1, 1, 1};
  obj->vertices[7] = (vec3_t){-1, 1, 1};
  obj->vertex_count = CUBE_VERTEX_COUNT;
  obj->edges = heap_caps_calloc(12, sizeof(uint8_t[2]), MALLOC_CAP_8BIT);
  obj->edges[0][0] = 0;
  obj->edges[0][1] = 1;
  obj->edges[1][0] = 1;
  obj->edges[1][1] = 2;
  obj->edges[2][0] = 2;
  obj->edges[2][1] = 3;
  obj->edges[3][0] = 3;
  obj->edges[3][1] = 0;
  obj->edges[4][0] = 4;
  obj->edges[4][1] = 5;
  obj->edges[5][0] = 5;
  obj->edges[5][1] = 6;
  obj->edges[6][0] = 6;
  obj->edges[6][1] = 7;
  obj->edges[7][0] = 7;
  obj->edges[7][1] = 4;
  obj->edges[8][0] = 0;
  obj->edges[8][1] = 4;
  obj->edges[9][0] = 1;
  obj->edges[9][1] = 5;
  obj->edges[10][0] = 2;
  obj->edges[10][1] = 6;
  obj->edges[11][0] = 3;
  obj->edges[11][1] = 7;
  obj->edge_count = CUBE_EDGE_COUNT;
  obj->offset = *offset;
  obj->rotation = *rotate;
}

static void cone_init(object3d_t *obj, vec3_t *offset, vec3_t *rotate) {
  obj->vertices =
      heap_caps_calloc(CONE_SIDES + 1, sizeof(vec3_t), MALLOC_CAP_8BIT);
  obj->vertices[CONE_SIDES] = (vec3_t){0, 2, 0};  // 꼭짓점
  obj->edges =
      heap_caps_calloc(CONE_SIDES * 2, sizeof(uint8_t[2]), MALLOC_CAP_8BIT);
  for (int i = 0; i < CONE_SIDES; i++) {
    float angle = i * 2 * M_PI / CONE_SIDES;
    obj->vertices[i] = (vec3_t){cos(angle), -1, sin(angle)};
    obj->edges[i][0] = i;
    obj->edges[i][1] = (i + 1) % CONE_SIDES;
    obj->edges[CONE_SIDES + i][0] = i;
    obj->edges[CONE_SIDES + i][1] = CONE_SIDES;
  }
  obj->vertex_count = CONE_SIDES + 1;
  obj->edge_count = CONE_SIDES * 2;
  obj->offset = *offset;
  obj->rotation = *rotate;
}

static void cylinder_init(object3d_t *obj, vec3_t *offset, vec3_t *rotate) {
  obj->vertices =
      heap_caps_calloc(CYLINDER_SIDES * 2, sizeof(vec3_t), MALLOC_CAP_8BIT);
  obj->edges =
      heap_caps_calloc(CYLINDER_SIDES * 3, sizeof(uint8_t[2]), MALLOC_CAP_8BIT);
  for (int i = 0; i < CYLINDER_SIDES; i++) {
    float angle = i * 2 * M_PI / CYLINDER_SIDES;
    obj->vertices[i] = (vec3_t){cos(angle), -1, sin(angle)};
    obj->vertices[CYLINDER_SIDES + i] = (vec3_t){cos(angle), 1, sin(angle)};
    obj->edges[i][0] = i;
    obj->edges[i][1] = (i + 1) % CYLINDER_SIDES;
    obj->edges[CYLINDER_SIDES + i][0] = CYLINDER_SIDES + i;
    obj->edges[CYLINDER_SIDES + i][1] =
        CYLINDER_SIDES + (i + 1) % CYLINDER_SIDES;
    obj->edges[2 * CYLINDER_SIDES + i][0] = i;
    obj->edges[2 * CYLINDER_SIDES + i][1] = CYLINDER_SIDES + i;
  }
  obj->vertex_count = CYLINDER_SIDES * 2;
  obj->edge_count = CYLINDER_SIDES * 3;
  obj->offset = *offset;
  obj->rotation = *rotate;
}

static void sphere_init(object3d_t *obj, vec3_t *offset, vec3_t *rotate) {
  obj->vertices =
      heap_caps_calloc(SPHERE_LATITUDE_COUNT * SPHERE_LONGITUDE_COUNT,
                       sizeof(vec3_t), MALLOC_CAP_8BIT);
  obj->edges =
      heap_caps_calloc(SPHERE_LATITUDE_COUNT * SPHERE_LONGITUDE_COUNT * 2,
                       sizeof(uint8_t[2]), MALLOC_CAP_8BIT);
  for (int i = 0; i < SPHERE_LATITUDE_COUNT; i++) {
    float theta = i * M_PI / (SPHERE_LATITUDE_COUNT - 1);
    for (int j = 0; j < SPHERE_LONGITUDE_COUNT; j++) {
      float phi = j * 2 * M_PI / SPHERE_LONGITUDE_COUNT;
      obj->vertices[i * SPHERE_LONGITUDE_COUNT + j] =
          (vec3_t){sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta)};
      if (i > 0) {
        obj->edges[(i - 1) * SPHERE_LONGITUDE_COUNT + j][0] =
            (i - 1) * SPHERE_LONGITUDE_COUNT + j;
        obj->edges[(i - 1) * SPHERE_LONGITUDE_COUNT + j][1] =
            i * SPHERE_LONGITUDE_COUNT + j;
        obj->edges[(i - 1) * SPHERE_LONGITUDE_COUNT + j +
                   SPHERE_LATITUDE_COUNT * SPHERE_LONGITUDE_COUNT][0] =
            (i - 1) * SPHERE_LONGITUDE_COUNT + j;
        obj->edges[(i - 1) * SPHERE_LONGITUDE_COUNT + j +
                   SPHERE_LATITUDE_COUNT * SPHERE_LONGITUDE_COUNT][1] =
            (i - 1) * SPHERE_LONGITUDE_COUNT + (j + 1) % SPHERE_LONGITUDE_COUNT;
      }
    }
    obj->edges[i * SPHERE_LONGITUDE_COUNT][0] =
        i * SPHERE_LONGITUDE_COUNT + SPHERE_LONGITUDE_COUNT - 1;
    obj->edges[i * SPHERE_LONGITUDE_COUNT][1] = i * SPHERE_LONGITUDE_COUNT;
    obj->edges[i * SPHERE_LONGITUDE_COUNT +
               SPHERE_LATITUDE_COUNT * SPHERE_LONGITUDE_COUNT][0] =
        i * SPHERE_LONGITUDE_COUNT;
    obj->edges[i * SPHERE_LONGITUDE_COUNT +
               SPHERE_LATITUDE_COUNT * SPHERE_LONGITUDE_COUNT][1] =
        i * SPHERE_LONGITUDE_COUNT + 1;

    obj->edges[(i + 1) * SPHERE_LONGITUDE_COUNT - 1][0] =
        i * SPHERE_LONGITUDE_COUNT + SPHERE_LONGITUDE_COUNT - 1;
    obj->edges[(i + 1) * SPHERE_LONGITUDE_COUNT - 1][1] =
        i * SPHERE_LONGITUDE_COUNT;
    obj->edges[(i + 1) * SPHERE_LONGITUDE_COUNT - 1 +
               SPHERE_LATITUDE_COUNT * SPHERE_LONGITUDE_COUNT][0] =
        i * SPHERE_LONGITUDE_COUNT + SPHERE_LONGITUDE_COUNT - 1;
    obj->edges[(i + 1) * SPHERE_LONGITUDE_COUNT - 1 +
               SPHERE_LATITUDE_COUNT * SPHERE_LONGITUDE_COUNT][1] =
        i * SPHERE_LONGITUDE_COUNT + SPHERE_LONGITUDE_COUNT - 2;
  }
  obj->vertex_count = SPHERE_LATITUDE_COUNT * SPHERE_LONGITUDE_COUNT;
  obj->edge_count = SPHERE_LATITUDE_COUNT * SPHERE_LONGITUDE_COUNT * 2;
  obj->offset = *offset;
  obj->rotation = *rotate;
}

static void draw_object(lv_layer_t *layer, render_data_t *data,
                        object3d_t *object) {
  vec3_t *rotated_vertices =
      heap_caps_malloc(object->vertex_count * sizeof(vec3_t), MALLOC_CAP_8BIT);
  lv_memcpy(rotated_vertices, object->vertices,
            object->vertex_count * sizeof(vec3_t));
  float *projected_x =
      heap_caps_malloc(object->vertex_count * sizeof(float), MALLOC_CAP_8BIT);
  float *projected_y =
      heap_caps_malloc(object->vertex_count * sizeof(float), MALLOC_CAP_8BIT);

  // 회전 및 위치 적용
  for (int i = 0; i < object->vertex_count; i++) {
    // 회전 적용
    rotate_vertex(&rotated_vertices[i], &object->rotation);

    // 위치(offset) 적용
    rotated_vertices[i].x += object->offset.x;
    rotated_vertices[i].y += object->offset.y;
    rotated_vertices[i].z += object->offset.z;

    // 투영
    project_vertex(rotated_vertices + i, &data->camera_pos, data->fov,
                   projected_x + i, projected_y + i);
  }

  lv_draw_line_dsc_t line_dsc;
  lv_draw_line_dsc_init(&line_dsc);
  line_dsc.color = lv_color_white();
  line_dsc.width = 1;
  // 와이어프레임 그리기
  for (int i = 0; i < object->edge_count; i++) {
    line_dsc.p1.x = projected_x[object->edges[i][0]];
    line_dsc.p1.y = projected_y[object->edges[i][0]];
    line_dsc.p2.x = projected_x[object->edges[i][1]];
    line_dsc.p2.y = projected_y[object->edges[i][1]];
    lv_draw_line(layer, &line_dsc);
  }

  heap_caps_free(rotated_vertices);
  heap_caps_free(projected_x);
  heap_caps_free(projected_y);
}

static void inline project_vertex(vec3_t *vertex, vec3_t *camera_pos, float fov,
                                  float *px, float *py) {
  float rel_x = vertex->x - camera_pos->x;
  float rel_y = vertex->y - camera_pos->y;
  float rel_z = vertex->z - camera_pos->z;

  if (rel_z < 0.0f) return;
  if (rel_z < 0.1f) rel_z = 0.1f;

  // float scale = (1.0f / tanf(fov * 0.5f * (M_PI / 180.0f))) / rel_z;
  // TODO: 뭔가 이상함 fov가 높을수록 물체가 작아져야 하는데 그 반대임
  float scale = fov / rel_z;

  *px = (LCD_WIDTH / 2) + (rel_x * scale);
  *py = (LCD_HEIGHT / 2) - (rel_y * scale);
}

static void inline rotate_vertex(vec3_t *vertex, vec3_t *rotation) {
  rotate_x(vertex, rotation->x);
  rotate_y(vertex, rotation->y);
  rotate_z(vertex, rotation->z);
}

// X축 회전
static void inline rotate_x(vec3_t *vertex, float angle) {
  float y = vertex->y * cos(angle) - vertex->z * sin(angle);
  float z = vertex->y * sin(angle) + vertex->z * cos(angle);
  vertex->y = y;
  vertex->z = z;
}

// Y축 회전
static void inline rotate_y(vec3_t *vertex, float angle) {
  float x = vertex->x * cos(angle) + vertex->z * sin(angle);
  float z = -vertex->x * sin(angle) + vertex->z * cos(angle);
  vertex->x = x;
  vertex->z = z;
}

// Z축 회전
static void inline rotate_z(vec3_t *vertex, float angle) {
  float x = vertex->x * cos(angle) - vertex->y * sin(angle);
  float y = vertex->x * sin(angle) + vertex->y * cos(angle);
  vertex->x = x;
  vertex->y = y;
}