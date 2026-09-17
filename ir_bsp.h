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

#ifdef __cplusplus
}
#endif

#endif