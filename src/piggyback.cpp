// piggyback.cpp — Trigger wheel ISR and timing offset processing (Core 1)
//
// Per-edge architecture (all integer arithmetic, no floats):
//   Each edge-to-edge interval is 360/(teeth*2) degrees (5 deg for 36-tooth).
//   hi_int = HIGH phase, lo_int = LOW phase — tracked independently.
//
//   1. Find missing tooth: LOW duration >= 2.5x previous LOW
//   2. Once MT found: count teeth, predict using per-edge windows
//   3. Advance (offset > 0): ONE prediction per tooth — schedule PB HIGH
//      early via timer. Falling edge is REACTIVE (PB LOW when TW goes LOW).
//   4. Retard (offset < 0): fully reactive — delay both edges by CD us.
//
//   CD = offset_tenths x tooth_period / 100
//
// RPM ramp: offset scales linearly from 0% at 1000 RPM to 100% at 1200 RPM
//   (averaged over 5 revolutions for stability).
// Slew: offset transitions at 0.5 deg/rev (5 tenths/rev).

#include "piggyback.h"
#include "shared.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "esp_log.h"

static const char* TAG = "piggyback";

// ---------------------------------------------------------------------------
// Timers — one-shot, created once at setup, re-armed on demand.
// Four timers: advance_rise, advance_fall, retard_rise, retard_fall.
// esp_timer functions are thread/ISR-safe in ESP-IDF 5.x.
// ---------------------------------------------------------------------------
static esp_timer_handle_t g_tmr_adv_rise  = nullptr;
static esp_timer_handle_t g_tmr_adv_fall  = nullptr;
static esp_timer_handle_t g_tmr_ret_rise  = nullptr;
static esp_timer_handle_t g_tmr_ret_fall  = nullptr;

static volatile bool     g_adv_fired = false;
static volatile uint32_t g_adv_pw    = 0;      // predicted HIGH duration (us)

// Advance rise: PB HIGH early, then schedule advance fall at predicted pulse width
void IRAM_ATTR cb_adv_rise(void*) {
    gpio_set_level((gpio_num_t)PIN_TRIGGER_OUT, 1);
    g_adv_fired = true;
    esp_timer_stop(g_tmr_adv_fall);
    esp_timer_start_once(g_tmr_adv_fall, g_adv_pw);
}

// Advance fall: PB LOW at predicted pulse end
void IRAM_ATTR cb_adv_fall(void*) {
    gpio_set_level((gpio_num_t)PIN_TRIGGER_OUT, 0);
}

// Retard rise: PB HIGH delayed
void IRAM_ATTR cb_ret_rise(void*) {
    gpio_set_level((gpio_num_t)PIN_TRIGGER_OUT, 1);
}

// Retard fall: PB LOW delayed
void IRAM_ATTR cb_ret_fall(void*) {
    gpio_set_level((gpio_num_t)PIN_TRIGGER_OUT, 0);
}

static bool s_timers_active = false;  // guard: only stop timers if actually armed

static inline void IRAM_ATTR stop_all_timers() {
    if (!s_timers_active) return;
    esp_timer_stop(g_tmr_adv_rise);
    esp_timer_stop(g_tmr_adv_fall);
    esp_timer_stop(g_tmr_ret_rise);
    esp_timer_stop(g_tmr_ret_fall);
    s_timers_active = false;
}

// ---------------------------------------------------------------------------
// ISR state (Core 1 only — no mutex needed)
// ---------------------------------------------------------------------------
static uint32_t hi_int[2]    = {0, 0};
static uint32_t lo_int[2]    = {0, 0};
static uint32_t s_last_rise  = 0;
static uint32_t s_last_fall  = 0;
static bool     s_first_edge = true;
static bool     mt_found     = false;
static uint8_t  tooth_index  = 0;
static bool     s_adv_active = false;   // true if this tooth's PB was driven by advance timer

// 5-revolution average RPM
static constexpr uint8_t  REV_BUF_N    = 6;
static uint32_t rev_ts[REV_BUF_N]      = {};
static uint8_t  rev_idx                = 0;
static uint8_t  rev_count              = 0;
// g_avg_rev_period_us (shared.cpp) — no local copy needed

// Slew: smoothly transition offset at 0.5 deg/rev
static constexpr int16_t  SLEW_STEP    = 5;
static int16_t s_offset_current        = 0;

// ---------------------------------------------------------------------------
// ISR — fires on both edges of PIN_TRIGGER_IN
// ---------------------------------------------------------------------------
static void IRAM_ATTR trigger_isr(void*) {
    g_isr_count = g_isr_count + 1;  // C++20: ++ on volatile is deprecated (misleading atomicity)
    const uint32_t now_us = (uint32_t)esp_timer_get_time();
    const bool     rising = gpio_get_level((gpio_num_t)PIN_TRIGGER_IN);

    if (!rising) {
        // -- FALLING EDGE ------------------------------------------------
        if (s_last_rise > 0) {
            const uint32_t hi = now_us - s_last_rise;
            hi_int[1] = hi_int[0];
            hi_int[0] = hi;
        }
        s_last_fall = now_us;

        const int16_t off = s_offset_current;

        if (s_adv_active) {
            // Advance fall already scheduled by cb_adv_rise — do nothing
        } else if (mt_found && off < 0) {
            // Retard falling: delay PB LOW by CD us
            const uint32_t pred_hi   = hi_int[1] > 0 ? hi_int[1] : hi_int[0];
            const uint32_t pred_lo   = lo_int[1] > 0 ? lo_int[1] : lo_int[0];
            const uint32_t tooth_per = pred_hi + pred_lo;
            const uint32_t cd = (uint32_t)(-off) * tooth_per / 100;
            esp_timer_stop(g_tmr_ret_fall);
            esp_timer_start_once(g_tmr_ret_fall, cd);
            s_timers_active = true;
        } else {
            // Passthrough
            gpio_set_level((gpio_num_t)PIN_TRIGGER_OUT, 0);
        }
        return;
    }

    // -- RISING EDGE -----------------------------------------------------
    if (s_first_edge) {
        s_first_edge = false;
        s_last_rise  = now_us;
        gpio_set_level((gpio_num_t)PIN_TRIGGER_OUT, 1);
        s_adv_active = false;
        return;
    }

    const uint32_t period  = now_us - s_last_rise;
    const uint32_t low_dur = (s_last_fall > s_last_rise)
                             ? (now_us - s_last_fall) : 0;
    s_last_rise = now_us;

    lo_int[1] = lo_int[0];
    lo_int[0] = low_dur;

    if (lo_int[1] == 0) {
        gpio_set_level((gpio_num_t)PIN_TRIGGER_OUT, 1);
        s_adv_active = false;
        return;
    }

    // -- MT DETECTION ----------------------------------------------------
    // Gap LOW ~ 3x normal LOW. Threshold 2.5x: low_dur * 2 > lo_int[1] * 5
    const bool is_mt = (low_dur * 2 > lo_int[1] * 5);

    if (is_mt) {
        // Auto-detect: tooth_index before reset = normal teeth counted last revolution
        if (mt_found) {
            static uint8_t s_auto_candidate = 0;
            static uint8_t s_auto_tally     = 0;
            const uint8_t candidate = tooth_index + TEETH_MISSING + 1; // +1: gap-detection tooth is real but not counted in tooth_index
            if (candidate >= 8 && candidate <= 60) {
                if (candidate == s_auto_candidate) {
                    if (++s_auto_tally >= 10) {
                        g_teeth_auto      = candidate;
                        g_teeth_confirmed = true;
                        // Persist to NVS if not in manual override mode
                        if (!g_state.teeth_manual) {
                            g_state.teeth_total = candidate;
                            g_nvm_dirty = true;
                        }
                    }
                } else {
                    s_auto_candidate = candidate;
                    s_auto_tally     = 1;
                }
            }
        }
        mt_found      = true;
        tooth_index   = 0;
        g_last_mt_us  = now_us;
        // Normalise gap LOW: gap_low = 2×normal_lo + normal_hi
        // => normal_lo = (gap_low - last_hi) / 2
        lo_int[0]             = (low_dur - hi_int[0]) / 2;
        g_avg_tooth_period_us = period / (TEETH_MISSING + 1);
        g_sync_state          = SYNC_SYNCED;
        g_synced_isr          = true;
        g_teeth_counted       = 0;

        // Record revolution timestamp
        rev_ts[rev_idx] = now_us;
        rev_idx = (rev_idx + 1) % REV_BUF_N;
        if (rev_count < REV_BUF_N) rev_count++;
        if (rev_count >= 2) {
            uint8_t oldest      = (rev_count < REV_BUF_N) ? 0 : rev_idx;
            uint8_t newest      = (rev_idx + REV_BUF_N - 1) % REV_BUF_N;
            uint8_t n_intervals = (rev_count < REV_BUF_N) ? (rev_count - 1) : (REV_BUF_N - 1);
            g_avg_rev_period_us   = (rev_ts[newest] - rev_ts[oldest]) / n_intervals;
        }

        // Slew offset toward target once per revolution
        const int16_t target = isr_get_offset_tenths();
        if (s_offset_current < target) {
            s_offset_current += SLEW_STEP;
            if (s_offset_current > target) s_offset_current = target;
        } else if (s_offset_current > target) {
            s_offset_current -= SLEW_STEP;
            if (s_offset_current < target) s_offset_current = target;
        }

    } else {
        // Normal tooth
        g_avg_tooth_period_us = period;
        if (mt_found) {
            tooth_index++;
            g_teeth_counted = tooth_index;
        }
    }

    // -- OFFSET & OUTPUT -------------------------------------------------
    const int16_t off = s_offset_current;

    // RPM ramp: 0% at 1000 RPM (60000 us/rev), 100% at 1200 RPM (50000 us/rev)
    static constexpr uint32_t RPM_LO_PER = 60000;
    static constexpr uint32_t RPM_HI_PER = 50000;

    const bool teeth_ok = g_state.teeth_manual || g_teeth_confirmed;
    if (!mt_found || off == 0 || g_avg_rev_period_us == 0
        || g_avg_rev_period_us > RPM_LO_PER || !teeth_ok) {
        stop_all_timers();
        g_adv_fired = false;
        gpio_set_level((gpio_num_t)PIN_TRIGGER_OUT, 1);
        s_adv_active = false;
        return;
    }

    const uint32_t pred_hi   = hi_int[1] > 0 ? hi_int[1] : hi_int[0];
    const uint32_t pred_lo   = lo_int[1] > 0 ? lo_int[1] : lo_int[0];
    const uint32_t tooth_per = pred_hi + pred_lo;

    if (tooth_per < 20) {
        gpio_set_level((gpio_num_t)PIN_TRIGGER_OUT, 1);
        s_adv_active = false;
        return;
    }

    uint32_t cd = (uint32_t)(off > 0 ? off : -off) * tooth_per / 100;

    // Scale cd linearly between 1000–1200 RPM
    if (g_avg_rev_period_us > RPM_HI_PER) {
        uint32_t scale = (RPM_LO_PER - g_avg_rev_period_us) * 100
                         / (RPM_LO_PER - RPM_HI_PER);
        cd = cd * scale / 100;
    }

    if (off > 0) {
        // -- ADVANCE -----------------------------------------------------
        if (g_adv_fired) {
            g_adv_fired  = false;
            s_adv_active = true;
        } else {
            gpio_set_level((gpio_num_t)PIN_TRIGGER_OUT, 1);
            s_adv_active = false;
        }

        const uint8_t  teeth_present = isr_get_teeth_total() - TEETH_MISSING;
        const bool     next_is_gap   = (tooth_index == (uint8_t)(teeth_present - 1));
        const uint32_t next_per      = next_is_gap
                                       ? (tooth_per * (TEETH_MISSING + 1))
                                       : tooth_per;
        if (cd > next_per) {
            g_adv_clipped = true;
            return;
        }
        g_adv_clipped = false;
        g_adv_pw      = pred_hi;

        esp_timer_stop(g_tmr_adv_rise);
        esp_timer_start_once(g_tmr_adv_rise, next_per - cd);
        s_timers_active = true;

    } else {
        // -- RETARD ------------------------------------------------------
        esp_timer_stop(g_tmr_ret_rise);
        esp_timer_start_once(g_tmr_ret_rise, cd);
        s_timers_active = true;
        s_adv_active = false;
    }
}

// ---------------------------------------------------------------------------
// Setup — called from timing_task on Core 1
// ---------------------------------------------------------------------------
void piggyback_setup(void) {
    // Output pin
    gpio_config_t out_cfg = {};
    out_cfg.pin_bit_mask = (1ULL << PIN_TRIGGER_OUT);
    out_cfg.mode         = GPIO_MODE_OUTPUT;
    gpio_config(&out_cfg);
    gpio_set_level((gpio_num_t)PIN_TRIGGER_OUT, 0);

    // Enable pin (active-low, internal pull-up)
    gpio_config_t en_cfg = {};
    en_cfg.pin_bit_mask = (1ULL << PIN_ENABLE);
    en_cfg.mode         = GPIO_MODE_INPUT;
    en_cfg.pull_up_en   = GPIO_PULLUP_ENABLE;
    gpio_config(&en_cfg);

    // Input pin
    gpio_config_t in_cfg = {};
    in_cfg.pin_bit_mask = (1ULL << PIN_TRIGGER_IN);
    in_cfg.mode         = GPIO_MODE_INPUT;
    in_cfg.intr_type    = GPIO_INTR_ANYEDGE;
    gpio_config(&in_cfg);

    // Create one-shot timers
    const esp_timer_create_args_t tmr_defs[4] = {
        { cb_adv_rise, nullptr, ESP_TIMER_ISR, "pb_adv_rise", false },
        { cb_adv_fall, nullptr, ESP_TIMER_ISR, "pb_adv_fall", false },
        { cb_ret_rise, nullptr, ESP_TIMER_ISR, "pb_ret_rise", false },
        { cb_ret_fall, nullptr, ESP_TIMER_ISR, "pb_ret_fall", false },
    };
    esp_timer_create(&tmr_defs[0], &g_tmr_adv_rise);
    esp_timer_create(&tmr_defs[1], &g_tmr_adv_fall);
    esp_timer_create(&tmr_defs[2], &g_tmr_ret_rise);
    esp_timer_create(&tmr_defs[3], &g_tmr_ret_fall);

    // Install GPIO ISR service and attach handler
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add((gpio_num_t)PIN_TRIGGER_IN, trigger_isr, nullptr);

    ESP_LOGI(TAG, "Piggyback ISR ready  GPIO%d -> GPIO%d  enable=GPIO%d",
             PIN_TRIGGER_IN, PIN_TRIGGER_OUT, PIN_ENABLE);
}
