Hardware & Basis

Waveshare ESP32-C6-Touch-LCD-1.9 (170×320, ST7789V2 via SPI, Touch CST816/FT3168-kompatibel über I2C)
LVGL v8.4.0, Arduino IDE 2.3.10, arduino-esp32 Core 3.3.11, Flash Size 8MB
IR-Sendeschaltung: 3V3 → Vorwiderstand → TSAL6100 → 2N2222 (Basis über 220Ω an GPIO22), IRremoteESP8266-Library (Wechsel von IRremote löste damals das Reichweitenproblem)

UI-Struktur

Einstiegspunkt: Geräteauswahl-Menü (7 Kacheln, 2 Spalten), jede Kachel führt in einen Geräte-Screen mit eigenem Zurück-Button zum Menü
Geräte: Lampe (RGB-Deckenfluter), TV, Internetradio, LED-Kerzen, Nixietube-Uhr, Soundbar, Bluray-Player

Stand pro Gerät

Gerät	Status	Protokoll
Lampe	✅ vollständig, 24 Kacheln funktionsfähig	NEC, Adresse 0xEF00
TV	✅ gerade fertiggestellt, 42 Kacheln (Steuerkreuz, Zahlen, Streaming-Apps, Farbtasten etc.)	RC6, Adresse 0x0
Soundbar, Bluray-Player, Internetradio, LED-Kerzen, Nixietube-Uhr	⏳ Dummy-Screens ("Befehle folgen"), IR-Codes bereits erfasst und gespeichert, aber noch nicht in UI eingebaut	Sony / Kaseikyo_Denon / NEC (je nach Gerät)

Offene Punkte

Restliche 5 Geräte-Screens analog zum TV-Screen bauen (Codes liegen bereits vor)
Bildformat-Code (RC6 0xF5) noch nicht live am TV verifiziert
Für Nixietube-Uhr ist eine geführte Prozedur für Zeit-/Datumseinstellung geplant (nicht als normale Kachel)
Physische Tasten (Lautstärke, Steuerkreuz, Ein/Aus etc.) und die Frage Geräte- vs. Szenen-Modell sind bewusst zurückgestellt
WLAN/Home-Assistant-Anbindung als späteres, eigenständiges Kapitel vorgemerkt
Deep-Sleep-Verhalten und Akku-Optimierung noch zu verfeinern

Workflow

Sketch liegt im öffentlichen GitHub-Repo tfinck77-lgtm/esp32-c6-diy-remote – spart Nutzungsvolumen, da ich einzelne Dateien direkt von dort holen kann statt komplette ZIP-Reuploads zu brauchen
