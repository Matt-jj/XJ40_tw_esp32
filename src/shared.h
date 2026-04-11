#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "soc/gpio_reg.h"
#include <stdint.h>
#include <stdbool.h>

#define FIRMWARE_VERSION     "0.1.0"
#define AP_SSID              "XJ40-Trigger"
#define AP_IP                "192.168.4.1"

// Pin assignments
#define PIN_TRIGGER_IN       4
#define PIN_TRIGGER_OUT      5
#define PIN_ENABLE           6   // active-low switch input (pull-up enabled)
                                 // LOW  = offset active, HIGH = bypass
#define PIN_STATUS_LED      21   // WS2812 data — GPIO21 (ESP32-S3-Zero, verify if clone differs)

// Trigger wheel defaults
#define DEFAULT_TEETH_TOTAL  36
#define TEETH_MISSING        1

// ---------------------------------------------------------------------------
// Sync state
// ---------------------------------------------------------------------------
typedef enum {
    SYNC_SEARCHING = 0,
    SYNC_SYNCED    = 1,
} SyncState_t;

// ---------------------------------------------------------------------------
// Web-settable shared state — mutex-protected, read/written by web task
// ISR reads these lock-free (aligned types, atomic on 32-bit Xtensa)
// ---------------------------------------------------------------------------
extern SemaphoreHandle_t state_mutex;

typedef struct {
    int16_t  offset_tenths;   // -100..+100 (tenths of a degree)
    bool     switch_mode;     // true = PIN_ENABLE controls offset bypass
    uint8_t  teeth_total;     // manual override teeth count (incl. missing)
    bool     teeth_manual;    // true = use teeth_total; false = use auto-detected
} State;

extern State g_state;

// ---------------------------------------------------------------------------
// ISR-written volatile state
// Written on Core 1, read on Core 0.  32-bit aligned types are atomic on
// Xtensa — no mutex needed for individual reads.
// ---------------------------------------------------------------------------
extern volatile uint32_t    g_avg_tooth_period_us;  // normalised per-tooth period (us)
extern volatile uint32_t    g_avg_rev_period_us;    // gap-to-gap revolution period (us)
extern volatile uint32_t    g_last_mt_us;           // esp_timer timestamp of last gap (us)
extern volatile bool        g_synced_isr;            // true once MT found and synced
extern volatile uint32_t    g_isr_count;
extern volatile SyncState_t g_sync_state;
extern volatile uint8_t     g_teeth_counted;
extern volatile uint8_t     g_teeth_auto;           // auto-detected teeth count
extern volatile bool        g_teeth_confirmed;       // true once stable for 10 revolutions
extern volatile bool        g_adv_clipped;
extern volatile bool        g_nvm_dirty;

// ---------------------------------------------------------------------------
// Init, NVS
// ---------------------------------------------------------------------------
void shared_init(void);
void nvm_load(void);
void nvm_save(void);

// ---------------------------------------------------------------------------
// Mutex-safe accessors (web task / Core 0)
// ---------------------------------------------------------------------------
int16_t  get_offset_tenths(void);
void     set_offset_tenths(int16_t val);
uint32_t get_rpm(void);
bool     get_synced(void);
bool     get_switch_mode(void);
void     set_switch_mode(bool val);
uint8_t  get_teeth_total(void);
void     set_teeth_total(uint8_t val);
bool     get_teeth_manual(void);
void     set_teeth_manual(bool val);
uint8_t  get_teeth_auto(void);

// ---------------------------------------------------------------------------
// ISR-safe inline accessors (lock-free, Core 1)
// g_state fields are aligned types — atomic reads on 32-bit Xtensa.
// ---------------------------------------------------------------------------
static inline IRAM_ATTR int16_t isr_get_offset_tenths(void) {
    if (g_state.switch_mode && ((REG_READ(GPIO_IN_REG) >> PIN_ENABLE) & 1)) return 0;
    return g_state.offset_tenths;
}

static inline IRAM_ATTR uint8_t isr_get_teeth_total(void) {
    if (!g_state.teeth_manual && g_teeth_confirmed)
        return g_teeth_auto;
    return g_state.teeth_total;
}
