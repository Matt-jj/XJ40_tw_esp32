#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdint.h>
#include <stdbool.h>

#define FIRMWARE_VERSION  "0.1.0"
#define AP_SSID           "XJ40-Trigger"
#define AP_IP             "192.168.4.1"

// Pin assignments — adjust to match final hardware
#define PIN_TRIGGER_IN    4
#define PIN_TRIGGER_OUT   5
#define PIN_ENABLE        6    // active-low switch input

extern SemaphoreHandle_t state_mutex;

// Shared state between Core 0 (web) and Core 1 (ISR)
typedef struct {
    int16_t  offset_tenths;   // -100..+100 (tenths of a degree)
    uint32_t rpm;
    bool     synced;
    bool     switch_mode;     // true = PIN_ENABLE controls offset bypass
    uint8_t  teeth_total;     // default 36
} State;

extern State g_state;

void shared_init(void);

// Mutex-safe accessors
int16_t  get_offset_tenths(void);
void     set_offset_tenths(int16_t val);
uint32_t get_rpm(void);
bool     get_synced(void);
bool     get_switch_mode(void);
void     set_switch_mode(bool val);
uint8_t  get_teeth_total(void);
void     set_teeth_total(uint8_t val);
