#ifndef IR_BSP_H
#define IR_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

// Initialisiert den IR-Sender (IRremoteESP8266). Einmalig aus setup()
// aufrufen, bevor Kacheln gedrueckt werden koennen.
void ir_bsp_init(void);

#ifdef __cplusplus
}
#endif

#endif