#ifndef UI_MAIN_H
#define UI_MAIN_H

#include <lvgl.h>
#include <rtthread.h>

lv_obj_t * ui_main_get_main_screen(void);
void ui_main_init(void);
void ui_main_wifi_init(lv_obj_t *parent);

rt_err_t sensor_get_data(float *temperature, float *humidity);
rt_uint8_t sensor_is_valid(void);

#endif
