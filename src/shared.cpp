#include "shared.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char* TAG = "shared";

// ---------------------------------------------------------------------------
// Mutex
// ---------------------------------------------------------------------------
SemaphoreHandle_t state_mutex;

// ---------------------------------------------------------------------------
// Web-settable shared state
// ---------------------------------------------------------------------------
State g_state = {
    .offset_tenths = 0,
    .switch_mode   = false,
    .teeth_total   = DEFAULT_TEETH_TOTAL,
};

// ---------------------------------------------------------------------------
// ISR-written volatile state
// ---------------------------------------------------------------------------
volatile uint32_t    g_avg_tooth_period_us = 0;
volatile bool        g_synced_isr          = false;
volatile uint32_t    g_isr_count           = 0;
volatile SyncState_t g_sync_state          = SYNC_SEARCHING;
volatile uint8_t     g_teeth_counted       = 0;
volatile bool        g_adv_clipped         = false;
volatile bool        g_nvm_dirty           = false;

// ---------------------------------------------------------------------------
// NVS — namespace "xj40"
// ---------------------------------------------------------------------------
#define NVS_NS "xj40"

void nvm_load(void) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "NVS: no saved settings — using defaults");
        return;
    }
    int16_t off = 0;
    uint8_t teeth = DEFAULT_TEETH_TOTAL;
    uint8_t sw = 0;
    nvs_get_i16(h, "offset", &off);
    nvs_get_u8(h, "teeth",  &teeth);
    nvs_get_u8(h, "sw_mode", &sw);
    nvs_close(h);

    if (off < -100) off = -100;
    if (off >  100) off =  100;
    if (teeth < 8)  teeth = 8;
    if (teeth > 60) teeth = 60;

    g_state.offset_tenths = off;
    g_state.teeth_total   = teeth;
    g_state.switch_mode   = (sw != 0);

    ESP_LOGI(TAG, "NVS: loaded offset=%d teeth=%u sw_mode=%s",
             off, teeth, sw ? "on" : "off");
}

void nvm_save(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i16(h, "offset",  g_state.offset_tenths);
    nvs_set_u8(h,  "teeth",   g_state.teeth_total);
    nvs_set_u8(h,  "sw_mode", g_state.switch_mode ? 1 : 0);
    nvs_commit(h);
    nvs_close(h);
    g_nvm_dirty = false;
    ESP_LOGI(TAG, "NVS: saved");
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
void shared_init(void) {
    state_mutex = xSemaphoreCreateMutex();
}

// ---------------------------------------------------------------------------
// Mutex-safe accessors
// ---------------------------------------------------------------------------
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
    g_nvm_dirty = true;
}

uint32_t get_rpm(void) {
    uint32_t period = g_avg_tooth_period_us;
    if (!g_synced_isr || period == 0) return 0;
    return (uint32_t)(60000000ULL / ((uint64_t)period * g_state.teeth_total));
}

bool get_synced(void) {
    return g_synced_isr;
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
    g_nvm_dirty = true;
}

uint8_t get_teeth_total(void) {
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    uint8_t v = g_state.teeth_total;
    xSemaphoreGive(state_mutex);
    return v;
}

void set_teeth_total(uint8_t val) {
    if (val < 8)  val = 8;
    if (val > 60) val = 60;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    g_state.teeth_total = val;
    xSemaphoreGive(state_mutex);
    g_nvm_dirty = true;
}
