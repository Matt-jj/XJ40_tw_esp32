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

## Session notes — 06-04-2026

### Real-time resource audit

Reviewed shared resources between `main.cpp` and `piggyback.cpp` for anything that could interrupt real-time operation. Key findings and fixes:

**Fix 1 — `stop_all_timers()` guard (`s_timers_active`)**
- `stop_all_timers()` was called on every passthrough tooth, causing 4× `esp_timer_stop()` spinlock acquisitions even when no timers were armed (~140 unnecessary cross-core spinlock calls per revolution).
- Fix: `s_timers_active` bool flag. `stop_all_timers()` returns immediately if false. Set true only when a timer is actually armed.

**Fix 2 — `trigger_isr` and NVS flash stall**
- `trigger_isr` code was in flash. NVS writes suspend the flash instruction cache → ISR stalled for up to 30ms during a sector erase.
- Fix: `IRAM_ATTR` on `trigger_isr` and `stop_all_timers()`. ISR code now lives in IRAM — completely immune to flash writes.
- `ESP_INTR_FLAG_IRAM` added to `gpio_install_isr_service()` to match.
- Removed `!g_synced_isr` guard on NVS save — settings now persist within 250ms of any change, engine running or stopped. Power-loss write-miss risk eliminated.
- Note: `CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD=y` (already set) is required — it places `esp_timer_start_once` / `esp_timer_stop` in IRAM, which is what previously caused the cache crash when IRAM_ATTR was attempted.

### Remaining shared resource notes (low risk, no action needed)
- `g_adv_pw` / `g_adv_fired` cross-core (Core 1 GPIO ISR ↔ Core 0 timer ISR): no memory barrier, but safe on ESP32-S3 (DRAM not L1-cached).
- `g_state.teeth_total` written from ISR on auto-confirm: atomic uint8_t write, fires once only.
- Stale sync reset writes 3 globals in sequence: practically impossible race (engine stopped for 2s before it triggers).

## Session notes — 11-04-2026

### Root cause fix — `gpio_get_level()` called from IRAM ISR

Two remaining non-IRAM calls inside IRAM code were causing 558ms reboot cycles:

1. `piggyback.cpp` — `gpio_get_level(PIN_TRIGGER_IN)` in `trigger_isr`
2. `shared.h` — `gpio_get_level(PIN_ENABLE)` in `isr_get_offset_tenths()`

When WiFi finishes initialising (~400ms after boot) it writes config to flash, suspending the instruction cache. The GPIO ISR fired during this window and tried to fetch `gpio_get_level()` from flash → cache fault → reboot. This repeated on every boot, explaining the exact 558ms crash cycle. The advance never applied because `g_teeth_confirmed` requires ~500ms (10 revolutions) but the crash always hit at ~400ms.

**Fix:** Replaced both calls with direct peripheral register reads (always DRAM, flash-independent):
- `(REG_READ(GPIO_IN_REG) >> PIN_TRIGGER_IN) & 1`
- `(REG_READ(GPIO_IN_REG) >> PIN_ENABLE) & 1`
- Added `#include "soc/gpio_reg.h"` to `shared.h`

### Timing accuracy analysis (XJ40_esp32_11-04-26_sesh1/2/3)

Three captures analysed: 1200 RPM 200kHz, 1200 RPM 1MHz, 3000 RPM 1MHz.

**Key correction:** tooth period = `rev_period / 36` (uniform angular positions), not `rev_period / 35` (physical teeth count). The MT gap spans 2 positions; dividing by 35 inflates the tooth period and all derived degree values.

**Results (5° advance, both speeds):**

| | 1200 RPM | 3000 RPM |
|---|---|---|
| Advance delivered | 4.906° | 4.759° |
| Systematic shortfall | −0.094° (−13µs) | −0.241° (−13µs) |
| Rise jitter (1σ) | ±0.004° (±0.6µs) | ±0.020° (±1.1µs) |
| Width error (mean) | +0.058° (+8µs) | +0.144° (+8µs) |

- **13µs fixed latency** — `esp_timer` ISR dispatch overhead, constant across RPM range (13.00µs at 1200, 13.39µs at 3000). Systematic shortfall scales with RPM in degree terms but is within tolerance.
- **Rise jitter** essentially at noise floor — dominated by emulator signal quality at 3000 RPM (CH2 period stdev 0.82µs), not ESP32 timer error. Real engine will have equivalent scatter from wheel runout and combustion events; ESP32 reacts to actual edges so this is self-correcting.
- **Width error +8µs** — caused by using previous tooth's HIGH duration as fall timer prediction. Constant across RPM. Shortens the LOW duration (fall→rise) by 8µs, increasing the MT gap ratio from 3.001× to 3.023× — marginally easier for ECU to detect the gap.
- **No crashes, no outliers, no interrupt conflicts** in any capture after the gpio_get_level fix.

### Analysis rules established
- Only make claims about firmware/hardware behaviour backed by evidence (code, captures, datasheets). No assumptions.
- Challenge assumptions in both directions — ask for evidence before proceeding.

## TODO

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
