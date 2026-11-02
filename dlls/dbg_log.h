//=========================================================
// dbg_log.h - Debug log line helper (debug builds / test runs only, not retail).
//
// Usage in any server code:
//     #include "dbg_log.h"
//     DBG_LOG("ShootBeam origin=(%.1f %.1f %.1f) attachment=%d", v.x, v.y, v.z, attachment);
//
// Every call appends one line to dbg_events.log in the engine's working directory
// (out/run/xash3d when started with scripts/run-gunman-xash.sh). The line starts with the
// game time. Nothing is written unless the cvar dbg_eventlog is 1 (debug.cfg sets it).
// The file is shared with the engine event wrapper in h_export.cpp (PLAYBACK / PRECACHE lines).
//
// Rules:
//  - Use DBG_LOG only to answer a concrete question (what fired, where, with which values).
//    Remove it again after the test or keep it behind dbg_eventlog.
//  - Write German remarks around it, the format string itself stays in English/printf form.
//=========================================================
#pragma once

void DbgLogWrite(const char* fmt, ...);

#define DBG_LOG(...) DbgLogWrite(__VA_ARGS__)
