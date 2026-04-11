#pragma once

// Initialise RMT channel and WS2812 encoder. Call once from app_main before tasks start.
void status_led_init(void);

// Update LED colour to reflect current sync/offset state.
// Call every ~250ms from timing_task. Non-blocking — RMT transmits in background.
void status_led_update(void);
