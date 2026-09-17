#include "ui_remote.h"
#include "lvgl.h"
#include <stdio.h>

LV_FONT_DECLARE(font_de_14);

// ---------------------------------------------------------------------
// Hauptnavigation
// ---------------------------------------------------------------------

#define SCREEN_W 170
#define SCREEN_H 320
#define HEADER_H 42

#define DEVICE_COLS 2
#define DEVICE_TILE_W 85
#define DEVICE_TILE_H 80
#define DEVICE_TILE_GAP 3

// ---------------------------------------------------------------------
// RGB-Lampe: 4 Spalten x 6 Zeilen = 24 Kacheln, wie auf der Original-
// Fernbedienung. Durch den festen Kopfbereich mit Zurueck-Taste bleiben
// fuer die Befehle 170x278 px als scrollbarer Bereich.
// ---------------------------------------------------------------------

#define REMOTE_COLS 4
#define REMOTE_ROWS 6

#define TILE_W 85
#define TILE_H 80
#define TILE_GAP 3

typedef enum {
  TILE_COLOR,
  TILE_FUNCTION
} tile_type_t;

typedef struct {
  const char *name;     // interner Name, fuer Log + IR-Zuordnung
  const char *caption;  // sichtbarer Text auf der Kachel
  tile_type_t type;
  uint8_t r, g, b;      // nur bei TILE_COLOR relevant
  uint8_t light_bg;     // 1 = heller Hintergrund -> schwarzer Text
  const char *icon;     // nur bei TILE_FUNCTION relevant (LV_SYMBOL_...)
} tile_def_t;

typedef enum {
  DEVICE_LAMP = 0,
  DEVICE_TV,
  DEVICE_INTERNETRADIO,
  DEVICE_LED_CANDLES,
  DEVICE_NIXIE_CLOCK,
  DEVICE_SOUNDBAR,
  DEVICE_BLURAY,
  DEVICE_COUNT
} device_id_t;

typedef struct {
  const char *caption;
} device_def_t;

static const device_def_t devices[DEVICE_COUNT] = {
  { "Lampe" },
  { "TV" },
  { "Internetradio" },
  { "LED-Kerzen" },
  { "Nixietube-Uhr" },
  { "Soundbar" },
  { "Bluray-Player" },
};

// Reihenfolge exakt wie auf der Original-Fernbedienung angegeben.
static const tile_def_t tiles[REMOTE_TILE_COUNT] = {
  // Reihe 1 - Funktionen
  { "heller",               "Heller",             TILE_FUNCTION, 0,0,0,          0, LV_SYMBOL_PLUS },
  { "dunkler",              "Dunkler",            TILE_FUNCTION, 0,0,0,          0, LV_SYMBOL_MINUS },
  { "aus",                  "Aus",                TILE_FUNCTION, 0,0,0,          0, LV_SYMBOL_POWER },
  { "an",                   "An",                 TILE_FUNCTION, 0,0,0,          0, LV_SYMBOL_POWER },

  // Reihe 2
  { "rot",                  "Rot",                TILE_COLOR,    0xE0,0x10,0x10, 0, NULL },
  { "gruen",                "Grün",               TILE_COLOR,    0x10,0xC0,0x20, 0, NULL },
  { "dunkelblau",           "Dkl. Blau",          TILE_COLOR,    0x00,0x10,0x90, 0, NULL },
  { "weiss",                "Weiß",               TILE_COLOR,    0xFF,0xFF,0xFF, 1, NULL },

  // Reihe 3
  { "dunkelorange",         "Dkl. Orange",        TILE_COLOR,    0xC0,0x50,0x00, 0, NULL },
  { "tuerkis",              "Türkis",             TILE_COLOR,    0x00,0xB0,0xA0, 0, NULL },
  { "lila",                 "Lila",               TILE_COLOR,    0x80,0x10,0xD0, 0, NULL },
  { "farbwechsel_schnell",  "Wechsel\nschnell",  TILE_FUNCTION, 0,0,0,          0, LV_SYMBOL_SHUFFLE },

  // Reihe 4
  { "gelborange",           "Gelb-Orange",        TILE_COLOR,    0xFF,0xA0,0x00, 1, NULL },
  { "dunkeltuerkis",        "Dkl. Türkis",        TILE_COLOR,    0x00,0x70,0x70, 0, NULL },
  { "hellilila",            "Hell. Lila",         TILE_COLOR,    0xC0,0x80,0xFF, 0, NULL },
  { "farbwechsel_langsam",  "Wechsel\nlangsam",  TILE_FUNCTION, 0,0,0,          0, LV_SYMBOL_SHUFFLE },

  // Reihe 5
  { "gelb",                 "Gelb",               TILE_COLOR,    0xFF,0xF0,0x00, 1, NULL },
  { "blau",                 "Blau",               TILE_COLOR,    0x20,0x40,0xFF, 0, NULL },
  { "rosa",                 "Rosa",               TILE_COLOR,    0xFF,0x40,0x90, 0, NULL },
  { "farbuebergang_schnell", "Übergang\nschnell", TILE_FUNCTION, 0,0,0,          0, LV_SYMBOL_LOOP },

  // Reihe 6
  { "hellgelb",             "Hell. Gelb",         TILE_COLOR,    0xFF,0xFF,0x99, 1, NULL },
  { "hellblau",             "Hell. Blau",         TILE_COLOR,    0x99,0xC0,0xFF, 1, NULL },
  { "hellrosa",             "Hell. Rosa",         TILE_COLOR,    0xFF,0xC0,0xD8, 1, NULL },
  { "farbuebergang_langsam", "Übergang\nlangsam", TILE_FUNCTION, 0,0,0,          0, LV_SYMBOL_LOOP },
};

// Wird in ir_bsp.cpp durch die echte Implementierung ueberschrieben.
__attribute__((weak)) void ir_send_command(uint8_t index)
{
  if (index < REMOTE_TILE_COUNT) {
    printf("[IR] Kachel gedrueckt: %s (Index %u) - IR-Code noch nicht hinterlegt\n",
           tiles[index].name, (unsigned)index);
  }
}

static void ui_show_device_menu(void);
static void ui_show_lamp_remote(void);
static void ui_show_placeholder_remote(device_id_t device);

static void clear_screen(void)
{
  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *create_header(const char *title)
{
  lv_obj_t *scr = lv_scr_act();

  lv_obj_t *header = lv_obj_create(scr);
  lv_obj_set_size(header, SCREEN_W, HEADER_H);
  lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(header, lv_color_make(0x20, 0x20, 0x26), 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *back = lv_btn_create(header);
  lv_obj_set_size(back, 42, 34);
  lv_obj_align(back, LV_ALIGN_LEFT_MID, 4, 0);
  lv_obj_set_style_radius(back, 7, 0);
  lv_obj_set_style_bg_color(back, lv_color_make(0x38, 0x38, 0x42), 0);
  lv_obj_set_style_shadow_width(back, 0, 0);

  lv_obj_t *back_label = lv_label_create(back);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_center(back_label);

  lv_obj_t *title_label = lv_label_create(header);
  lv_label_set_text(title_label, title);
  lv_obj_set_style_text_font(title_label, &font_de_14, 0);
  lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
  lv_obj_set_width(title_label, 116);
  lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(title_label, LV_ALIGN_RIGHT_MID, -4, 0);

  return back;
}

static void back_event_cb(lv_event_t *e)
{
  (void)e;
  ui_show_device_menu();
}

static void confirm_flash_timer_cb(lv_timer_t *timer)
{
  lv_obj_t *tile = (lv_obj_t *)timer->user_data;
  lv_obj_clear_state(tile, LV_STATE_USER_1);
}

static void tile_event_cb(lv_event_t *e)
{
  uint32_t index = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
  lv_obj_t *tile = lv_event_get_target(e);

  ir_send_command((uint8_t)index);

  lv_obj_add_state(tile, LV_STATE_USER_1);
  lv_timer_t *flash_timer = lv_timer_create(confirm_flash_timer_cb, 150, tile);
  lv_timer_set_repeat_count(flash_timer, 1);
}

static void device_event_cb(lv_event_t *e)
{
  device_id_t device = (device_id_t)(uintptr_t)lv_event_get_user_data(e);

  if (device == DEVICE_LAMP) {
    ui_show_lamp_remote();
  } else {
    ui_show_placeholder_remote(device);
  }
}

static void ui_show_device_menu(void)
{
  clear_screen();
  lv_obj_t *scr = lv_scr_act();

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Geräte");
  lv_obj_set_style_text_font(title, &font_de_14, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

  lv_obj_t *cont = lv_obj_create(scr);
  lv_obj_set_size(cont, SCREEN_W, SCREEN_H - 32);
  lv_obj_align(cont, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(cont, 0, 0);
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_set_scroll_dir(cont, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);

  for (uint32_t i = 0; i < DEVICE_COUNT; i++) {
    uint32_t col = i % DEVICE_COLS;
    uint32_t row = i / DEVICE_COLS;

    lv_obj_t *tile = lv_btn_create(cont);
    lv_obj_set_size(tile, DEVICE_TILE_W - DEVICE_TILE_GAP,
                          DEVICE_TILE_H - DEVICE_TILE_GAP);
    lv_obj_set_pos(tile,
                   col * DEVICE_TILE_W + DEVICE_TILE_GAP / 2,
                   row * DEVICE_TILE_H + DEVICE_TILE_GAP / 2);
    lv_obj_set_style_radius(tile, 8, 0);
    lv_obj_set_style_bg_color(tile, lv_color_make(0x30, 0x30, 0x38), 0);
    lv_obj_set_style_shadow_width(tile, 0, 0);

    lv_obj_t *label = lv_label_create(tile);
    lv_label_set_text(label, devices[i].caption);
    lv_obj_set_style_text_font(label, &font_de_14, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_width(label, DEVICE_TILE_W - 16);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(tile, device_event_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)i);
  }
}

static void ui_show_lamp_remote(void)
{
  clear_screen();

  lv_obj_t *back = create_header("Lampe");
  lv_obj_add_event_cb(back, back_event_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *scr = lv_scr_act();
  lv_obj_t *cont = lv_obj_create(scr);
  lv_obj_set_size(cont, SCREEN_W, SCREEN_H - HEADER_H);
  lv_obj_set_pos(cont, 0, HEADER_H);
  lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(cont, 0, 0);
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_set_scroll_dir(cont, LV_DIR_ALL);
  lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);

  static lv_style_t style_confirm;
  static bool style_confirm_initialized = false;
  if (!style_confirm_initialized) {
    lv_style_init(&style_confirm);
    lv_style_set_bg_opa(&style_confirm, LV_OPA_60);
    lv_style_set_border_width(&style_confirm, 3);
    lv_style_set_border_color(&style_confirm, lv_color_white());
    lv_style_set_border_opa(&style_confirm, LV_OPA_COVER);
    style_confirm_initialized = true;
  }

  for (uint32_t i = 0; i < REMOTE_TILE_COUNT; i++) {
    uint32_t col = i % REMOTE_COLS;
    uint32_t row = i / REMOTE_COLS;
    const tile_def_t *t = &tiles[i];

    lv_obj_t *tile = lv_obj_create(cont);
    lv_obj_set_size(tile, TILE_W - TILE_GAP, TILE_H - TILE_GAP);
    lv_obj_set_pos(tile, col * TILE_W + TILE_GAP / 2,
                         row * TILE_H + TILE_GAP / 2);
    lv_obj_set_style_radius(tile, 8, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_shadow_width(tile, 0, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_style(tile, &style_confirm, LV_PART_MAIN | LV_STATE_USER_1);

    lv_color_t text_color = t->light_bg ? lv_color_black() : lv_color_white();

    if (t->type == TILE_COLOR) {
      lv_obj_set_style_bg_color(tile, lv_color_make(t->r, t->g, t->b), 0);
      lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    } else {
      lv_obj_set_style_bg_color(tile, lv_color_make(0x30, 0x30, 0x38), 0);
      lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);

      lv_obj_t *icon = lv_label_create(tile);
      lv_label_set_text(icon, t->icon);
      lv_obj_set_style_text_color(icon, text_color, 0);
      lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 6);
    }

    lv_obj_t *label = lv_label_create(tile);
    lv_label_set_text(label, t->caption);
    lv_obj_set_style_text_font(label, &font_de_14, 0);
    lv_obj_set_style_text_color(label, text_color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -6);

    lv_obj_add_event_cb(tile, tile_event_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)i);
  }
}

static void ui_show_placeholder_remote(device_id_t device)
{
  if (device <= DEVICE_LAMP || device >= DEVICE_COUNT) {
    ui_show_device_menu();
    return;
  }

  clear_screen();

  lv_obj_t *back = create_header(devices[device].caption);
  lv_obj_add_event_cb(back, back_event_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *scr = lv_scr_act();

  lv_obj_t *label = lv_label_create(scr);
  lv_label_set_text(label, "Befehle folgen");
  lv_obj_set_style_text_font(label, &font_de_14, 0);
  lv_obj_set_style_text_color(label, lv_color_make(0xB0, 0xB0, 0xB8), 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 12);
}

void ui_remote_create(void)
{
  // Oberste Ebene: Beim Einschalten immer zuerst die Geraeteauswahl.
  ui_show_device_menu();
}
