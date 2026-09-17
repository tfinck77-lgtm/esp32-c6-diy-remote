#ifndef UI_REMOTE_H
#define UI_REMOTE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Anzahl der Befehle der RGB-Lampen-Fernbedienung.
#define REMOTE_TILE_COUNT 24

// Wird aufgerufen, sobald eine Befehlskachel der RGB-Lampe angetippt wird.
// index: 0..23 (Reihe fuer Reihe, links nach rechts, siehe ui_remote.c).
// Die echte Implementierung liegt in ir_bsp.cpp.
void ir_send_command(uint8_t index);

// Einstiegspunkt der UI. Zeigt beim Start die Geraeteauswahl; von dort
// wird in die jeweilige Befehlsansicht gewechselt.
void ui_remote_create(void);

#ifdef __cplusplus
}
#endif

#endif
