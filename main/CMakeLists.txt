idf_component_register(SRCS "main.c" "lcd.c" "render.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_lcd lvgl)