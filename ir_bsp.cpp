#include "ir_bsp.h"
#include "ui_remote.h"

// Sende-Pin fuer die IR-LED.
#define IR_SEND_PIN 22

// Umgestellt von IRremote (v4.x) auf IRremoteESP8266, da IRremote
// bekannte plattformspezifische Sende-Bugs bei neueren ESP32-Varianten
// hat (u.a. dokumentiert fuer ESP32-S3) und IRremoteESP8266 auf dem
// ESP32 ueber die RMT-Peripherie sendet. Dieselbe Library nutzt auch
// das OMOTE-Projekt.
#include <IRremoteESP8266.h>
#include <IRsend.h>

// Alle 24 Kacheln der Fernbedienung verwenden dieselbe NEC-Adresse.
// Praktischerweise entspricht das gemessene NEC-Kommando-Byte exakt
// dem Kachel-Index (0 = "Heller" ... 23 = "Uebergang langsam", siehe
// Reihenfolge des tiles[]-Arrays in ui_remote.c) - daher genuegt eine
// direkte Umrechnung ohne separate Zuordnungstabelle.
#define IR_NEC_ADDRESS 0xEF00

// Anzahl zusaetzlicher Wiederholungen pro Tastendruck. 0 = nur einmal
// senden. Falls Befehle beim Original-Geraet gelegentlich nicht
// ankommen, hier z.B. auf 1 oder 2 erhoehen.
#define IR_NUM_REPEATS 0

// inverted=false, use_modulation=true (Standard) - passt zu unserer
// NPN-Transistorschaltung (GPIO HIGH -> LED an), keine Aenderung an
// der Hardware noetig.
IRsend IrSender(IR_SEND_PIN);

void ir_bsp_init(void)
{
  IrSender.begin();
}

// Ueberschreibt die __attribute__((weak))-Platzhalterfunktion aus
// ui_remote.c - der Rest der UI (Kacheln, Touch, Bestaetigungs-Flash)
// muss dafuer nicht angefasst werden.
extern "C" void ir_send_command(uint8_t index)
{
  if (index < REMOTE_TILE_COUNT) {
    // IRremoteESP8266 kennt (anders als IRremote) kein sendNEC(address,
    // command, repeat) mit automatischer Adress-/Kommando-Kodierung.
    // encodeNEC() baut daraus das rohe 32-Bit-NEC-Datenwort (inkl.
    // Erkennung von "Extended NEC" bei 16-Bit-Adressen wie unserer
    // 0xEF00), das sendNEC() dann erwartet.
    uint32_t necData = IrSender.encodeNEC(IR_NEC_ADDRESS, index);
    IrSender.sendNEC(necData, kNECBits, IR_NUM_REPEATS);
  }
}