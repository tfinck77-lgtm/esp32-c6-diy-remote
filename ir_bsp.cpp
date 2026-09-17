#include "ir_bsp.h"
#include "ui_remote.h"

// Sende-Pin fuer die IR-LED.
#define IR_SEND_PIN 22

// Umgestellt von IRremote (v4.x) auf IRremoteESP8266, da IRremote
// bekannte plattformspezifische Sende-Bugs bei neueren ESP32-Varianten
// hat (u.a. dokumentiert fuer ESP32-S3) und IRremoteESP8266 auf dem
// ESP32 ueber die RMT-Peripherie sendet.
#include <IRremoteESP8266.h>
#include <IRsend.h>

// Alle 24 Kacheln der Fernbedienung verwenden dieselbe NEC-Adresse.
#define IR_NEC_ADDRESS 0xEF00

// Anzahl zusaetzlicher Wiederholungen pro Tastendruck.
#define IR_NUM_REPEATS 0

// inverted=false, use_modulation=true (Standard).
IRsend IrSender(IR_SEND_PIN);


// ---------------------------------------------------------------------
// Bit-Reihenfolge
// ---------------------------------------------------------------------
//
// Die Raw-Data-Werte in den IR-Codes-*.txt wurden mit Arduino-IRremote
// aufgezeichnet. Diese Werte liegen dort in LSB-first-Darstellung vor.
//
// IRremoteESP8266 erwartet bei den direkten send*()-Aufrufen dagegen
// das Datenwort in der entgegengesetzten Reihenfolge.
//
// Deshalb werden NUR die direkt uebergebenen Raw-Daten hier gespiegelt.
// Die speziell aufgebauten Codes fuer Lampe (NEC encodeNEC) und TV (RC6)
// werden NICHT gespiegelt.
// ---------------------------------------------------------------------

static uint64_t reverseBits(uint64_t data, uint16_t nbits)
{
  uint64_t result = 0;

  for (uint16_t i = 0; i < nbits; i++) {
    result <<= 1;
    result |= (data & 1ULL);
    data >>= 1;
  }

  return result;
}


void ir_bsp_init(void)
{
  IrSender.begin();
}


// ---------------------------------------------------------------------
// Lampe
// ---------------------------------------------------------------------

// Ueberschreibt die __attribute__((weak))-Platzhalterfunktion aus
// ui_remote.c.
extern "C" void ir_send_command(uint8_t index)
{
  if (index < REMOTE_TILE_COUNT) {

    // Hier NICHT reverseBits() verwenden.
    //
    // encodeNEC() erzeugt bereits das fuer IRremoteESP8266 passende
    // Datenwort.
    uint32_t necData = IrSender.encodeNEC(IR_NEC_ADDRESS, index);

    IrSender.sendNEC(
        necData,
        kNECBits,
        IR_NUM_REPEATS
    );
  }
}


// ---------------------------------------------------------------------
// TV - Philips RC6
// ---------------------------------------------------------------------

extern "C" void ir_send_rc6(uint8_t address, uint8_t command)
{
  static bool toggle = false;
  toggle = !toggle;

  uint64_t data =
      ((uint64_t)(toggle ? 1 : 0) << 16) |
      ((uint64_t)address << 8) |
      (uint64_t)command;

  // Ebenfalls NICHT spiegeln.
  //
  // Der RC6-Wert wird hier explizit fuer sendRC6() aufgebaut.
  IrSender.sendRC6(
      data,
      20,
      IR_NUM_REPEATS
  );
}


// ---------------------------------------------------------------------
// Generische Raw-Codes
//
// Soundbar     -> Denon/Kaseikyo, 48 Bit
// Bluray       -> Sony, 20 Bit bzw. 12 Bit
// Internetradio-> NEC, 32 Bit
// LED-Kerzen   -> NEC, 32 Bit
// Nixie-Uhr    -> NEC, 32 Bit
//
// Diese Werte stammen direkt aus den aufgezeichneten Raw-Data-Werten.
// Deshalb wird hier vor dem Senden die Bitreihenfolge gespiegelt.
// ---------------------------------------------------------------------

extern "C" void ir_send_generic(
    uint64_t data,
    uint16_t nbits,
    ir_protocol_t protocol)
{
  // Sicherheitscheck gegen ungueltige Bitlaengen.
  if (nbits == 0 || nbits > 64) {
    return;
  }

  // Arduino-IRremote-Aufzeichnung -> IRremoteESP8266-Sendedaten.
  const uint64_t correctedData = reverseBits(data, nbits);

  switch (protocol) {

    case IR_PROTO_NEC:
      IrSender.sendNEC(
          correctedData,
          nbits,
          IR_NUM_REPEATS
      );
      break;

    case IR_PROTO_SONY:
      // IRremoteESP8266 sendSony() verwendet standardmaessig
      // kSonyMinRepeat = 2, also insgesamt drei Sendungen.
      IrSender.sendSony(
          correctedData,
          nbits
      );
      break;

    case IR_PROTO_DENON:
      // 48 Bit Kaseikyo/Denon.
      IrSender.sendDenon(
          correctedData,
          nbits,
          IR_NUM_REPEATS
      );
      break;
  }
}