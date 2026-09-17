#ifndef IR_BSP_H
#define IR_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

// Initialisiert den IR-Sender (IRremoteESP8266). Einmalig aus setup()
// aufrufen, bevor Kacheln gedrueckt werden koennen.
void ir_bsp_init(void);

// Sendet einen RC6-Mode-0-Befehl (fuer den Philips-TV). address/command
// wie in den aufgezeichneten Codes (z.B. Adresse 0x0, Ok = Command 0x5C).
// Toggle-Bit wird intern automatisch verwaltet.
void ir_send_rc6(uint8_t address, uint8_t command);

#ifdef __cplusplus
}
#endif

#endif