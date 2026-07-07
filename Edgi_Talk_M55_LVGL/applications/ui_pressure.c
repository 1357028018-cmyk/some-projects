#include "ui_pressure.h"
#include "ui_main.h"
#include <rtthread.h>
#include "lvgl.h"
#include "lv_port_disp.h"
#include "airbag_control.h"
#include "test_airbag.h"

/* ========== 热力图尺寸参数 ==========
 * 屏幕分辨率: 512x800 (竖屏)
 * 16x16 网格, cell 22x14, gap 1
 * 总尺寸: 16*22 + 15*1 = 367 x 16*14 + 15*1 = 239
 */
#define GRID_DIV   16
#define CELL_W     22
#define CELL_H     14
#define GRID_GAP   1
#define GRID_BG_COLOR lv_color_hex(0x404040)

/* ========== 气囊卡片尺寸 ========== */
#define AIRBAG_CARD_W   210
#define AIRBAG_CARD_H   200
#define PILLOW_W        200
#define PILLOW_H        130
#define BAG_W           44
#define BAG_H           100
#define BAG_GAP         10

/* ========== 数据卡片尺寸 ========== */
#define DATA_CARD_W     210
#define DATA_CARD_H     200

extern uint8_t* pressure_get_data(void);
extern uint8_t pressure_is_ready(void);
extern void pressure_clear_ready(void);
extern const char *posture_get_majority_label(void);
extern int radar_get_heart_rate(void);
extern int radar_get_breath_rate(void);
extern int snore_get_detected(void);
extern float snore_get_confidence(void);

static lv_obj_t * g_heatmap_screen = NULL;
static lv_obj_t * g_heatmap_container = NULL;
static lv_obj_t * g_cells[16][16];
static uint8_t g_last_pressure_data[16 * 16];
static lv_timer_t * g_update_timer = NULL;

/* ========== 顶部 / 姿势 ========== */
static lv_obj_t * g_posture_label = NULL;
static lv_obj_t * g_posture_pill = NULL;

/* ========== 气囊状态UI对象 ========== */
static lv_obj_t * g_pillow_rect = NULL;
static lv_obj_t * g_airbag_rects[3];
static lv_obj_t * g_airbag_labels[3];
static lv_obj_t * g_state_label = NULL;

/* ========== 数据面板UI对象 ========== */
static lv_obj_t * g_data_panel = NULL;
static lv_obj_t * g_hr_label = NULL;
static lv_obj_t * g_br_label = NULL;
static lv_obj_t * g_snore_label = NULL;

/* ========== 状态 pill（stable 等） ========== */
static lv_obj_t * g_state_pill = NULL;

/* ========== 手动控制按钮 ========== */
static lv_obj_t * g_manual_btns[6] = {NULL};
static const char * g_manual_labels[6] = {
    "Inflate L", "Inflate M", "Inflate R",
    "Deflate L", "Deflate M", "Deflate R",
};

static void back_btn_event_handler(lv_event_t * e)
{
    (void)e;
    lv_obj_t * main_scr = ui_main_get_main_screen();
    if (main_scr != NULL)
    {
        lv_scr_load(main_scr);
    }
}

static void test_btn_event_handler(lv_event_t * e)
{
    (void)e;
    airbag_test_run();
}

static void inflate_all_btn_event_handler(lv_event_t * e)
{
    (void)e;
    airbag_inflate_all();
}

static void deflate_all_btn_event_handler(lv_event_t * e)
{
    (void)e;
    airbag_deflate_all();
}

static void manual_btn_event_handler(lv_event_t * e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    uint8_t bag = (uint8_t)(idx % 3);
    if (idx < 3) airbag_manual_inflate(bag);
    else         airbag_manual_deflate(bag);
}

static void update_heatmap(void)
{
    uint8_t * data = pressure_get_data();

    if (pressure_is_ready())
    {
        for (int i = 0; i < 16 * 16; i++)
        {
            g_last_pressure_data[i] = data[i];
        }
        pressure_clear_ready();
    }

    /* 90° 逆时针旋转: new[gx][gy] = original[gy][15-gx] */
    for (int gy = 0; gy < GRID_DIV; gy++)
    {
        for (int gx = 0; gx < GRID_DIV; gx++)
        {
            uint8_t val = g_last_pressure_data[gy * GRID_DIV + (15 - gx)];
            lv_color_t color = lv_color_make(val, 0, 255 - val);
            if (g_cells[gy][gx] != NULL)
            {
                lv_obj_set_style_bg_color(g_cells[gy][gx], color, 0);
                lv_obj_set_style_bg_opa(g_cells[gy][gx], LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(g_cells[gy][gx], 0, 0);
                lv_obj_set_style_outline_width(g_cells[gy][gx], 0, 0);
                lv_obj_set_style_shadow_width(g_cells[gy][gx], 0, 0);
            }
        }
    }
}

static lv_color_t airbag_color_for_sec(uint8_t sec)
{
    if (sec <= 0)
        return lv_color_hex(0x808080);
    else if (sec < 4)
        return lv_color_hex(0xFFD700);
    else if (sec < 8)
        return lv_color_hex(0xFF8C00);
    else
        return lv_color_hex(0x00C853);
}

static void update_airbag_icon(void)
{
    for (int i = 0; i < 3; i++) {
        uint8_t sec = airbag_get_sec(i);
        lv_color_t color = airbag_color_for_sec(sec);

        if (g_airbag_rects[i] != NULL) {
            lv_obj_set_style_bg_color(g_airbag_rects[i], color, 0);
            lv_obj_set_style_bg_opa(g_airbag_rects[i], LV_OPA_COVER, 0);
        }
        if (g_airbag_labels[i] != NULL) {
            char buf[16];
            const char *names[3] = {"L", "M", "R"};
            if (sec <= 0)
                rt_snprintf(buf, sizeof(buf), "%s\n--", names[i]);
            else
                rt_snprintf(buf, sizeof(buf), "%s\n%ds", names[i], sec);
            lv_label_set_text(g_airbag_labels[i], buf);
        }
    }

    if (g_state_label != NULL) {
        /* g_state_label 现在是容器, 子 label 是 text */
        lv_obj_t *lbl = lv_obj_get_child(g_state_label, 0);
        if (lbl != NULL) {
            lv_label_set_text(lbl, airbag_state_name());
        }
    }
}

static void update_posture_pill(void)
{
    if (g_posture_pill == NULL) return;
    const char *label = posture_get_majority_label();
    if (label == NULL) return;
    /* 子 label 才是文字 */
    lv_obj_t *lbl = lv_obj_get_child(g_posture_pill, 0);
    if (lbl == NULL) return;
    /* 根据姿势名显示状态: collecting / noise / stable */
    if (rt_strcmp(label, "collecting...") == 0) {
        lv_label_set_text(lbl, "init");
        lv_obj_set_style_bg_color(g_posture_pill, lv_color_hex(0xE0E7FF), 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x4338CA), 0);
    } else if (rt_strstr(label, "noise")) {
        lv_label_set_text(lbl, "noise");
        lv_obj_set_style_bg_color(g_posture_pill, lv_color_hex(0xFEF3C7), 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xB45309), 0);
    } else {
        lv_label_set_text(lbl, "stable");
        lv_obj_set_style_bg_color(g_posture_pill, lv_color_hex(0xE0F2FE), 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x0369A1), 0);
    }
}

/* 手动按钮高亮: 有 pending 或正在执行时变绿 */
static void update_manual_buttons(void)
{
    static int last_active = -1;
    int active = airbag_manual_is_active();
    if (active == last_active) return;
    last_active = active;
    lv_color_t color = active ? lv_color_hex(0x00C853) : lv_color_hex(0x2980B9);
    for (int i = 0; i < 6; i++) {
        if (g_manual_btns[i] != NULL)
            lv_obj_set_style_bg_color(g_manual_btns[i], color, 0);
    }
}

/* ========== 数据面板更新 ========== */
static void update_data_panel(void)
{
    int hr = radar_get_heart_rate();
    int br = radar_get_breath_rate();
    int snore = snore_get_detected();
    float conf = snore_get_confidence();

    if (g_hr_label) {
        char buf[16];
        if (hr > 0)
            rt_snprintf(buf, sizeof(buf), "%d", hr);
        else
            rt_snprintf(buf, sizeof(buf), "--");
        lv_label_set_text(g_hr_label, buf);
    }

    if (g_br_label) {
        char buf[16];
        if (br > 0)
            rt_snprintf(buf, sizeof(buf), "%d", br);
        else
            rt_snprintf(buf, sizeof(buf), "--");
        lv_label_set_text(g_br_label, buf);
    }

    if (g_snore_label) {
        char buf[32];
        if (snore) {
            rt_snprintf(buf, sizeof(buf), "YES %d%%", (int)(conf * 100.0f + 0.5f));
            lv_obj_set_style_text_color(g_snore_label, lv_color_hex(0xDC2626), 0);
        } else {
            rt_snprintf(buf, sizeof(buf), "NO");
            lv_obj_set_style_text_color(g_snore_label, lv_color_hex(0x1E293B), 0);
        }
        lv_label_set_text(g_snore_label, buf);
    }
}

static void update_timer_callback(lv_timer_t * timer)
{
    (void)timer;
    update_heatmap();
    if (g_posture_label != NULL) {
        lv_label_set_text(g_posture_label, posture_get_majority_label());
    }
    update_posture_pill();
    update_airbag_icon();
    update_data_panel();
    update_manual_buttons();
}

lv_obj_t * ui_pressure_get_screen(void)
{
    return g_heatmap_screen;
}

/* ========== 创建数据面板内的单行 (白底圆角 + label + value) ==========
 * mode: 0 = 普通数值, 1 = 带单位 (bpm/.../min)
 * row_idx: 0=第一行, 1=第二行, 2=第三行 (用于固定Y坐标)
 * 返回的 label 指向 value label, 以便 update_data_panel 更新 */
static lv_obj_t * create_data_row(lv_obj_t * parent, int row_idx,
                                  const char * label_text, const char * init_val,
                                  const char * unit, int mode)
{
    /* 行容器: 白底圆角, 固定Y坐标避免重叠 */
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 186, 36);
    int y = 22 + row_idx * 42;  /* 22, 64, 106 */
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, y);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* 左侧: 标签 */
    lv_obj_t * lbl = lv_label_create(row);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 10, 0);

    /* 右侧: 数值 */
    lv_obj_t * val = lv_label_create(row);
    lv_label_set_text(val, init_val);
    lv_obj_set_style_text_color(val, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_16, 0);
    if (mode == 1) {
        lv_obj_align(val, LV_ALIGN_RIGHT_MID, -58, 0);
    } else {
        lv_obj_align(val, LV_ALIGN_RIGHT_MID, -10, 0);
    }

    /* 单位 (mode=1) */
    if (mode == 1 && unit != NULL) {
        lv_obj_t * u = lv_label_create(row);
        lv_label_set_text(u, unit);
        lv_obj_set_style_text_color(u, lv_color_hex(0x94A3B8), 0);
        lv_obj_set_style_text_font(u, &lv_font_montserrat_10, 0);
        lv_obj_align(u, LV_ALIGN_RIGHT_MID, -10, 0);
    }

    return val;
}

void ui_pressure_init(void)
{
    rt_kprintf("[UI] ui_pressure_init start\r\n");

    g_heatmap_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_heatmap_screen, lv_color_hex(0xF0F4F8), 0);
    lv_obj_set_style_bg_opa(g_heatmap_screen, LV_OPA_COVER, 0);

    /* ========== Header: Back 按钮 + 标题 ========== */
    {
        lv_obj_t *back_btn = lv_button_create(g_heatmap_screen);
        lv_obj_remove_style_all(back_btn);
        lv_obj_set_size(back_btn, 70, 36);
        lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 16, 12);
        lv_obj_set_style_bg_color(back_btn, lv_color_hex(0xE74C3C), 0);
        lv_obj_set_style_bg_opa(back_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(back_btn, 8, 0);
        lv_obj_set_style_pad_all(back_btn, 0, 0);

        lv_obj_t *back_btn_label = lv_label_create(back_btn);
        lv_label_set_text(back_btn_label, LV_SYMBOL_LEFT " Back");
        lv_obj_set_style_text_color(back_btn_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(back_btn_label, &lv_font_montserrat_14, 0);
        lv_obj_center(back_btn_label);

        lv_obj_add_event_cb(back_btn, back_btn_event_handler, LV_EVENT_CLICKED, NULL);

        lv_obj_t *title = lv_label_create(g_heatmap_screen);
        lv_label_set_text(title, "Health Monitor");
        lv_obj_set_style_text_color(title, lv_color_hex(0x1E293B), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);
    }

    /* ========== Posture 行 (POSTURE 标签 + 姿势名 + 状态 pill) ========== */
    {
        lv_obj_t *posture_label_tag = lv_label_create(g_heatmap_screen);
        lv_label_set_text(posture_label_tag, "POSTURE");
        lv_obj_set_style_text_color(posture_label_tag, lv_color_hex(0x94A3B8), 0);
        lv_obj_set_style_text_font(posture_label_tag, &lv_font_montserrat_12, 0);
        lv_obj_align(posture_label_tag, LV_ALIGN_TOP_LEFT, 60, 62);

        g_posture_label = lv_label_create(g_heatmap_screen);
        lv_label_set_text(g_posture_label, "collecting...");
        lv_obj_set_style_text_color(g_posture_label, lv_color_hex(0x1E293B), 0);
        lv_obj_set_style_text_font(g_posture_label, &lv_font_montserrat_22, 0);
        lv_obj_align(g_posture_label, LV_ALIGN_TOP_MID, 0, 56);

        g_posture_pill = lv_obj_create(g_heatmap_screen);
        lv_obj_remove_style_all(g_posture_pill);
        lv_obj_set_size(g_posture_pill, 60, 22);
        lv_obj_align(g_posture_pill, LV_ALIGN_TOP_RIGHT, -60, 60);
        lv_obj_set_style_radius(g_posture_pill, 11, 0);
        lv_obj_set_style_bg_color(g_posture_pill, lv_color_hex(0xE0F2FE), 0);
        lv_obj_set_style_bg_opa(g_posture_pill, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(g_posture_pill, 0, 0);

        lv_obj_t *pill_lbl = lv_label_create(g_posture_pill);
        lv_label_set_text(pill_lbl, "init");
        lv_obj_set_style_text_color(pill_lbl, lv_color_hex(0x0369A1), 0);
        lv_obj_set_style_text_font(pill_lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(pill_lbl);
    }

    /* ========== 热力图 16x16 ========== */
    {
        int heatmap_w = GRID_DIV * CELL_W + (GRID_DIV - 1) * GRID_GAP;
        int heatmap_h = GRID_DIV * CELL_H + (GRID_DIV - 1) * GRID_GAP;
        g_heatmap_container = lv_obj_create(g_heatmap_screen);
        lv_obj_set_size(g_heatmap_container, heatmap_w, heatmap_h);
        lv_obj_align(g_heatmap_container, LV_ALIGN_TOP_MID, 0, 100);
        lv_obj_set_style_bg_color(g_heatmap_container, GRID_BG_COLOR, 0);
        lv_obj_set_style_bg_opa(g_heatmap_container, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(g_heatmap_container, 0, 0);
        lv_obj_set_style_pad_all(g_heatmap_container, 0, 0);
        lv_obj_set_style_pad_gap(g_heatmap_container, GRID_GAP, 0);
        lv_obj_set_style_layout(g_heatmap_container, LV_LAYOUT_GRID, 0);

        static lv_coord_t col_dsc[GRID_DIV + 1];
        static lv_coord_t row_dsc[GRID_DIV + 1];
        for (int i = 0; i < GRID_DIV; i++)
        {
            col_dsc[i] = CELL_W;
            row_dsc[i] = CELL_H;
        }
        col_dsc[GRID_DIV] = LV_GRID_TEMPLATE_LAST;
        row_dsc[GRID_DIV] = LV_GRID_TEMPLATE_LAST;
        lv_obj_set_style_grid_column_dsc_array(g_heatmap_container, col_dsc, 0);
        lv_obj_set_style_grid_row_dsc_array(g_heatmap_container, row_dsc, 0);

        for (int gy = 0; gy < GRID_DIV; gy++)
        {
            for (int gx = 0; gx < GRID_DIV; gx++)
            {
                g_cells[gy][gx] = lv_obj_create(g_heatmap_container);
                lv_obj_set_size(g_cells[gy][gx], CELL_W, CELL_H);
                lv_obj_set_style_bg_color(g_cells[gy][gx], lv_color_hex(0x000000), 0);
                lv_obj_set_style_bg_opa(g_cells[gy][gx], LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(g_cells[gy][gx], 0, 0);
                lv_obj_set_style_radius(g_cells[gy][gx], 0, 0);
                lv_obj_set_style_outline_width(g_cells[gy][gx], 0, 0);
                lv_obj_set_style_shadow_width(g_cells[gy][gx], 0, 0);
                lv_obj_set_grid_cell(g_cells[gy][gx], LV_GRID_ALIGN_STRETCH, gx, 1,
                                     LV_GRID_ALIGN_STRETCH, gy, 1);
            }
        }
    }

    /* ========== 底部: 气囊卡片 + 数据卡片 (左右两栏) ========== */
    int bottom_y = 360;
    int bottom_h = 200;
    {
        /* 气囊卡片 */
        lv_obj_t *airbag_card = lv_obj_create(g_heatmap_screen);
        lv_obj_remove_style_all(airbag_card);
        lv_obj_set_size(airbag_card, AIRBAG_CARD_W, AIRBAG_CARD_H);
        lv_obj_align(airbag_card, LV_ALIGN_TOP_LEFT, 16, bottom_y);
        lv_obj_set_style_radius(airbag_card, 14, 0);
        lv_obj_set_style_bg_color(airbag_card, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(airbag_card, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(airbag_card, 4, 0);
        lv_obj_set_style_shadow_opa(airbag_card, LV_OPA_10, 0);
        lv_obj_set_style_pad_all(airbag_card, 0, 0);

        /* 卡片标题 */
        lv_obj_t *airbag_title = lv_label_create(airbag_card);
        lv_label_set_text(airbag_title, "AIRBAG STATUS");
        lv_obj_set_style_text_color(airbag_title, lv_color_hex(0x94A3B8), 0);
        lv_obj_set_style_text_font(airbag_title, &lv_font_montserrat_12, 0);
        lv_obj_align(airbag_title, LV_ALIGN_TOP_MID, 0, 10);

        /* 枕头外框 */
        g_pillow_rect = lv_obj_create(airbag_card);
        lv_obj_remove_style_all(g_pillow_rect);
        lv_obj_set_size(g_pillow_rect, PILLOW_W, PILLOW_H);
        lv_obj_align(g_pillow_rect, LV_ALIGN_TOP_MID, 0, 32);
        lv_obj_set_style_radius(g_pillow_rect, 14, 0);
        lv_obj_set_style_bg_color(g_pillow_rect, lv_color_hex(0xECEFF1), 0);
        lv_obj_set_style_bg_opa(g_pillow_rect, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(g_pillow_rect, lv_color_hex(0xB0BEC5), 0);
        lv_obj_set_style_border_width(g_pillow_rect, 2, 0);
        lv_obj_set_style_pad_all(g_pillow_rect, 0, 0);
        lv_obj_clear_flag(g_pillow_rect, LV_OBJ_FLAG_SCROLLABLE);

        /* 三个气囊 */
        int bag_total_w = 3 * BAG_W + 2 * BAG_GAP;
        int bag_start_x = (PILLOW_W - bag_total_w) / 2;
        int bag_start_y = (PILLOW_H - BAG_H) / 2;
        for (int i = 0; i < 3; i++) {
            g_airbag_rects[i] = lv_obj_create(g_pillow_rect);
            lv_obj_remove_style_all(g_airbag_rects[i]);
            lv_obj_set_size(g_airbag_rects[i], BAG_W, BAG_H);
            lv_obj_align(g_airbag_rects[i], LV_ALIGN_TOP_LEFT,
                         bag_start_x + i * (BAG_W + BAG_GAP), bag_start_y);
            lv_obj_set_style_radius(g_airbag_rects[i], 8, 0);
            lv_obj_set_style_bg_color(g_airbag_rects[i], lv_color_hex(0x9CA3AF), 0);
            lv_obj_set_style_bg_opa(g_airbag_rects[i], LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(g_airbag_rects[i], lv_color_hex(0x475569), 0);
            lv_obj_set_style_border_width(g_airbag_rects[i], 1, 0);
            lv_obj_clear_flag(g_airbag_rects[i], LV_OBJ_FLAG_SCROLLABLE);

            g_airbag_labels[i] = lv_label_create(g_airbag_rects[i]);
            lv_label_set_text(g_airbag_labels[i], "L\n--");
            lv_obj_set_style_text_color(g_airbag_labels[i], lv_color_hex(0x1E293B), 0);
            lv_obj_set_style_text_font(g_airbag_labels[i], &lv_font_montserrat_14, 0);
            lv_obj_align(g_airbag_labels[i], LV_ALIGN_CENTER, 0, 0);
        }

        /* State 行 */
        lv_obj_t *state_text = lv_label_create(airbag_card);
        lv_label_set_text(state_text, "State:");
        lv_obj_set_style_text_color(state_text, lv_color_hex(0x475569), 0);
        lv_obj_set_style_text_font(state_text, &lv_font_montserrat_12, 0);
        lv_obj_align(state_text, LV_ALIGN_BOTTOM_MID, -60, -12);

        g_state_label = lv_obj_create(airbag_card);
        lv_obj_remove_style_all(g_state_label);
        lv_obj_set_size(g_state_label, 130, 22);
        lv_obj_align(g_state_label, LV_ALIGN_BOTTOM_MID, 30, -12);
        lv_obj_set_style_radius(g_state_label, 6, 0);
        lv_obj_set_style_bg_color(g_state_label, lv_color_hex(0x1E293B), 0);
        lv_obj_set_style_bg_opa(g_state_label, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(g_state_label, 0, 0);

        lv_obj_t *state_pill_lbl = lv_label_create(g_state_label);
        lv_label_set_text(state_pill_lbl, "IDLE");
        lv_obj_set_style_text_color(state_pill_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(state_pill_lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(state_pill_lbl);
    }

    /* ========== 数据卡片 ========== */
    {
        g_data_panel = lv_obj_create(g_heatmap_screen);
        lv_obj_remove_style_all(g_data_panel);
        lv_obj_set_size(g_data_panel, DATA_CARD_W, DATA_CARD_H);
        lv_obj_align(g_data_panel, LV_ALIGN_TOP_LEFT, 234, bottom_y);
        lv_obj_set_style_radius(g_data_panel, 14, 0);
        lv_obj_set_style_bg_color(g_data_panel, lv_color_hex(0xE8F0FE), 0);
        lv_obj_set_style_bg_opa(g_data_panel, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(g_data_panel, 4, 0);
        lv_obj_set_style_shadow_opa(g_data_panel, LV_OPA_10, 0);
        lv_obj_set_style_pad_all(g_data_panel, 12, 0);
        lv_obj_clear_flag(g_data_panel, LV_OBJ_FLAG_SCROLLABLE);

        /* 标题 */
        lv_obj_t *data_title = lv_label_create(g_data_panel);
        lv_label_set_text(data_title, "VITAL SIGNS");
        lv_obj_set_style_text_color(data_title, lv_color_hex(0x94A3B8), 0);
        lv_obj_set_style_text_font(data_title, &lv_font_montserrat_12, 0);
        lv_obj_align(data_title, LV_ALIGN_TOP_MID, 0, 0);

        /* HR 行 */
        g_hr_label = create_data_row(g_data_panel, 0, "Heart Rate", "--", "bpm", 1);
        /* BR 行 */
        g_br_label = create_data_row(g_data_panel, 1, "Breath Rate", "--", "/min", 1);
        /* Snore 行 */
        g_snore_label = create_data_row(g_data_panel, 2, "Snore", "NO", NULL, 0);
    }

    /* ========== 底部: 6 个手动按钮 + 图例 + Test 按钮 ========== */
    {
        /* 6 个手动控制按钮: Inflate L/M/R (row 1), Deflate L/M/R (row 2) */
        const int btn_w = 130;
        const int btn_h = 34;
        const int btn_gap = 8;
        const int total_w = btn_w * 3 + btn_gap * 2;
        const int start_x = (512 - total_w) / 2;
        const int row1_y = 580;
        const int row2_y = row1_y + btn_h + 6;

        for (int i = 0; i < 6; i++) {
            int row = i / 3;
            int col = i % 3;
            int x = start_x + col * (btn_w + btn_gap);
            int y = (row == 0) ? row1_y : row2_y;

            g_manual_btns[i] = lv_button_create(g_heatmap_screen);
            lv_obj_remove_style_all(g_manual_btns[i]);
            lv_obj_set_size(g_manual_btns[i], btn_w, btn_h);
            lv_obj_set_pos(g_manual_btns[i], x, y);
            lv_obj_set_style_radius(g_manual_btns[i], 6, 0);
            lv_obj_set_style_bg_color(g_manual_btns[i], lv_color_hex(0x2980B9), 0);
            lv_obj_set_style_bg_opa(g_manual_btns[i], LV_OPA_COVER, 0);
            lv_obj_set_style_pad_all(g_manual_btns[i], 0, 0);

            lv_obj_t *lbl = lv_label_create(g_manual_btns[i]);
            lv_label_set_text(lbl, g_manual_labels[i]);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
            lv_obj_center(lbl);

            lv_obj_add_event_cb(g_manual_btns[i], manual_btn_event_handler,
                                LV_EVENT_CLICKED, (void *)(intptr_t)i);
        }

        /* 图例 */
        lv_obj_t *legend = lv_label_create(g_heatmap_screen);
        lv_label_set_text(legend,
            "0s  <5s  <9s  9s");
        lv_obj_set_style_text_color(legend, lv_color_hex(0x64748B), 0);
        lv_obj_set_style_text_font(legend, &lv_font_montserrat_10, 0);
        lv_obj_align(legend, LV_ALIGN_BOTTOM_MID, 0, -52);

        /* Inflate All 按钮 (Test 左侧) */
        lv_obj_t *inflate_btn = lv_button_create(g_heatmap_screen);
        lv_obj_remove_style_all(inflate_btn);
        lv_obj_set_size(inflate_btn, 90, 32);
        lv_obj_align(inflate_btn, LV_ALIGN_BOTTOM_MID, -100, -16);
        lv_obj_set_style_radius(inflate_btn, 8, 0);
        lv_obj_set_style_bg_color(inflate_btn, lv_color_hex(0x27AE60), 0);
        lv_obj_set_style_bg_opa(inflate_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(inflate_btn, 0, 0);

        lv_obj_t *inflate_btn_label = lv_label_create(inflate_btn);
        lv_label_set_text(inflate_btn_label, LV_SYMBOL_PLAY " INFLATE");
        lv_obj_set_style_text_color(inflate_btn_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(inflate_btn_label, &lv_font_montserrat_12, 0);
        lv_obj_center(inflate_btn_label);

        lv_obj_add_event_cb(inflate_btn, inflate_all_btn_event_handler, LV_EVENT_CLICKED, NULL);

        /* Test 按钮 */
        lv_obj_t *test_btn = lv_button_create(g_heatmap_screen);
        lv_obj_remove_style_all(test_btn);
        lv_obj_set_size(test_btn, 90, 32);
        lv_obj_align(test_btn, LV_ALIGN_BOTTOM_MID, 0, -16);
        lv_obj_set_style_radius(test_btn, 8, 0);
        lv_obj_set_style_bg_color(test_btn, lv_color_hex(0x2980B9), 0);
        lv_obj_set_style_bg_opa(test_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(test_btn, 0, 0);

        lv_obj_t *test_btn_label = lv_label_create(test_btn);
        lv_label_set_text(test_btn_label, LV_SYMBOL_PLAY " TEST");
        lv_obj_set_style_text_color(test_btn_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(test_btn_label, &lv_font_montserrat_12, 0);
        lv_obj_center(test_btn_label);

        lv_obj_add_event_cb(test_btn, test_btn_event_handler, LV_EVENT_CLICKED, NULL);

        /* Deflate All 按钮 (Test 右侧) */
        lv_obj_t *deflate_btn = lv_button_create(g_heatmap_screen);
        lv_obj_remove_style_all(deflate_btn);
        lv_obj_set_size(deflate_btn, 90, 32);
        lv_obj_align(deflate_btn, LV_ALIGN_BOTTOM_MID, 100, -16);
        lv_obj_set_style_radius(deflate_btn, 8, 0);
        lv_obj_set_style_bg_color(deflate_btn, lv_color_hex(0xC0392B), 0);
        lv_obj_set_style_bg_opa(deflate_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(deflate_btn, 0, 0);

        lv_obj_t *deflate_btn_label = lv_label_create(deflate_btn);
        lv_label_set_text(deflate_btn_label, LV_SYMBOL_STOP " DEFLATE");
        lv_obj_set_style_text_color(deflate_btn_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(deflate_btn_label, &lv_font_montserrat_12, 0);
        lv_obj_center(deflate_btn_label);

        lv_obj_add_event_cb(deflate_btn, deflate_all_btn_event_handler, LV_EVENT_CLICKED, NULL);
    }

    for (int i = 0; i < 16 * 16; i++)
    {
        g_last_pressure_data[i] = 0;
    }

    update_heatmap();
    update_posture_pill();
    update_airbag_icon();
    update_data_panel();

    g_update_timer = lv_timer_create(update_timer_callback, 100, NULL);

    rt_kprintf("[UI] ui_pressure_init done\r\n");
}
