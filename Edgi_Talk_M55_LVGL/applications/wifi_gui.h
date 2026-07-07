#ifndef WIFI_GUI_H
#define WIFI_GUI_H

#include <lvgl.h>
#include <rtthread.h>

void wifi_gui_set_status_label(lv_obj_t *label);
void wifi_gui_init(lv_obj_t *parent);
void wifi_gui_show_settings(void);
void wifi_gui_update_status(void);

#endif
