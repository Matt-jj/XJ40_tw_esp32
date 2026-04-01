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
- Timing offset slider (-10° to +10°)
- Live RPM + sync status (polls /api/status every 1s)
- Switch mode toggle, teeth count config

## API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/` | Web UI |
| GET | `/api/status` | JSON: rpm, synced, offset_tenths, switch_mode, teeth |
| POST | `/api/offset?value=X` | Set offset (X = tenths, -100..+100) |
| POST | `/api/config?switch_mode=0|1&teeth=N` | Set config |

## Build & Flash

```bash
pio run                       # build
pio run --target upload       # build and flash
pio device monitor            # serial debug at 115200
```

## TODO

- [x] **Timing ISR** — ported from Pico project, flashed and producing output
- [ ] **Verify advance/retard offsets via logic analyser** — NEXT SESSION
  - CSV capture `XJ40_esp32_01-04-26_sesh1` taken at 100kHz, 5° advance set on slider
  - Analysis was interrupted: raw data around gap showed CH1 (TW) and CH2 (PB) both LOW simultaneously, suggesting signal polarity/phasing needs closer inspection
  - Need to identify: which edge of CH1 is the tooth leading edge relative to CH2
  - Approach: re-capture at **500kHz or 1MHz**, just 2-3 revolutions, to get ~2µs resolution and clearly resolve the advance delay (~695µs at 1200RPM)
  - Also check: gap tooth passthrough behaviour (PB was +1410µs late on gap tooth — investigate if expected or a problem)
- [ ] **Strip unused libraries** — once code is finalised, disable unused ESP-IDF components via `sdkconfig.defaults` to reduce flash usage and eliminate potential instability from unused code. Key candidates:
  - `CONFIG_BT_ENABLED=n` — Bluetooth (~80KB+)
  - `CONFIG_MBEDTLS_*` — TLS/SSL (~100KB, not needed for plain HTTP)
  - `CONFIG_ESP_COREDUMP_ENABLE=n` — core dump (useful in dev, remove for release)
  - Log level → `CONFIG_LOG_DEFAULT_LEVEL_WARN=y` for release
  - Run `pio run --target menuconfig` for interactive config browser
- [ ] **OTA updates** — add dual OTA partition table before final installation (unit will be buried under dashboard). Size partitions based on final firmware size.

## Key files

- [src/main.cpp](src/main.cpp) — app_main, task creation, deferred NVM save
- [src/piggyback.h](src/piggyback.h) / [src/piggyback.cpp](src/piggyback.cpp) — trigger wheel ISR, advance/retard timing
- [src/shared.h](src/shared.h) / [src/shared.cpp](src/shared.cpp) — shared state, mutex, accessors
- [src/web.h](src/web.h) / [src/web.cpp](src/web.cpp) — WiFi AP, DNS, HTTP server
- [src/web_ui.h](src/web_ui.h) — embedded HTML/JS
- [platformio.ini](platformio.ini) — build config
- [sdkconfig.defaults](sdkconfig.defaults) — ESP-IDF config overrides
