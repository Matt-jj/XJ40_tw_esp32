# XJ40 Trigger — ESP32-S3

Ignition timing modifier for the **Jaguar XJ40 AJ26 4.0** straight-six engine, ported to the **ESP32-S3** using the native **ESP-IDF** framework.

Sits between the crank position sensor and the ECU, reading the 36-1 trigger wheel signal and re-outputting it with a configurable timing offset.

> **Companion project:** The original working implementation runs on a Raspberry Pi Pico 2W and lives at [Matt-jj/xj40_tw](https://github.com/Matt-jj/xj40_tw). That project is the reference for all timing logic.

---

## Status

| Component | Status |
|-----------|--------|
| Project structure | ✅ Done |
| WiFi AP + captive portal | ✅ Done |
| HTTP server | ✅ Done |
| Web UI | ✅ Done |
| Shared state + mutex | ✅ Done |
| Trigger wheel ISR (Core 1) | ⬜ Todo |
| Advance (predictive alarm) | ⬜ Todo |
| Retard (reactive delay) | ⬜ Todo |
| RPM averaging (5-rev) | ⬜ Todo |
| Offset slew | ⬜ Todo |
| NVM persistence | ⬜ Todo |
| Hardware bring-up + pin finalisation | ⬜ Todo |

---

## Architecture

The ESP32-S3 dual-core maps directly to the Pico design:

| Core | Task | Role |
|------|------|------|
| **Core 0** | `web_task` | WiFi AP, DNS captive portal, HTTP server |
| **Core 1** | `timing_task` | Trigger wheel ISR, alarm timers, offset application |

Shared state is protected by a FreeRTOS mutex (`SemaphoreHandle_t`).
One-shot hardware timers via `esp_timer` will replace the Pico's alarm pool.

---

## How it works

```
Hall sensor (5V) ──► Opto-isolator ──► ESP32-S3 GPIO (input)
                                              │
                                        ISR processes
                                        36-1 trigger wheel,
                                        applies timing offset
                                              │
                                   ESP32-S3 GPIO (output) ──► Opto-isolator ──► ECU
```

### Timing offset

- Range: **-10.0° (retard)** to **+10.0° (advance)**, in 0.1° steps
- **Advance** — predictive: fires output edge early via one-shot `esp_timer`
- **Retard** — reactive: delays both edges after trigger wheel edges
- **RPM ramp** — offset scales 0→100% between 1000–1200 RPM (5-revolution average)
- **Slew** — transitions at 0.5°/rev when offset setting changes

---

## Hardware

> Pin assignments are provisional pending hardware bring-up.

| Pin | Function |
|-----|----------|
| GPIO4 | Trigger input (from opto-isolator) |
| GPIO5 | Trigger output (to opto-isolator / ECU) |
| GPIO6 | Enable switch (active-low, internal pull-up) |

- **Trigger wheel:** 36-1 (35 teeth + 1 missing), 10° per tooth
- **Sensor:** Hall effect, 5V output
- **Isolation:** Opto-isolators for 5V ↔ 3.3V level conversion

---

## Web UI

Connect to the **XJ40-Trigger** WiFi network (open, no password).
Any URL redirects to the captive portal at **192.168.4.1**.

- Timing offset slider (-10° to +10°)
- Live RPM display and sync status indicator
- Switch mode toggle (enable pin bypasses offset)
- Teeth count configuration

### API

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/` | Web UI |
| GET | `/api/status` | JSON: `rpm`, `synced`, `offset_tenths`, `switch_mode`, `teeth` |
| POST | `/api/offset?value=X` | Set offset (X = tenths, -100..+100) |
| POST | `/api/config?switch_mode=0\|1&teeth=N` | Update config |

---

## Build & Flash

Requires [PlatformIO](https://platformio.org/) with the Espressif32 platform.

```bash
# Build
pio run

# Build and flash
pio run --target upload

# Serial monitor (115200 baud)
pio device monitor
```

---

## Project structure

```
├── src/
│   ├── main.cpp        # app_main — task creation, core pinning
│   ├── shared.h        # Pin defs, shared state, mutex declarations
│   ├── shared.cpp      # State accessors, mutex init
│   ├── web.h           # web_task declaration
│   ├── web.cpp         # WiFi AP, DNS captive portal, HTTP handlers
│   └── web_ui.h        # Embedded HTML/JS for web interface
├── platformio.ini      # Board and build config (ESP32-S3, ESP-IDF)
├── sdkconfig.defaults  # ESP-IDF config overrides
└── CLAUDE.md           # Development notes
```

---

## Key differences from the Pico version

| | Pico 2W | ESP32-S3 |
|--|---------|----------|
| Framework | arduino-pico | ESP-IDF |
| RTOS | Bare metal (Core 1) | FreeRTOS |
| Mutex | `mutex_t` (Pico SDK) | `SemaphoreHandle_t` |
| Alarm pool | `alarm_pool_add_alarm_in_us` | `esp_timer` one-shot |
| ISR placement | Automatic | `IRAM_ATTR` required |
| Web server | WebServer (Arduino) | `esp_http_server` |

---

## Licence

Provided as-is for personal and educational use.
