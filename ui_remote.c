#include "ui_remote.h"
#include "ir_bsp.h"
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

// ---------------------------------------------------------------------
// TV (Philips, RC6-Protokoll, Adresse 0x0). Layout wie bei der Lampe:
// 4 Spalten, Farbtasten mit eigenem Hintergrund, Rest mit Icon/Text.
// ---------------------------------------------------------------------

#define TV_RC6_ADDRESS 0x00

typedef struct {
  const char *caption;
  const char *icon;     // NULL -> nur Text, zentriert (z.B. Zifferntasten)
  uint8_t     command;
  uint8_t     is_color;
  uint8_t     r, g, b;
  uint8_t     light_bg;
} tv_tile_def_t;

static const tv_tile_def_t tv_tiles[] = {
  { "Ein/Aus",   LV_SYMBOL_POWER,  0x0C, 0,0,0,0, 0 },
  { "Home",      LV_SYMBOL_HOME,   0x54, 0,0,0,0, 0 },
  { "Back",      LV_SYMBOL_LEFT,   0x0A, 0,0,0,0, 0 },
  { "Menü",      LV_SYMBOL_LIST,   0x57, 0,0,0,0, 0 },
  { "Sources",   LV_SYMBOL_USB,    0x38, 0,0,0,0, 0 },
  { "Ambilight", LV_SYMBOL_IMAGE,  0x8F, 0,0,0,0, 0 },

  { "Hoch",      LV_SYMBOL_UP,     0x58, 0,0,0,0, 0 },
  { "Links",     LV_SYMBOL_LEFT,   0x5A, 0,0,0,0, 0 },
  { "Ok",        LV_SYMBOL_OK,     0x5C, 0,0,0,0, 0 },
  { "Rechts",    LV_SYMBOL_RIGHT,  0x5B, 0,0,0,0, 0 },
  { "Runter",    LV_SYMBOL_DOWN,   0x59, 0,0,0,0, 0 },
  { "Mute",      LV_SYMBOL_MUTE,   0x0D, 0,0,0,0, 0 },

  { "Vol +",     LV_SYMBOL_VOLUME_MAX, 0x10, 0,0,0,0, 0 },
  { "Vol -",     LV_SYMBOL_VOLUME_MID, 0x11, 0,0,0,0, 0 },
  { "Sender +",  LV_SYMBOL_PLUS,   0x20, 0,0,0,0, 0 },
  { "Sender -",  LV_SYMBOL_MINUS,  0x21, 0,0,0,0, 0 },
  { "Bildformat",LV_SYMBOL_EYE_OPEN, 0xF5, 0,0,0,0, 0 },
  { "TV-Guide",  LV_SYMBOL_LIST,   0xCC, 0,0,0,0, 0 },

  { "Rewind",    LV_SYMBOL_PREV,   0x2B, 0,0,0,0, 0 },
  { "Play",      LV_SYMBOL_PLAY,   0x2C, 0,0,0,0, 0 },
  { "Pause",     LV_SYMBOL_PAUSE,  0x30, 0,0,0,0, 0 },
  { "Stop",      LV_SYMBOL_STOP,   0x31, 0,0,0,0, 0 },
  { "Forward",   LV_SYMBOL_NEXT,   0x28, 0,0,0,0, 0 },
  { "Record",    LV_SYMBOL_CHARGE, 0x37, 0,0,0,0, 0 },

  { "Rot",       NULL, 0x6D, 1, 0xE0,0x10,0x10, 0 },
  { "Grün",      NULL, 0x6E, 1, 0x10,0xC0,0x20, 0 },
  { "Gelb",      NULL, 0x6F, 1, 0xFF,0xF0,0x00, 1 },
  { "Blau",      NULL, 0x70, 1, 0x20,0x40,0xFF, 0 },
  { "Netflix",   LV_SYMBOL_VIDEO,  0x76, 0,0,0,0, 0 },
  { "YouTube",   LV_SYMBOL_VIDEO,  0x79, 0,0,0,0, 0 },

  { "Prime Video", LV_SYMBOL_VIDEO, 0xBA, 0,0,0,0, 0 },
  { "Videotext", LV_SYMBOL_KEYBOARD, 0x3C, 0,0,0,0, 0 },

  { "1", NULL, 0x1, 0,0,0,0, 0 },
  { "2", NULL, 0x2, 0,0,0,0, 0 },
  { "3", NULL, 0x3, 0,0,0,0, 0 },
  { "4", NULL, 0x4, 0,0,0,0, 0 },
  { "5", NULL, 0x5, 0,0,0,0, 0 },
  { "6", NULL, 0x6, 0,0,0,0, 0 },
  { "7", NULL, 0x7, 0,0,0,0, 0 },
  { "8", NULL, 0x8, 0,0,0,0, 0 },
  { "9", NULL, 0x9, 0,0,0,0, 0 },
  { "0", NULL, 0x0, 0,0,0,0, 0 },
};

#define TV_TILE_COUNT (sizeof(tv_tiles) / sizeof(tv_tiles[0]))

// ---------------------------------------------------------------------
// Soundbar, Bluray-Player, Internetradio, LED-Kerzen, Nixietube-Uhr:
// gemeinsame Kachel-Definition. data/nbits sind 1:1 der aufgezeichnete
// Raw-Data-Wert samt Bitanzahl aus den IR-Codes-*.txt-Dateien - siehe
// ir_send_generic() in ir_bsp.cpp fuer den Versand.
// ---------------------------------------------------------------------

typedef struct {
  const char *caption;
  const char *icon;     // NULL -> nur Text, zentriert (z.B. Zifferntasten)
  uint64_t    data;
  uint16_t    nbits;
  uint8_t     is_color;
  uint8_t     r, g, b;
  uint8_t     light_bg;
} generic_tile_def_t;

// --- Soundbar (Kaseikyo_Denon, jeweils 48 Bit) ---
static const generic_tile_def_t soundbar_tiles[] = {
  { "Ein/Aus",            LV_SYMBOL_POWER,      0x6A0832503254, 48, 0,0,0,0, 0 },
  { "Mute",                LV_SYMBOL_MUTE,       0x3A0862503254, 48, 0,0,0,0, 0 },
  { "TV",                  LV_SYMBOL_VIDEO,      0xCB0992503254, 48, 0,0,0,0, 0 },
  { "HDMI",                NULL,                 0xEB09B2503254, 48, 0,0,0,0, 0 },
  { "OPT",                 NULL,                 0x3B0962503254, 48, 0,0,0,0, 0 },
  { "AUX",                 NULL,                 0xDA0882503254, 48, 0,0,0,0, 0 },
  { "PURE",                NULL,                 0x180A42503254, 48, 0,0,0,0, 0 },
  { "Bluetooth",           LV_SYMBOL_BLUETOOTH,  0x5E0C02503254, 48, 0,0,0,0, 0 },
  { "Vol +",               LV_SYMBOL_VOLUME_MAX, 0x1A0842503254, 48, 0,0,0,0, 0 },
  { "Vol -",                LV_SYMBOL_VOLUME_MID, 0xA0852503254, 48, 0,0,0,0, 0 },
  { "Bass +",              LV_SYMBOL_PLUS,       0x4E0C12503254, 48, 0,0,0,0, 0 },
  { "Bass -",              LV_SYMBOL_MINUS,      0x7E0C22503254, 48, 0,0,0,0, 0 },
  { "Movie",               LV_SYMBOL_IMAGE,      0x680A32503254, 48, 0,0,0,0, 0 },
  { "Music",               LV_SYMBOL_AUDIO,      0x80A52503254,  48, 0,0,0,0, 0 },
  { "Night",               NULL,                 0x880AD2503254, 48, 0,0,0,0, 0 },
  { "Dialog\nLow",        NULL,                 0x6E0C32503254, 48, 0,0,0,0, 0 },
  { "Dialog\nMed",        NULL,                 0x1E0C42503254, 48, 0,0,0,0, 0 },
  { "Dialog\nHigh",       NULL,                 0xE0C52503254,  48, 0,0,0,0, 0 },
};
#define SOUNDBAR_TILE_COUNT (sizeof(soundbar_tiles) / sizeof(soundbar_tiles[0]))

// --- Bluray-Player (Sony; Vol +/-/Mute nutzen die TV-Lautstaerke des
// Players mit 12 Bit, der Rest 20 Bit). Die im Original als "bei mir
// funktionslos" markierten TV-Tasten (TV Input, TV Ein/Aus) sind
// bewusst weggelassen. ---
static const generic_tile_def_t bluray_tiles[] = {
  { "Ein/Aus",       LV_SYMBOL_POWER, 0xE2D15, 20, 0,0,0,0, 0 },
  { "Disc\nauswerfen", LV_SYMBOL_EJECT, 0xE2D16, 20, 0,0,0,0, 0 },
  { "Home",          LV_SYMBOL_HOME,  0xE2D42, 20, 0,0,0,0, 0 },
  { "Top Menu",      LV_SYMBOL_LIST,  0xE2D2C, 20, 0,0,0,0, 0 },
  { "Pop Up /\nMenu", LV_SYMBOL_LIST, 0xE2D29, 20, 0,0,0,0, 0 },
  { "Options",       LV_SYMBOL_SETTINGS, 0xE2D3F, 20, 0,0,0,0, 0 },

  { "Hoch",          LV_SYMBOL_UP,    0xE2D39, 20, 0,0,0,0, 0 },
  { "Links",         LV_SYMBOL_LEFT,  0xE2D3B, 20, 0,0,0,0, 0 },
  { "Mitte/Ok",      LV_SYMBOL_OK,    0xE2D3D, 20, 0,0,0,0, 0 },
  { "Rechts",        LV_SYMBOL_RIGHT, 0xE2D3C, 20, 0,0,0,0, 0 },
  { "Runter",        LV_SYMBOL_DOWN,  0xE2D3A, 20, 0,0,0,0, 0 },
  { "Return",        LV_SYMBOL_LEFT,  0xE2D43, 20, 0,0,0,0, 0 },

  { "Rewind",        LV_SYMBOL_PREV,  0xE2D1B, 20, 0,0,0,0, 0 },
  { "Play",          LV_SYMBOL_PLAY,  0xE2D1A, 20, 0,0,0,0, 0 },
  { "Pause",         LV_SYMBOL_PAUSE, 0xE2D19, 20, 0,0,0,0, 0 },
  { "Stop",          LV_SYMBOL_STOP,  0xE2D18, 20, 0,0,0,0, 0 },
  { "Forward",       LV_SYMBOL_NEXT,  0xE2D1C, 20, 0,0,0,0, 0 },
  { "Previous",      LV_SYMBOL_PREV,  0xE2D57, 20, 0,0,0,0, 0 },

  { "Next",          LV_SYMBOL_NEXT,  0xE2D56, 20, 0,0,0,0, 0 },
  { "Subtitle",      LV_SYMBOL_KEYBOARD, 0xE2D63, 20, 0,0,0,0, 0 },
  { "Audio",         LV_SYMBOL_AUDIO, 0xE2D64, 20, 0,0,0,0, 0 },
  { "Display",       LV_SYMBOL_EYE_OPEN, 0xE2D41, 20, 0,0,0,0, 0 },
  { "Favorite",      NULL,            0xE2D5E, 20, 0,0,0,0, 0 },
  { "Rot",           NULL,            0xE2D69, 20, 1, 0xE0,0x10,0x10, 0 },

  { "Grün",          NULL,            0xE2D66, 20, 1, 0x10,0xC0,0x20, 0 },
  { "Gelb",          NULL,            0xE2D67, 20, 1, 0xFF,0xF0,0x00, 1 },
  { "Blau",          NULL,            0xE2D68, 20, 1, 0x20,0x40,0xFF, 0 },
  { "Vol +",         LV_SYMBOL_VOLUME_MAX, 0x92, 12, 0,0,0,0, 0 },
  { "Vol -",         LV_SYMBOL_VOLUME_MID, 0x93, 12, 0,0,0,0, 0 },
  { "Mute",          LV_SYMBOL_MUTE,  0x94, 12, 0,0,0,0, 0 },
};
#define BLURAY_TILE_COUNT (sizeof(bluray_tiles) / sizeof(bluray_tiles[0]))

// --- Internetradio (NEC, Adresse 0xE608, jeweils 32 Bit) ---
static const generic_tile_def_t internetradio_tiles[] = {
  { "Ein/Aus", LV_SYMBOL_POWER,  0xB847E608, 32, 0,0,0,0, 0 },
  { "Quelle",  LV_SYMBOL_USB,    0xBC43E608, 32, 0,0,0,0, 0 },
  { "Menü",    LV_SYMBOL_LIST,   0xE11EE608, 32, 0,0,0,0, 0 },
  { "zurück",  LV_SYMBOL_LEFT,   0xA758E608, 32, 0,0,0,0, 0 },

  { "Wiedergabe/\nPause", LV_SYMBOL_PLAY, 0xAB54E608, 32, 0,0,0,0, 0 },
  { "Stop/\nAuswurf",     LV_SYMBOL_STOP, 0xED12E608, 32, 0,0,0,0, 0 },
  { "Ok/Mute", LV_SYMBOL_OK,     0xEA15E608, 32, 0,0,0,0, 0 },
  { "Wiederholung", LV_SYMBOL_LOOP, 0xE51AE608, 32, 0,0,0,0, 0 },

  { "Rücklauf/\nTitel zurück", LV_SYMBOL_PREV, 0xE21DE608, 32, 0,0,0,0, 0 },
  { "Vorlauf/\nTitel vor",     LV_SYMBOL_NEXT, 0xEE11E608, 32, 0,0,0,0, 0 },
  { "Vol +", LV_SYMBOL_VOLUME_MAX, 0xEB14E608, 32, 0,0,0,0, 0 },
  { "Vol -", LV_SYMBOL_VOLUME_MID, 0xE916E608, 32, 0,0,0,0, 0 },

  { "Zufalls-\nwiedergabe", LV_SYMBOL_SHUFFLE, 0xB14EE608, 32, 0,0,0,0, 0 },
  { "Info",    NULL,             0xA659E608, 32, 0,0,0,0, 0 },
  { "Programm", NULL,            0xB34CE608, 32, 0,0,0,0, 0 },
  { "Sleep",   NULL,             0xE817E608, 32, 0,0,0,0, 0 },

  { "1. Alarm", NULL,            0xE01FE608, 32, 0,0,0,0, 0 },
  { "2. Alarm", NULL,            0xE41BE608, 32, 0,0,0,0, 0 },
  { "ZZz",     NULL,             0xEC13E608, 32, 0,0,0,0, 0 },
  { "P",       NULL,             0xA35CE608, 32, 0,0,0,0, 0 },

  { "1", NULL, 0xBD42E608, 32, 0,0,0,0, 0 },
  { "2", NULL, 0xBE41E608, 32, 0,0,0,0, 0 },
  { "3", NULL, 0xBF40E608, 32, 0,0,0,0, 0 },
  { "4", NULL, 0xB946E608, 32, 0,0,0,0, 0 },
  { "5", NULL, 0xBA45E608, 32, 0,0,0,0, 0 },
  { "6", NULL, 0xBB44E608, 32, 0,0,0,0, 0 },
  { "7", NULL, 0xB54AE608, 32, 0,0,0,0, 0 },
  { "8", NULL, 0xB649E608, 32, 0,0,0,0, 0 },
  { "9", NULL, 0xB748E608, 32, 0,0,0,0, 0 },
  { "10",  NULL, 0xB24DE608, 32, 0,0,0,0, 0 },
  { "10+", NULL, 0xA25DE608, 32, 0,0,0,0, 0 },
};
#define INTERNETRADIO_TILE_COUNT (sizeof(internetradio_tiles) / sizeof(internetradio_tiles[0]))

// --- LED-Kerzen (NEC, Adresse 0xB708, jeweils 32 Bit). Die
// aufgezeichnete Dublette "lichtekette aus/ein" (identischer Code wie
// "ein") wurde weggelassen. ---
static const generic_tile_def_t led_candles_tiles[] = {
  { "Ein",         LV_SYMBOL_POWER, 0xFF00B708, 32, 0,0,0,0, 0 },
  { "Aus",         LV_SYMBOL_POWER, 0xFD02B708, 32, 0,0,0,0, 0 },
  { "Heller",      LV_SYMBOL_PLUS,  0xED12B708, 32, 0,0,0,0, 0 },
  { "Dunkler",     LV_SYMBOL_MINUS, 0xEF10B708, 32, 0,0,0,0, 0 },
  { "Mode\nKerze", NULL,            0xF30CB708, 32, 0,0,0,0, 0 },
  { "Mode\nLicht", NULL,            0xF10EB708, 32, 0,0,0,0, 0 },
  { "Timer\n2 Std", NULL,           0xFB04B708, 32, 0,0,0,0, 0 },
  { "Timer\n4 Std", NULL,           0xF906B708, 32, 0,0,0,0, 0 },
  { "Timer\n6 Std", NULL,           0xF708B708, 32, 0,0,0,0, 0 },
  { "Timer\n8 Std", NULL,           0xF50AB708, 32, 0,0,0,0, 0 },
};
#define LED_CANDLES_TILE_COUNT (sizeof(led_candles_tiles) / sizeof(led_candles_tiles[0]))

// --- Nixietube-Uhr (NEC, Adresse 0x0, jeweils 32 Bit) ---
static const generic_tile_def_t nixie_tiles[] = {
  { "Ein/Aus", LV_SYMBOL_POWER, 0xBA45FF00, 32, 0,0,0,0, 0 },
  { "Menü",    LV_SYMBOL_LIST,  0xB946FF00, 32, 0,0,0,0, 0 },
  { "Mute",    LV_SYMBOL_MUTE,  0xB847FF00, 32, 0,0,0,0, 0 },
  { "Mode",    NULL,            0xBB44FF00, 32, 0,0,0,0, 0 },

  { "+",       LV_SYMBOL_PLUS,  0xBF40FF00, 32, 0,0,0,0, 0 },
  { "-",       LV_SYMBOL_MINUS, 0xE619FF00, 32, 0,0,0,0, 0 },
  { "zurück",  LV_SYMBOL_LEFT,  0xBC43FF00, 32, 0,0,0,0, 0 },
  { "Ok",      LV_SYMBOL_OK,    0xF20DFF00, 32, 0,0,0,0, 0 },

  { "|<<",     LV_SYMBOL_PREV,  0xF807FF00, 32, 0,0,0,0, 0 },
  { "Play/\nPause", LV_SYMBOL_PLAY, 0xEA15FF00, 32, 0,0,0,0, 0 },
  { ">>|",     LV_SYMBOL_NEXT,  0xF609FF00, 32, 0,0,0,0, 0 },
  { "0", NULL, 0xE916FD00, 32, 0,0,0,0, 0 },

  { "1", NULL, 0xF30CFF00, 32, 0,0,0,0, 0 },
  { "2", NULL, 0xE718FF00, 32, 0,0,0,0, 0 },
  { "3", NULL, 0xA15EFF00, 32, 0,0,0,0, 0 },
  { "4", NULL, 0xF708FF00, 32, 0,0,0,0, 0 },
  { "5", NULL, 0xE31CFF00, 32, 0,0,0,0, 0 },
  { "6", NULL, 0xA55AFF00, 32, 0,0,0,0, 0 },
  { "7", NULL, 0xBD42FF00, 32, 0,0,0,0, 0 },
  { "8", NULL, 0xAD52FF00, 32, 0,0,0,0, 0 },
  { "9", NULL, 0xB54AFF00, 32, 0,0,0,0, 0 },
};
#define NIXIE_TILE_COUNT (sizeof(nixie_tiles) / sizeof(nixie_tiles[0]))

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
static void ui_show_tv_remote(void);
static void ui_show_internetradio_remote(void);
static void ui_show_led_candles_remote(void);
static void ui_show_nixie_remote(void);
static void ui_show_soundbar_remote(void);
static void ui_show_bluray_remote(void);
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

static void tv_tile_event_cb(lv_event_t *e)
{
  uint32_t index = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
  lv_obj_t *tile = lv_event_get_target(e);

  ir_send_rc6(TV_RC6_ADDRESS, tv_tiles[index].command);

  lv_obj_add_state(tile, LV_STATE_USER_1);
  lv_timer_t *flash_timer = lv_timer_create(confirm_flash_timer_cb, 150, tile);
  lv_timer_set_repeat_count(flash_timer, 1);
}

// Aktives Kachel-Array + Protokoll fuer die generischen Geraete-Menues
// (Soundbar, Bluray-Player, Internetradio, LED-Kerzen, Nixietube-Uhr).
// Wird jeweils beim Aufbau des Menues in ui_show_generic_remote() gesetzt,
// generic_tile_event_cb() liest anhand des Kachel-Index daraus die
// passenden IR-Daten.
static const generic_tile_def_t *g_active_generic_tiles = NULL;
static ir_protocol_t g_active_generic_protocol;

static void generic_tile_event_cb(lv_event_t *e)
{
  uint32_t index = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
  lv_obj_t *tile = lv_event_get_target(e);

  if (g_active_generic_tiles != NULL) {
    ir_send_generic(g_active_generic_tiles[index].data,
                    g_active_generic_tiles[index].nbits,
                    g_active_generic_protocol);
  }

  lv_obj_add_state(tile, LV_STATE_USER_1);
  lv_timer_t *flash_timer = lv_timer_create(confirm_flash_timer_cb, 150, tile);
  lv_timer_set_repeat_count(flash_timer, 1);
}

static void device_event_cb(lv_event_t *e)
{
  device_id_t device = (device_id_t)(uintptr_t)lv_event_get_user_data(e);

  if (device == DEVICE_LAMP) {
    ui_show_lamp_remote();
  } else if (device == DEVICE_TV) {
    ui_show_tv_remote();
  } else if (device == DEVICE_INTERNETRADIO) {
    ui_show_internetradio_remote();
  } else if (device == DEVICE_LED_CANDLES) {
    ui_show_led_candles_remote();
  } else if (device == DEVICE_NIXIE_CLOCK) {
    ui_show_nixie_remote();
  } else if (device == DEVICE_SOUNDBAR) {
    ui_show_soundbar_remote();
  } else if (device == DEVICE_BLURAY) {
    ui_show_bluray_remote();
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

static void ui_show_tv_remote(void)
{
  clear_screen();

  lv_obj_t *back = create_header("TV");
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

  for (uint32_t i = 0; i < TV_TILE_COUNT; i++) {
    uint32_t col = i % REMOTE_COLS;
    uint32_t row = i / REMOTE_COLS;
    const tv_tile_def_t *t = &tv_tiles[i];

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

    if (t->is_color) {
      lv_obj_set_style_bg_color(tile, lv_color_make(t->r, t->g, t->b), 0);
      lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    } else {
      lv_obj_set_style_bg_color(tile, lv_color_make(0x30, 0x30, 0x38), 0);
      lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);

      if (t->icon != NULL) {
        lv_obj_t *icon = lv_label_create(tile);
        lv_label_set_text(icon, t->icon);
        lv_obj_set_style_text_color(icon, text_color, 0);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 6);
      }
    }

    lv_obj_t *label = lv_label_create(tile);
    lv_label_set_text(label, t->caption);
    lv_obj_set_style_text_font(label, &font_de_14, 0);
    lv_obj_set_style_text_color(label, text_color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    if (t->icon != NULL) {
      lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -6);
    } else {
      lv_obj_center(label);
    }

    lv_obj_add_event_cb(tile, tv_tile_event_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)i);
  }
}

// Gemeinsame Render-Funktion fuer Soundbar/Bluray-Player/Internetradio/
// LED-Kerzen/Nixietube-Uhr - Layout 1:1 wie ui_show_tv_remote(), nur mit
// generic_tile_def_t (Raw-Data+Bitanzahl+Protokoll statt einzelnem
// Kommando-Byte) und generic_tile_event_cb() als Klick-Handler.
static void ui_show_generic_remote(device_id_t device, const generic_tile_def_t *tiles,
                                    uint32_t count, ir_protocol_t protocol)
{
  clear_screen();

  lv_obj_t *back = create_header(devices[device].caption);
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

  g_active_generic_tiles = tiles;
  g_active_generic_protocol = protocol;

  for (uint32_t i = 0; i < count; i++) {
    uint32_t col = i % REMOTE_COLS;
    uint32_t row = i / REMOTE_COLS;
    const generic_tile_def_t *t = &tiles[i];

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

    if (t->is_color) {
      lv_obj_set_style_bg_color(tile, lv_color_make(t->r, t->g, t->b), 0);
      lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    } else {
      lv_obj_set_style_bg_color(tile, lv_color_make(0x30, 0x30, 0x38), 0);
      lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);

      if (t->icon != NULL) {
        lv_obj_t *icon = lv_label_create(tile);
        lv_label_set_text(icon, t->icon);
        lv_obj_set_style_text_color(icon, text_color, 0);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 6);
      }
    }

    lv_obj_t *label = lv_label_create(tile);
    lv_label_set_text(label, t->caption);
    lv_obj_set_style_text_font(label, &font_de_14, 0);
    lv_obj_set_style_text_color(label, text_color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    if (t->icon != NULL) {
      lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -6);
    } else {
      lv_obj_center(label);
    }

    lv_obj_add_event_cb(tile, generic_tile_event_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)i);
  }
}

static void ui_show_internetradio_remote(void)
{
  ui_show_generic_remote(DEVICE_INTERNETRADIO, internetradio_tiles,
                          INTERNETRADIO_TILE_COUNT, IR_PROTO_NEC);
}

static void ui_show_led_candles_remote(void)
{
  ui_show_generic_remote(DEVICE_LED_CANDLES, led_candles_tiles,
                          LED_CANDLES_TILE_COUNT, IR_PROTO_NEC);
}

static void ui_show_nixie_remote(void)
{
  ui_show_generic_remote(DEVICE_NIXIE_CLOCK, nixie_tiles,
                          NIXIE_TILE_COUNT, IR_PROTO_NEC);
}

static void ui_show_soundbar_remote(void)
{
  ui_show_generic_remote(DEVICE_SOUNDBAR, soundbar_tiles,
                          SOUNDBAR_TILE_COUNT, IR_PROTO_DENON);
}

static void ui_show_bluray_remote(void)
{
  ui_show_generic_remote(DEVICE_BLURAY, bluray_tiles,
                          BLURAY_TILE_COUNT, IR_PROTO_SONY);
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