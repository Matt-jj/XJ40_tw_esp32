// status_led.cpp — WS2812 status LED via RMT (Core 0, non-timing-critical)
//
// States:
//   Not synced          : blue slow blink (1Hz)
//   Synced, offset = 0  : blue solid
//   Synced, advance > 0 : green solid
//   Synced, retard  < 0 : red solid
//
// RMT runs entirely in hardware — zero interaction with timing ISR on Core 1.

#include "status_led.h"
#include "shared.h"
#include "led_strip_encoder.h"
#include "driver/rmt_tx.h"
#include "soc/gpio_reg.h"

#define LED_RMT_RESOLUTION_HZ  10000000   // 10MHz — 100ns per tick
#define LED_BRIGHTNESS         20          // 0–255: keep dim for in-car use

static rmt_channel_handle_t s_led_chan    = nullptr;
static rmt_encoder_handle_t s_led_encoder = nullptr;

// Send one GRB pixel. WS2812 wire order is G, R, B.
static void set_led(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t grb[3] = { g, r, b };
    rmt_transmit_config_t tx_cfg = {};
    tx_cfg.loop_count = 0;
    rmt_transmit(s_led_chan, s_led_encoder, grb, sizeof(grb), &tx_cfg);
    rmt_tx_wait_all_done(s_led_chan, 10);  // <1ms for single LED — safe in 250ms loop
}

void status_led_init(void)
{
    rmt_tx_channel_config_t chan_cfg = {};
    chan_cfg.gpio_num          = (gpio_num_t)PIN_STATUS_LED;
    chan_cfg.clk_src           = RMT_CLK_SRC_DEFAULT;
    chan_cfg.resolution_hz     = LED_RMT_RESOLUTION_HZ;
    chan_cfg.mem_block_symbols = 64;
    chan_cfg.trans_queue_depth = 4;
    rmt_new_tx_channel(&chan_cfg, &s_led_chan);

    led_strip_encoder_config_t enc_cfg = {};
    enc_cfg.resolution = LED_RMT_RESOLUTION_HZ;
    rmt_new_led_strip_encoder(&enc_cfg, &s_led_encoder);

    rmt_enable(s_led_chan);
    set_led(0, 0, 0);  // off on startup until first update
}

void status_led_update(void)
{
    // Blink counter: called every 250ms → toggle every 2 calls = 1Hz blink
    static uint8_t s_blink = 0;
    s_blink = (s_blink + 1) % 4;

    const bool  synced = g_synced_isr;
    int16_t     off    = g_state.offset_tenths;

    // Switch mode bypass: if enable pin is high, offset is suppressed at the ISR
    if (g_state.switch_mode && ((REG_READ(GPIO_IN_REG) >> PIN_ENABLE) & 1)) {
        off = 0;
    }

    const uint8_t B = LED_BRIGHTNESS;

    if (!synced) {
        // Blue blink 1Hz
        set_led(0, 0, (s_blink < 2) ? B : 0);
    } else if (off > 0) {
        set_led(0, B, 0);   // green — advance
    } else if (off < 0) {
        set_led(B, 0, 0);   // red — retard
    } else {
        set_led(0, 0, B);   // blue solid — synced, no offset
    }
}
