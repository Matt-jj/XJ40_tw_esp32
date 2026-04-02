#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "shared.h"
#include "web.h"
#include "piggyback.h"

static const char* TAG = "main";

// ---------------------------------------------------------------------------
// Core 1 — timing ISR
// ---------------------------------------------------------------------------
static void timing_task(void* arg) {
    ESP_LOGI(TAG, "Timing task started on core %d", xPortGetCoreID());
    piggyback_setup();
    // ISR handles everything — task just watches for deferred NVM save.
    // Only save when not synced (no active signal) to avoid interfering
    // with flash erase timing. ESP-IDF NVS is thread-safe but flash ops
    // can add microseconds of latency — safer to do it between teeth.
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(250));

        // Stale sync reset: if no gap seen in 2s, declare engine stopped
        if (g_synced_isr) {
            const uint32_t elapsed = (uint32_t)esp_timer_get_time() - g_last_mt_us;
            if (elapsed > 2000000UL) {
                g_synced_isr        = false;
                g_avg_rev_period_us = 0;
                g_sync_state        = SYNC_SEARCHING;
            }
        }

        if (g_nvm_dirty && !g_synced_isr) {
            nvm_save();
        }
    }
}

// ---------------------------------------------------------------------------
// Entry point (Core 0)
// ---------------------------------------------------------------------------
extern "C" void app_main(void) {
    nvs_flash_init();
    shared_init();
    nvm_load();

    // Core 0 — WiFi AP, DNS, HTTP server
    xTaskCreatePinnedToCore(web_task,    "web",    8192, NULL,  5, NULL, 0);

    // Core 1 — timing ISR
    xTaskCreatePinnedToCore(timing_task, "timing", 4096, NULL, 10, NULL, 1);
}
