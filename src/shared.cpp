#include "shared.h"

SemaphoreHandle_t state_mutex;

State g_state = {
    .offset_tenths = 0,
    .rpm           = 0,
    .synced        = false,
    .switch_mode   = false,
    .teeth_total   = 36,
};

void shared_init(void) {
    state_mutex = xSemaphoreCreateMutex();
}

int16_t get_offset_tenths(void) {
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    int16_t v = g_state.offset_tenths;
    xSemaphoreGive(state_mutex);
    return v;
}

void set_offset_tenths(int16_t val) {
    if (val < -100) val = -100;
    if (val >  100) val =  100;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    g_state.offset_tenths = val;
    xSemaphoreGive(state_mutex);
}

uint32_t get_rpm(void) {
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    uint32_t v = g_state.rpm;
    xSemaphoreGive(state_mutex);
    return v;
}

bool get_synced(void) {
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    bool v = g_state.synced;
    xSemaphoreGive(state_mutex);
    return v;
}

bool get_switch_mode(void) {
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    bool v = g_state.switch_mode;
    xSemaphoreGive(state_mutex);
    return v;
}

void set_switch_mode(bool val) {
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    g_state.switch_mode = val;
    xSemaphoreGive(state_mutex);
}

uint8_t get_teeth_total(void) {
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    uint8_t v = g_state.teeth_total;
    xSemaphoreGive(state_mutex);
    return v;
}

void set_teeth_total(uint8_t val) {
    if (val < 2) val = 2;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    g_state.teeth_total = val;
    xSemaphoreGive(state_mutex);
}
