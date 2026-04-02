# XJ40_tw_esp32

ESP32-S3 port of the XJ40 ignition timing modifier.
ESP-IDF framework via PlatformIO.

## Architecture

- **Core 0** — WiFi AP, DNS captive portal, HTTP server (`web_task`)
- **Core 1** — timing ISR, trigger wheel processing (`piggyback.cpp`)
- Shared state protected by FreeRTOS `SemaphoreHandle_t state_mutex`

## Hardware (provisional — pins TBC)

- **Trigger input:**  GPIO4 (PIN_TRIGGER_IN)
- **Trigger output:** GPIO5 (PIN_TRIGGER_OUT)
- **Enable pin:**     GPIO6 (PIN_ENABLE) — active-low

## Web UI

- Captive portal at 192.168.4.1 (SSID: XJ40-Trigger, open)
- Timing offset slider (-10° to +10°) with Apply button
- Live RPM + sync status (polls /api/status every 1s)
- Switch mode toggle
- Trigger wheel teeth: auto-detected (10-rev confirmation, saved to NVM), manual override checkbox (engine stopped only)

## API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/` | Web UI |
| GET | `/api/status` | JSON: rpm, synced, offset_tenths, switch_mode, teeth, teeth_auto, teeth_manual, teeth_confirmed |
| POST | `/api/offset?value=X` | Set offset (X = tenths, -100..+100) |
| POST | `/api/config?switch_mode=0\|1&teeth=N&teeth_manual=0\|1` | Set config |

## Build & Flash

```bash
pio run                       # build
pio run --target upload       # build and flash
pio device monitor            # serial debug at 115200
```

## Session notes — 02-04-2026

### Logic analyser analysis (captured_glitch_02-04-2026_sesh1, 200kHz)

- **5° advance confirmed correct:** measured 533µs early at 1542 RPM = 4.93°. Shortfall from expected 540µs is quantization + esp_timer latency, within tolerance.
- **Glitch root cause identified:** `ESP_TIMER_TASK` dispatch runs on Core 0 (same as WiFi). AP beacon every ~100ms caused timer task delays of 600–830µs, causing advance rise timer to fire after the next tooth had already started → 1–2 desynchronised teeth.
- **Fix applied:** Changed all 4 advance/retard timer callbacks to `ESP_TIMER_ISR` dispatch + `IRAM_ATTR`. Added `CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD=y` to sdkconfig. Timer callbacks now run directly from hardware timer ISR, immune to WiFi task scheduling. Worst-case latency < 10µs (= 0.30° at 5000 RPM on a 60-tooth wheel).

### Features added this session

- **Auto-detect teeth count** — ISR tallies `tooth_index + TEETH_MISSING + 1` at each gap, confirms after 10 consistent revolutions. Saves to NVM on confirmation (if manual override not active).
- **Gap-based RPM** — `g_avg_rev_period_us` computed from gap-to-gap timestamps. `get_rpm()` uses this directly, independent of teeth_total. Breaks the circular dependency between RPM and teeth_total.
- **RPM stale detection** — `get_rpm()` returns 0 if no gap seen in > 2 rev periods. `timing_task` fully resets sync state after 2s of no gaps (re-arms ISR for re-sync). NVM deferred save now works correctly when engine stops.
- **Manual teeth override** — checkbox (default OFF) enables manual teeth entry via number input. Only editable at RPM=0. Checkbox state and manual value both saved to NVM.
- **Offset Apply button** — removed auto-send on slider change. Display goes orange (pending), Apply button commits. Poll won't overwrite slider while pending.

## TODO

- [ ] **Re-capture with logic analyser** — confirm glitch is gone after ESP_TIMER_ISR fix. Use 500kHz+ for better timing resolution. Test at higher RPM if possible.
- [ ] **Verify retard mode** — retard not yet confirmed working via capture. Test at 5° retard.
- [ ] **Confirm pin assignments** — GPIO4/5/6 marked provisional. Verify against hardware schematic before final installation.
- [ ] **Strip unused libraries** — once code is finalised, disable unused ESP-IDF components via `sdkconfig.defaults` to reduce flash usage and eliminate potential instability from unused code. Key candidates:
  - `CONFIG_BT_ENABLED=n` — Bluetooth (~80KB+)
  - `CONFIG_MBEDTLS_*` — TLS/SSL (~100KB, not needed for plain HTTP)
  - `CONFIG_ESP_COREDUMP_ENABLE=n` — core dump (useful in dev, remove for release)
  - Log level → `CONFIG_LOG_DEFAULT_LEVEL_WARN=y` for release
  - Run `pio run --target menuconfig` for interactive config browser
- [ ] **OTA updates** — add dual OTA partition table before final installation (unit will be buried under dashboard). Size partitions based on final firmware size.

## Key files

- [src/main.cpp](src/main.cpp) — app_main, task creation, stale sync reset, deferred NVM save
- [src/piggyback.h](src/piggyback.h) / [src/piggyback.cpp](src/piggyback.cpp) — trigger wheel ISR, advance/retard timing, auto-detect
- [src/shared.h](src/shared.h) / [src/shared.cpp](src/shared.cpp) — shared state, mutex, accessors, gap-based RPM
- [src/web.h](src/web.h) / [src/web.cpp](src/web.cpp) — WiFi AP, DNS, HTTP server
- [src/web_ui.h](src/web_ui.h) — embedded HTML/JS
- [platformio.ini](platformio.ini) — build config
- [sdkconfig.defaults](sdkconfig.defaults) — ESP-IDF config overrides
