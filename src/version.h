#pragma once

// ---------------------------------------------------------------------------
// Firmware version — MAJOR.MINOR, minor digit runs 1-9 only (1.9 -> 2.0).
// Bump once per logical unit of work, before flashing a build worth logging.
//
// FIRMWARE_GIT_HASH is injected by CMake at build time (see CMakeLists.txt).
// Never write a hash into the history below — a commit cannot contain its own.
// ---------------------------------------------------------------------------

#define FIRMWARE_VERSION     "1.0"

// --- History (newest first) ------------------------------------------------
// 1.0  First release under MAJOR.MINOR versioning. Supersedes the untracked
//      "0.1.0" string. Baseline: IRAM trigger ISR with ISR-dispatch advance/
//      retard timers, auto teeth detect w/ manual override, gap-based RPM,
//      stale-sync reset, WS2812 status LED, captive-portal web UI.
// ---------------------------------------------------------------------------

// Fallback so the tree still builds outside git (tarball, CI export).
#ifndef FIRMWARE_GIT_HASH
#define FIRMWARE_GIT_HASH    "nogit"
#endif
