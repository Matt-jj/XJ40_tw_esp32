# XJ40 Trigger — ESP32-S3

Ignition timing modifier for the **Jaguar XJ40 AJ26 4.0** straight-six engine, running on the **ESP32-S3** using the native **ESP-IDF** framework.

Sits between the crank position sensor and the ECU, reading the 36-1 trigger wheel signal and re-outputting it with a configurable timing offset.

> **Companion project:** The original working implementation runs on a Raspberry Pi Pico 2W and lives at [Matt-jj/xj40_tw](https://github.com/Matt-jj/xj40_tw). That project is the reference for all timing logic.

---

## Status

| Component | Status |
|-----------|--------|
| WiFi AP + captive portal | ✅ Done |
| HTTP server + web UI | ✅ Done |
| Trigger wheel ISR (Core 1) | ✅ Done |
| Advance (predictive — `esp_timer` ISR dispatch) | ✅ Done |
| Retard (reactive delay) | ✅ Done |
| RPM averaging (gap-to-gap, 5-rev) | ✅ Done |
| RPM stale detection + sync reset | ✅ Done |
| Offset slew (0.5°/rev) | ✅ Done |
| Auto-detect trigger wheel teeth (10-rev confirmation) | ✅ Done |
| NVM persistence (offset, teeth, switch mode) | ✅ Done |
| Hardware bring-up + pin finalisation | ⬜ Provisional |
| Logic analyser verification (advance/retard) | 🔄 In progress |
| Strip unused libraries + OTA partition | ⬜ Pre-release |

---

## Architecture

| Core | Task | Role |
|------|------|------|
| **Core 0** | `web_task` | WiFi AP, DNS captive portal, HTTP server |
| **Core 1** | `timing_task` | Trigger wheel ISR, offset timers, stale sync reset, NVM save |

Shared state is protected by a FreeRTOS mutex. ISR-written volatile state (RPM, sync, tooth count) uses aligned types for atomic reads on 32-bit Xtensa — no mutex needed.

Advance/retard one-shot timers use `esp_timer` with **ISR dispatch** (`ESP_TIMER_ISR`), running from the hardware timer interrupt rather than the timer service task. This eliminates WiFi-induced jitter on Core 0 (measured glitch of 800µs with task dispatch → < 10µs with ISR dispatch).

---

## How it works

```
Hall sensor (5V) ──► Opto-isolator ──► ESP32-S3 GPIO4 (input)
                                              │
                                        ISR (Core 1)
                                        processes 36-1 wheel,
                                        applies timing offset
                                              │
                                   ESP32-S3 GPIO5 (output) ──► Opto-isolator ──► ECU
```

### Timing offset

- Range: **-10.0° (retard)** to **+10.0° (advance)**, in 0.5° steps
- **Advance** — predictive: fires output rising edge early via `esp_timer` ISR one-shot
- **Retard** — reactive: delays both edges after trigger wheel edges via `esp_timer` ISR one-shot
- **RPM ramp** — offset scales 0→100% between 1000–1200 RPM
- **Slew** — transitions at 0.5°/rev when offset changes (prevents timing jolts)
- **Pass-through** — always active below 1000 RPM and until teeth count is confirmed

### Teeth auto-detection

The ISR counts teeth between successive gap detections. After 10 consistent readings the count is confirmed and saved to NVM. No manual configuration needed — the wheel tooth count is detected automatically on first run.

A manual override is available via the web UI (engine must be stopped to change).

### RPM measurement

RPM is computed from gap-to-gap timing (`g_avg_rev_period_us`), averaged over 5 revolutions. This is completely independent of teeth count, which breaks the circular dependency between RPM and auto-detection. If no gap is seen for > 2 rev periods, RPM returns 0. Full sync reset occurs after 2 seconds with no signal.

---

## Hardware

> Pin assignments are provisional pending final hardware bring-up.

| Pin | Function |
|-----|----------|
| GPIO4 | Trigger input (from opto-isolator) |
| GPIO5 | Trigger output (to opto-isolator / ECU) |
| GPIO6 | Enable switch (active-low, internal pull-up) |

- **Trigger wheel:** 36-1 (35 teeth + 1 missing), 10° per tooth
- **Sensor:** Hall effect, 5V output
- **Isolation:** Opto-isolators for 5V ↔ 3.3V level conversion
- **Module:** TENSTAR ESP32-S3-Zero (4MB flash, 2MB PSRAM)

---

## Web UI

Connect to the **XJ40-Trigger** WiFi network (open, no password).  
Any URL redirects to the captive portal at **192.168.4.1**.

- Live RPM display and sync status indicator
- Timing offset slider (-10° to +10°) with Apply button
- Remote switch toggle (enable pin bypasses offset)
- Trigger wheel teeth display — auto-detected, with manual override (engine stopped only)

### API

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/` | Web UI |
| GET | `/api/status` | JSON: `rpm`, `synced`, `offset_tenths`, `switch_mode`, `teeth`, `teeth_auto`, `teeth_manual`, `teeth_confirmed` |
| POST | `/api/offset?value=X` | Set offset (X = tenths, -100..+100) |
| POST | `/api/config?switch_mode=0\|1&teeth=N&teeth_manual=0\|1` | Update config |

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
│   ├── main.cpp        # app_main — task creation, stale sync reset, NVM save
│   ├── piggyback.h     # piggyback_setup() declaration
│   ├── piggyback.cpp   # Trigger wheel ISR, advance/retard timers, auto-detect
│   ├── shared.h        # Pin defs, shared state, ISR-safe accessors
│   ├── shared.cpp      # State accessors, mutex, NVS load/save, gap-based RPM
│   ├── web.h           # web_task declaration
│   ├── web.cpp         # WiFi AP, DNS captive portal, HTTP handlers
│   └── web_ui.h        # Embedded HTML/JS for web interface
├── platformio.ini      # Board and build config (ESP32-S3, ESP-IDF)
├── sdkconfig.defaults  # ESP-IDF config overrides
└── CLAUDE.md           # Development notes and session log
```

---

## Key differences from the Pico version

| | Pico 2W | ESP32-S3 |
|--|---------|----------|
| Framework | arduino-pico | ESP-IDF |
| RTOS | Bare metal (Core 1) | FreeRTOS |
| Mutex | `mutex_t` (Pico SDK) | `SemaphoreHandle_t` |
| Alarm pool | `alarm_pool_add_alarm_in_us` | `esp_timer` (ISR dispatch) |
| ISR placement | Automatic | `IRAM_ATTR` on timer callbacks |
| Web server | WebServer (Arduino) | `esp_http_server` |
| Teeth config | Manual slider | Auto-detected + manual override |
| NVM | EEPROM 2-of-3 majority vote | ESP-IDF NVS (two-phase commit, CRC32) |

---

## Licence

Provided as-is for personal and educational use.
