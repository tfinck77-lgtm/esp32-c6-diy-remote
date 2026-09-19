#include <Arduino.h>
#include "ir_bsp.h"
#include "ui_remote.h"

// Sende-Pin fuer die IR-LED.
#define IR_SEND_PIN 22

#include <IRremoteESP8266.h>
#include <IRsend.h>

// Alle 24 Kacheln der Fernbedienung verwenden dieselbe NEC-Adresse.
#define IR_NEC_ADDRESS 0xEF00

// Anzahl zusaetzlicher Wiederholungen pro Tastendruck.
#define IR_NUM_REPEATS 0

// Pause zwischen zwei Raw-Frames (Denon erwartet bei kurzem
// Tastendruck der Originalfernbedienung zwei Frames).
#define IR_RAW_FRAME_GAP_MS 40

IRsend IrSender(IR_SEND_PIN);

// ---------------------------------------------------------------------
// Bit-Reihenfolge
// ---------------------------------------------------------------------
//
// Die Raw-Data-Werte in den IR-Codes-*.txt wurden mit Arduino-IRremote
// aufgezeichnet. Diese Werte liegen dort in LSB-first-Darstellung vor.
// IRremoteESP8266 erwartet bei den direkten send*()-Aufrufen dagegen
// das Datenwort in der entgegengesetzten Reihenfolge.
//
// Nur die ueber send*() uebergebenen Raw-Daten werden hier gespiegelt.
// Lampe (NEC encodeNEC), TV (RC6), Soundbar (NEC encodeNEC) und
// sendRaw() werden NICHT gespiegelt.
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
extern "C" void ir_send_command(uint8_t index)
{
    if (index < REMOTE_TILE_COUNT) {
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

    IrSender.sendRC6(
        data,
        20,
        IR_NUM_REPEATS
    );
}

// ---------------------------------------------------------------------
// Generische Raw-Codes (NEC / Sony)
//
// Soundbar laeuft NICHT mehr hierueber, sondern ueber
// ir_send_soundbar_command().
// ---------------------------------------------------------------------
extern "C" void ir_send_generic(uint64_t data,
                                 uint16_t nbits,
                                 ir_protocol_t protocol)
{
    if (nbits == 0 || nbits > 64) {
        return;
    }

    const uint64_t correctedData = reverseBits(data, nbits);

    switch (protocol) {
        case IR_PROTO_NEC:
            IrSender.sendNEC(correctedData, nbits, IR_NUM_REPEATS);
            break;
        case IR_PROTO_SONY:
            IrSender.sendSony(correctedData, nbits);
            break;
        case IR_PROTO_DENON:
            // Nicht mehr verwendet.
            break;
    }
}

// ---------------------------------------------------------------------
// Rohdaten-Versand fuer Mark/Space-Sequenzen.
// ---------------------------------------------------------------------
extern "C" void ir_send_raw(const uint16_t *data, uint16_t len)
{
    if (data == NULL || len == 0) {
        return;
    }

    IrSender.sendRaw(data, len, 38);
    delay(IR_RAW_FRAME_GAP_MS);
    IrSender.sendRaw(data, len, 38);
}

// ---------------------------------------------------------------------
// Soundbar - eigene NEC-Adresse
//
// Die Denon DHT-S217 lernt IR-Codes fremder Fernbedienungen selbst an
// (Source-Taste 3 s halten, dann mit der Original-Fernbedienung die
// Zielfunktion waehlen, danach den neuen Code senden). Wir muessen ihr
// daher keine original Denon/Kaseikyo-Codes mehr senden, sondern waehlen
// frei eigene NEC-Codes, die mit keinem anderen hier eingepflegten
// Geraet kollidieren:
//   - Lampe:          NEC, Adresse 0xEF00
//   - Internetradio:  NEC, Adresse 0xE608
//   - LED-Kerzen:      NEC, Adresse 0xB708
//   - Nixietube-Uhr:  NEC, Adresse 0x0000
//   - TV:             RC6 (anderes Protokoll)
//   - Bluray-Player:  Sony (anderes Protokoll)
// Adresse 0x1300 ist bei keinem der obigen Geraete in Verwendung.
// ---------------------------------------------------------------------
#define IR_SOUNDBAR_NEC_ADDRESS 0x1300

extern "C" void ir_send_soundbar_command(uint8_t index)
{
    uint32_t necData = IrSender.encodeNEC(IR_SOUNDBAR_NEC_ADDRESS, index);
    IrSender.sendNEC(necData, kNECBits, IR_NUM_REPEATS);
}