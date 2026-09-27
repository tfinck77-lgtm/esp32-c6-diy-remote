// ---------------------------------------------------------------------
// DIY-Fernbedienung (Waveshare ESP32-C6-Touch-LCD-1.9)
// Kachel-UI + IR + Deep Sleep mit IMU-Wake-on-Motion
// + Akkustand-Icon oben rechts
// + Screen-Wiederherstellung nach Wakeup
// + Bewegung zaehlt als Aktivitaet (wie Touch)
//
// AENDERUNGEN (Bugfix Motion-Detection nach 1. Deep-Sleep-Zyklus):
// - CTRL9-Kommando-Protokoll (WoM-Setup) wird jetzt korrekt per
//   STATUS1-Polling + CTRL_CMD_ACK (0x00) abgeschlossen, statt das
//   Acknowledge komplett wegzulassen. Laut QMI8658-Datenblatt bleibt
//   sonst ein interner "Command Pending"-Zustand haengen, der die
//   folgende Rueckkehr in den reinen Accel-Modus stoert.
// - qmi_configure_accel() quittiert defensiv ebenfalls ein evtl. noch
//   offenes CTRL9-Kommando, bevor es neu konfiguriert.
// - I2C-Lesefehler in imu_motion_detected() werden jetzt geloggt statt
//   still verschluckt zu werden.
// - Debug-Ausgabe zeigt jetzt zusaetzlich die rohen Accel-Werte, damit
//   man sieht, ob die Werte nach dem Wakeup wirklich einfrieren.
// ---------------------------------------------------------------------

#include "lcd_bsp.h"
#include "lcd_bl_pwm_bsp.h"
#include "ir_bsp.h"
#include "battery_icon.h"
#include "ui_remote.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_system.h"

// ---------------------------------------------------------------------
// QMI8658 Konstanten
// ---------------------------------------------------------------------
#define IMU_I2C_ADDR        0x6B
#define IMU_I2C_PORT        I2C_NUM_0

#define REG_CTRL1           0x02
#define REG_CTRL2           0x03
#define REG_CTRL7           0x08
#define REG_CTRL9           0x0A
#define REG_CAL1_L          0x0B
#define REG_CAL1_H          0x0C
#define REG_STATUS0         0x2E
#define REG_STATUS1         0x2F
#define REG_RESET           0x60
#define REG_RST_RESULT      0x4D
#define RESET_TRIGGER_VALUE 0xB0
#define RESET_SUCCESS_VALUE 0x80

// Accelerometer-Rohdaten (AX/AY/AZ), jeweils 16 Bit signed
#define REG_AX_L            0x35

#define IMU_INT1_PIN        GPIO_NUM_1
#define WOM_THRESHOLD_MG    50

// CTRL9-Kommando-Protokoll
#define CTRL9_CMD_ACK           0x00
#define CTRL9_CMD_WOM_SETTING   0x08
#define STATUS1_CMDDONE_BIT     0x01   // Bit0 = CmdDone
#define CTRL9_ACK_POLL_TRIES    50
#define CTRL9_ACK_POLL_DELAY_MS 2

// Schwelle fuer "Bewegung" im Wachzustand (Summe der Achsen-Deltas).
// Bei +-4g ist Vollausschlag +-32768. Rauschen in Ruhe: ~100-200.
// Bewusste Bewegung: >1000. 500 ist ein guter Kompromiss.
#define MOTION_DELTA_THRESHOLD  500

// Wie oft im Wachzustand auf Bewegung geprueft wird.
#define MOTION_CHECK_INTERVAL_MS  300

// ---------------------------------------------------------------------
// Stromspar-Konfiguration
// ---------------------------------------------------------------------
#define IDLE_TIMEOUT_MS     30000UL
#define BATT_CHECK_MS       30000UL

// DEVICE_COUNT aus ui_remote.c - hier hart als Zahl, weil der Enum
// nicht exportiert wird. Bei Aenderungen in ui_remote.c anpassen.
#define DEVICE_COUNT_HINT   8

// ---------------------------------------------------------------------
// RTC_NOINIT-Variablen
// ---------------------------------------------------------------------
RTC_NOINIT_ATTR static uint32_t boot_magic;
#define BOOT_MAGIC_FROM_WAKEUP  0xCAFEBABE

// 0xFF = Hauptmenue. Wird vor dem Sleep gesetzt, nach dem Wakeup gelesen.
RTC_NOINIT_ATTR static uint8_t last_screen_id;

static bool imuReady = false;

// ---------------------------------------------------------------------
// Bewegungs-Erkennung im Wachzustand
// ---------------------------------------------------------------------
static int16_t last_ax = 0, last_ay = 0, last_az = 0;
static bool    have_last_accel = false;

// =====================================================================
// QMI8658 I2C-Helfer
// =====================================================================
static esp_err_t qmi_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    return i2c_master_write_to_device(
        IMU_I2C_PORT, IMU_I2C_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t qmi_read_reg(uint8_t reg, uint8_t *out)
{
    return i2c_master_write_read_device(
        IMU_I2C_PORT, IMU_I2C_ADDR, &reg, 1, out, 1, pdMS_TO_TICKS(100));
}

static esp_err_t qmi_read_block(uint8_t reg, uint8_t *out, size_t len)
{
    return i2c_master_write_read_device(
        IMU_I2C_PORT, IMU_I2C_ADDR, &reg, 1, out, len, pdMS_TO_TICKS(100));
}

static uint8_t qmi_status1()
{
    uint8_t s = 0;
    qmi_read_reg(REG_STATUS1, &s);
    return s;
}

// =====================================================================
// CTRL9-Kommando-Protokoll: auf CmdDone warten und per ACK quittieren
// =====================================================================
// Laut QMI8658-Datenblatt setzt der Chip nach Ausfuehrung eines CTRL9-
// Kommandos STATUS1 Bit0 (CmdDone). Der Host MUSS das mit
// CTRL9 = CTRL_CMD_ACK (0x00) quittieren, sonst bleibt intern ein
// "Command Pending"-Zustand haengen, der spaetere Umkonfigurationen
// (z.B. zurueck in den reinen Accel-Modus) stoert.
static void qmi_ctrl9_wait_and_ack()
{
    bool cmdDone = false;
    for (int i = 0; i < CTRL9_ACK_POLL_TRIES; i++) {
        uint8_t s = qmi_status1();
        if (s & STATUS1_CMDDONE_BIT) {
            cmdDone = true;
            break;
        }
        delay(CTRL9_ACK_POLL_DELAY_MS);
    }

    if (!cmdDone) {
        Serial.println("[QMI] WARNUNG: CmdDone-Bit nicht gesetzt, quittiere trotzdem");
    }

    // Acknowledge schreiben (unabhaengig vom Timeout - sonst bleibt
    // der Pending-Zustand mit Sicherheit haengen).
    if (qmi_write_reg(REG_CTRL9, CTRL9_CMD_ACK) != ESP_OK) {
        Serial.println("[QMI] FEHLER: CTRL9-ACK konnte nicht geschrieben werden");
    }
}

// =====================================================================
// Echter Hardware-Reset des Sensors (Register 0x60 = 0xB0)
// =====================================================================
// Funktioniert laut Datenblatt "from any mode" - raeumt zuverlaessig
// jeden haengengebliebenen internen Zustand auf (z.B. nach WoM), im
// Gegensatz zum reinen Umschalten einzelner CTRL-Register-Bits.
static bool qmi_soft_reset()
{
    if (qmi_write_reg(REG_RESET, RESET_TRIGGER_VALUE) != ESP_OK) {
        Serial.println("[QMI] FEHLER: Reset-Kommando konnte nicht geschrieben werden");
        return false;
    }

    // Datenblatt: max. 15ms fuer den Reset-Vorgang
    unsigned long t0 = millis();
    while (millis() - t0 < 20) {
        uint8_t result = 0;
        if (qmi_read_reg(REG_RST_RESULT, &result) == ESP_OK && result == RESET_SUCCESS_VALUE) {
            Serial.println("[QMI] Reset erfolgreich.");
            return true;
        }
        delay(1);
    }

    Serial.println("[QMI] WARNUNG: Reset-Erfolg nicht bestaetigt (0x4D != 0x80)");
    return false;
}

// =====================================================================
// QMI8658: Accel-Modus (Wachzustand)
// =====================================================================
static bool qmi_configure_accel()
{
    Serial.println("[QMI] Accel-Modus konfigurieren...");

    // Erst einen echten Reset, damit ein evtl. haengengebliebener
    // WoM-Zustand vom vorherigen Sleep garantiert aufgeraeumt ist.
    qmi_soft_reset();
    delay(10);

    if (qmi_write_reg(REG_CTRL7, 0x00) != ESP_OK) return false;
    delay(10);

    // CTRL1: Auto-Ink, Big-Endian. Ohne INT1-Enable (0x60 statt 0x68).
    if (qmi_write_reg(REG_CTRL1, 0x60) != ESP_OK) return false;
    // CTRL2: Accel LP 21Hz, +-4g
    if (qmi_write_reg(REG_CTRL2, 0x1D) != ESP_OK) return false;
    delay(10);

    // CTRL7 = 0x01: nur Accelerometer an
    if (qmi_write_reg(REG_CTRL7, 0x01) != ESP_OK) return false;
    delay(50);

    uint8_t status0 = 0;
    qmi_read_reg(REG_STATUS0, &status0);
    Serial.printf("[QMI] Accel-Modus aktiv. STATUS0=0x%02X\n", status0);
    return true;
}

// =====================================================================
// QMI8658: WoM-Modus (Sleep)
// =====================================================================
static bool qmi_configure_wom()
{
    Serial.println("[QMI] WoM konfigurieren...");

    if (qmi_write_reg(REG_CTRL7, 0x00) != ESP_OK) return false;
    delay(10);

    if (qmi_write_reg(REG_CTRL1, 0x68) != ESP_OK) return false;
    if (qmi_write_reg(REG_CTRL2, 0x1D) != ESP_OK) return false;
    if (qmi_write_reg(REG_CAL1_L, WOM_THRESHOLD_MG) != ESP_OK) return false;
    if (qmi_write_reg(REG_CAL1_H, 0x00) != ESP_OK) return false;
    if (qmi_write_reg(REG_CTRL9, CTRL9_CMD_WOM_SETTING) != ESP_OK) return false;

    // CTRL9-Kommando korrekt abschliessen (CmdDone abwarten + ACK),
    // statt wie bisher nur zu delayen und das Acknowledge wegzulassen.
    qmi_ctrl9_wait_and_ack();
    delay(20);

    if (qmi_write_reg(REG_CTRL7, 0x01) != ESP_OK) return false;
    delay(50);

    Serial.println("[QMI] WoM aktiv.");
    return true;
}

// =====================================================================
// WoM-Flag clearen
// =====================================================================
static void qmi_clear_wom_flag()
{
    uint8_t s = qmi_status1();
    Serial.printf("[QMI] STATUS1 = 0x%02X\n", s);
    delay(10);

    pinMode(IMU_INT1_PIN, INPUT);
    if (digitalRead(IMU_INT1_PIN) != LOW) {
        Serial.println("[QMI] Fallback: WoM neu konfigurieren");
        qmi_configure_wom();
        delay(10);
    }
}

// =====================================================================
// Bewegungs-Erkennung im Wachzustand
// =====================================================================
static bool imu_motion_detected()
{
    uint8_t buf[6] = {0};
    esp_err_t err = qmi_read_block(REG_AX_L, buf, 6);
    if (err != ESP_OK) {
        Serial.printf("[IMU] FEHLER: I2C-Read fehlgeschlagen (%s)\n",
                      esp_err_to_name(err));
        return false;
    }

    int16_t ax = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t ay = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t az = (int16_t)((buf[5] << 8) | buf[4]);

    if (!have_last_accel) {
        last_ax = ax; last_ay = ay; last_az = az;
        have_last_accel = true;
        return false;
    }

    int32_t d = abs(ax - last_ax) + abs(ay - last_ay) + abs(az - last_az);

    last_ax = ax; last_ay = ay; last_az = az;

    if (d > MOTION_DELTA_THRESHOLD) {
        Serial.printf("[MOTION] delta=%ld (ax=%d ay=%d az=%d)\n",
                      (long)d, ax, ay, az);
        return true;
    }
    return false;
}

// =====================================================================
// Idle-Timer zuruecksetzen (funktioniert fuer Touch UND Bewegung)
// =====================================================================
// lv_disp_trig_activity() erwartet ein lv_disp_t*, nicht NULL.
// NULL wuerde die Funktion wirkungslos machen (siehe lv_disp.c).
static void reset_idle_timer()
{
    lv_disp_t *disp = lv_disp_get_default();
    if (disp) {
        lv_disp_trig_activity(disp);
    } else {
        Serial.println("[IDLE] WARNUNG: kein Default-Display!");
    }
}

// =====================================================================
// Deep Sleep
// =====================================================================
static void go_to_sleep()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println("[SLEEP] Gehe in Deep Sleep...");

    // Screen-ID merken, BEVOR wir schlafen gehen.
    uint8_t screen_now = ui_remote_get_current_device();
    if (screen_now >= DEVICE_COUNT_HINT) {
        screen_now = 0xFF;
    }
    last_screen_id = screen_now;
    Serial.printf("[SLEEP] Speichere Screen-ID %u\n", last_screen_id);

    Serial.flush();

    setUpdutySubdivide(LCD_PWM_MODE, LCD_PWM_MODE_0);   // Backlight aus
    delay(50);
    gpio_hold_en(GPIO_NUM_15);

    if (imuReady) {
        // QMI8658 in WoM-Modus umschalten.
        qmi_configure_wom();
        delay(20);

        qmi_clear_wom_flag();
        delay(20);

        if (digitalRead(IMU_INT1_PIN) != LOW) {
            unsigned long t0 = millis();
            while (digitalRead(IMU_INT1_PIN) != LOW && millis() - t0 < 500) {
                delay(10);
            }
            qmi_clear_wom_flag();
            delay(20);
        }

        esp_err_t err = esp_deep_sleep_enable_gpio_wakeup(
            1ULL << IMU_INT1_PIN, ESP_GPIO_WAKEUP_GPIO_HIGH);
        if (err != ESP_OK) {
            Serial.printf("[SLEEP] Wakeup-Config Fehler: %s\n", esp_err_to_name(err));
        }
        Serial.println("[SLEEP] Bewegung weckt auf.");
    } else {
        Serial.println("[SLEEP] IMU nicht bereit - KEIN Wakeup konfiguriert!");
    }

    Serial.flush();
    delay(100);
    esp_deep_sleep_start();
}

// =====================================================================
// setup()
// =====================================================================
void setup()
{
    Serial.begin(115200);

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    bool from_wakeup = (boot_magic == BOOT_MAGIC_FROM_WAKEUP);
    bool cold_boot = (cause == ESP_SLEEP_WAKEUP_UNDEFINED) && !from_wakeup;

    if (cold_boot) {
        delay(1000);
    }

    Serial.println("\n=== BOOT ===");
    Serial.printf("Wakeup cause: %d  (cold_boot=%d, from_wakeup=%d)\n",
                  (int)cause, cold_boot, from_wakeup);
    Serial.printf("[BOOT] last_screen_id = %u\n", last_screen_id);

    // Wakeup-Pfad: sofort neu starten, damit Display sauber re-init.
    if (cause == ESP_SLEEP_WAKEUP_GPIO || cause == ESP_SLEEP_WAKEUP_EXT1) {
        boot_magic = BOOT_MAGIC_FROM_WAKEUP;
        Serial.println("Wakeup -> esp_restart()");
        Serial.flush();
        esp_restart();
    }

    // Ab hier: normaler Durchlauf (Kaltstart ODER Neustart nach esp_restart)
    gpio_hold_dis(GPIO_NUM_15);

    lcd_lvgl_Init();
    setUpdutySubdivide(LCD_PWM_MODE, LCD_PWM_MODE_0);   // Backlight bleibt aus

    ir_bsp_init();
    battery_icon_create(lv_layer_top());

    // QMI8658 in Accel-Modus fuer Bewegungs-Erkennung im Wachzustand.
    imuReady = qmi_configure_accel();
    if (!imuReady) {
        Serial.println("[QMI] Accel-Konfiguration fehlgeschlagen!");
    }
    have_last_accel = false;

    // -----------------------------------------------------------------
    // Screen-Wiederherstellung
    // -----------------------------------------------------------------
    if (from_wakeup && last_screen_id < DEVICE_COUNT_HINT) {
        Serial.printf("[WAKE] Stelle Screen-ID %u wieder her\n", last_screen_id);
        ui_remote_show_device(last_screen_id);
    } else {
        Serial.println("[BOOT] Kein Wakeup -> Hauptmenue");
        ui_remote_create();
    }

    // UI steht - jetzt erst Backlight an.
    setUpdutySubdivide(LCD_PWM_MODE, LCD_PWM_MODE_100);

    boot_magic = 0;

    Serial.println("[MAIN] Setup fertig.");
}

// =====================================================================
// loop()
// =====================================================================
void loop()
{
    // Idle-Timeout: Sleep, wenn weder Touch noch Bewegung.
    if (lv_disp_get_inactive_time(NULL) > IDLE_TIMEOUT_MS) {
        Serial.println("[IDLE] Timeout erreicht -> Sleep");
        go_to_sleep();
    }

    // Bewegungs-Erkennung alle MOTION_CHECK_INTERVAL_MS.
    // Bei Bewegung: Idle-Timer zuruecksetzen (wie ein Touch).
    static unsigned long last_motion_check = 0;
    if (imuReady && millis() - last_motion_check >= MOTION_CHECK_INTERVAL_MS) {
        last_motion_check = millis();
        if (imu_motion_detected()) {
            reset_idle_timer();
        }
    }

    // DEBUG: einmal pro Sekunde Idle-Wert, Motion-Check-Zeit und die
    // rohen Accel-Werte ausgeben. Zum Debuggen der Bewegungs-Erkennung.
    // Nach dem Testen entfernen!
    static unsigned long last_debug = 0;
    if (millis() - last_debug >= 1000) {
        last_debug = millis();
        uint8_t status0 = 0;
        qmi_read_reg(REG_STATUS0, &status0);
        Serial.printf("[DEBUG] idle=%lu  millis=%lu  ax=%d ay=%d az=%d  STATUS0=0x%02X\n",
                      (unsigned long)lv_disp_get_inactive_time(NULL),
                      (unsigned long)millis(),
                      last_ax, last_ay, last_az, status0);
    }

    battery_icon_task_maybe(BATT_CHECK_MS);

    delay(20);
}
