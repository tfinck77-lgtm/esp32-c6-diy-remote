#pragma once
#include <lvgl.h>
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Akkustand-Anzeige: 3 vertikale Segmente oben rechts, OHNE Rahmen.
// Bei < 10 % werden alle 3 Segmente rot (kritisch).
//
// Schwellen (bei Bedarf anpassen):
//   >= 60 %  -> 3 gruene Segmente
//   30-59 %  -> 2 gruene Segmente
//   10-29 %  -> 1 gruenes Segment
//   < 10 %   -> alle 3 Segmente rot
// ---------------------------------------------------------------------------

#define BAT_ADC_PIN     0      // GPIO0 (BAT_ADC)
#define BAT_DIV_RATIO   3.0f   // (200k + 100k) / 100k

// --- Akku-Prozent aus Spannung (typische Li-Ion Entladekurve, interpoliert) ---
struct BatPoint { float voltage; uint8_t percent; };

static const BatPoint battCurve[] = {
    {4.20f, 100}, {4.06f, 90}, {3.98f, 80}, {3.92f, 70},
    {3.87f, 60},  {3.82f, 50}, {3.79f, 40}, {3.77f, 30},
    {3.74f, 20},  {3.68f, 10}, {3.45f, 5},  {3.00f, 0}
};

inline uint8_t battery_read_percent() {
    uint32_t sum_mv = 0;
    const int samples = 8;
    for (int i = 0; i < samples; i++) {
        sum_mv += analogReadMilliVolts(BAT_ADC_PIN);
    }
    float vbat = (sum_mv / (float)samples / 1000.0f) * BAT_DIV_RATIO;

    const int n = sizeof(battCurve) / sizeof(battCurve[0]);
    if (vbat >= battCurve[0].voltage) return 100;
    if (vbat <= battCurve[n - 1].voltage) return 0;

    for (int i = 0; i < n - 1; i++) {
        if (vbat <= battCurve[i].voltage && vbat >= battCurve[i + 1].voltage) {
            float ratio = (vbat - battCurve[i + 1].voltage) /
                          (battCurve[i].voltage - battCurve[i + 1].voltage);
            return battCurve[i + 1].percent +
                   (uint8_t)(ratio * (battCurve[i].percent - battCurve[i + 1].percent));
        }
    }
    return 0;
}

// --- LVGL-Widget: 3 Segmente, ohne Rahmen ---
#define BATT_SEG_W   12   // Breite pro Segment
#define BATT_SEG_H   6    // Hoehe pro Segment
#define BATT_SEG_GAP 2    // Abstand zwischen den Segmenten
// Gesamthoehe: 3 * 6 + 2 * 2 = 22 px

static lv_obj_t * batt_seg[3];

inline void battery_icon_update(uint8_t percent);

inline void battery_icon_create(lv_obj_t * parent) {
    // Ein unsichtbarer Container, der die 3 Segmente haelt.
    // Damit koennen wir die Position zentral steuern.
    // Container wird nicht gezeichnet, nur als Anker benutzt.
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_size(cont, BATT_SEG_W,
                    3 * BATT_SEG_H + 2 * BATT_SEG_GAP);
    lv_obj_align(cont, LV_ALIGN_TOP_RIGHT, -2, 2);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_pad_gap(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 3; i++) {
        batt_seg[i] = lv_obj_create(cont);
        lv_obj_set_size(batt_seg[i], BATT_SEG_W, BATT_SEG_H);
        // i = 0 -> oberstes Segment (y=0)
        // i = 2 -> unterstes Segment (y=16)
        lv_obj_set_pos(batt_seg[i], 0, i * (BATT_SEG_H + BATT_SEG_GAP));
        lv_obj_set_style_border_width(batt_seg[i], 0, 0);
        lv_obj_set_style_radius(batt_seg[i], 1, 0);
        lv_obj_set_style_bg_color(batt_seg[i], lv_color_black(), 0);
        lv_obj_set_style_bg_opa(batt_seg[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(batt_seg[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    battery_icon_update(battery_read_percent());
}

inline void battery_icon_update(uint8_t percent) {
    lv_color_t green = lv_palette_main(LV_PALETTE_GREEN);
    lv_color_t red   = lv_palette_main(LV_PALETTE_RED);

    if (percent < 10) {
        // Kritisch: alle 3 Segmente rot.
        for (int i = 0; i < 3; i++) {
            lv_obj_set_style_bg_color(batt_seg[i], red, 0);
        }
        return;
    }

    int lit;
    if (percent >= 60)      lit = 3;
    else if (percent >= 30) lit = 2;
    else                    lit = 1;

    // Fuellt von unten. batt_seg[2] ist unten, batt_seg[0] oben.
    for (int i = 0; i < 3; i++) {
        int from_bottom = 2 - i;
        bool on = (from_bottom < lit);
        lv_obj_set_style_bg_color(batt_seg[i],
                                  on ? green : lv_color_black(), 0);
    }
}

inline void battery_icon_task_maybe(uint32_t intervall_ms = 30000) {
    static uint32_t last_check = 0;
    uint32_t now = millis();
    if (now - last_check >= intervall_ms || last_check == 0) {
        last_check = now;
        battery_icon_update(battery_read_percent());
    }
}