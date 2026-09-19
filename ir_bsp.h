#ifndef IR_BSP_H
#define IR_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ir_bsp_init(void);

void ir_send_rc6(uint8_t address, uint8_t command);

typedef enum {
    IR_PROTO_NEC,
    IR_PROTO_SONY,
    IR_PROTO_DENON
} ir_protocol_t;

void ir_send_generic(uint64_t data,
                      uint16_t nbits,
                      ir_protocol_t protocol);

// Rohdaten-Versand: mark/space-Sequenz in µs, 38 kHz.
// Array muss mit einem Mark beginnen und mit einem Mark enden.
void ir_send_raw(const uint16_t *data, uint16_t len);

// Soundbar: eigene, frei gewaehlte NEC-Codes (siehe ir_bsp.cpp) fuer das
// Anlernen an der Soundbar - nicht von der Original-Fernbedienung
// aufgezeichnet.
void ir_send_soundbar_command(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif