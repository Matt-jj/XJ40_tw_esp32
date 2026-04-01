#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "shared.h"
#include "web.h"

static const char* TAG = "main";

// ---------------------------------------------------------------------------
// Core 1 — timing ISR (stub, to be implemented)
// ---------------------------------------------------------------------------

static void timing_task(void* arg) {
    ESP_LOGI(TAG, "Timing task started on core %d", xPortGetCoreID());
    // TODO: configure GPIO interrupts, hardware timers, trigger wheel ISR
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

extern "C" void app_main(void) {
    shared_init();

    // Core 0 — WiFi AP, DNS, HTTP server
    xTaskCreatePinnedToCore(web_task,    "web",    8192, NULL,  5, NULL, 0);

    // Core 1 — timing ISR (stub for now)
    xTaskCreatePinnedToCore(timing_task, "timing", 4096, NULL, 10, NULL, 1);
}
