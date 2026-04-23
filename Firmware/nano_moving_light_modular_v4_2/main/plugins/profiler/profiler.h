// ══════════════════════════════════════════════════════════════════
//  PROFILER — Temporäres Diagnose-Plugin
//  Gibt alle PROFILER_INTERVAL_MS ms CPU-/Heap-/Task-Metriken aus.
//
//  Aktivieren: am Ende von config.h hinzufügen:
//    #include "plugins/profiler/profiler.h"   // TEMP
//  Deaktivieren: Zeile auskommentieren + diese Datei löschen.
//
//  WICHTIG: Profiler muss das LETZTE Plugin in config.h sein,
//           damit profiler_loop() den gesamten Loop-Zeitstempel misst.
// ══════════════════════════════════════════════════════════════════

#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <freertos/task.h>

// ── Konfiguration (hier anpassen) ─────────────────────────────────
#define PROFILER_INTERVAL_MS  5000   // Ausgabe-Intervall in ms
#define PROFILER_MAX_TASKS    20     // Max FreeRTOS-Tasks in der Tabelle
// ──────────────────────────────────────────────────────────────────

static uint32_t _prof_lastUs = 0;
static uint32_t _prof_lastMs = 0;
static uint32_t _prof_frames = 0;
static uint32_t _prof_sumUs  = 0;

// Hilfsfunktion: Zahl mit Tausender-Leerzeichen ausgeben
// z. B. 198456 → "198 456",  1234567 → "1 234 567"
static void _prof_printNum(uint32_t n) {
  if      (n >= 1000000) Serial.printf("%lu %03lu %03lu", (unsigned long)(n/1000000), (unsigned long)((n/1000)%1000), (unsigned long)(n%1000));
  else if (n >= 1000)    Serial.printf("%lu %03lu",       (unsigned long)(n/1000),    (unsigned long)(n%1000));
  else                   Serial.print(n);
}

// ── divider strings (UTF-8 box-drawing ═) ─────────────────────────
static const char _prof_top[] PROGMEM =
  "\n\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550"
  " [PROFILER] "
  "\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550";
static const char _prof_bot[] PROGMEM =
  "\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550"
  "\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550"
  "\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550";

// ── Setup ──────────────────────────────────────────────────────────
void profiler_setup() {
  _prof_lastUs = micros();
  _prof_lastMs = millis();
  Serial.printf("[Profiler] started — reporting every %u ms\n", PROFILER_INTERVAL_MS);
}

// ── Loop ───────────────────────────────────────────────────────────
void profiler_loop() {
  uint32_t now = micros();
  _prof_sumUs += now - _prof_lastUs;
  _prof_lastUs = now;
  _prof_frames++;

  if (millis() - _prof_lastMs < PROFILER_INTERVAL_MS) return;
  _prof_lastMs = millis();

  uint32_t avgUs  = _prof_frames ? (_prof_sumUs / _prof_frames) : 0;
  uint32_t freqHz = avgUs        ? (1000000UL  / avgUs)         : 0;
  _prof_frames = 0;
  _prof_sumUs  = 0;

  // ── Header ────────────────────────────────────────────────────────
  Serial.println((__FlashStringHelper*)_prof_top);

  // ── Timing ────────────────────────────────────────────────────────
  Serial.printf("  Uptime        : %10lu ms\n", millis());
  Serial.printf("  Loop freq     : %10lu Hz    avg frame: %lu us\n",
                (unsigned long)freqHz, (unsigned long)avgUs);

  // ── Heap ──────────────────────────────────────────────────────────
  Serial.printf("  Free heap     : %10u B    min ever: %u B\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap());
  Serial.printf("  Max alloc blk : %10u B\n", ESP.getMaxAllocHeap());

  // ── WiFi ──────────────────────────────────────────────────────────
  int rssi = WiFi.RSSI();
  if (rssi == 0 || WiFi.status() != WL_CONNECTED)
    Serial.println(F("  WiFi RSSI     :   -- (not connected)"));
  else
    Serial.printf("  WiFi RSSI     : %10d dBm\n", rssi);

  // ── CPU ───────────────────────────────────────────────────────────
  Serial.printf("  CPU freq      : %10u MHz\n", ESP.getCpuFreqMHz());

  // ── FreeRTOS task table ───────────────────────────────────────────
  TaskStatus_t tasks[PROFILER_MAX_TASKS];
  UBaseType_t  taskCount = uxTaskGetSystemState(tasks, PROFILER_MAX_TASKS, NULL);

  Serial.println(F(""));
  Serial.println(F("  Tasks         Name             St  StackFree  Prio"));
  Serial.println(F("                ─────────────────────────────────────"));

  for (UBaseType_t i = 0; i < taskCount; i++) {
    char st = '?';
    switch (tasks[i].eCurrentState) {
      case eRunning:   st = 'R'; break;  // Currently running
      case eReady:     st = 'r'; break;  // Ready to run
      case eBlocked:   st = 'B'; break;  // Waiting for event
      case eSuspended: st = 'S'; break;  // Suspended
      case eDeleted:   st = 'D'; break;  // Deleted (cleanup pending)
      default:         break;
    }
    uint32_t stackFreeBytes = (uint32_t)tasks[i].usStackHighWaterMark * sizeof(StackType_t);
    // Warn low stack with marker
    const char* warn = (stackFreeBytes < 512) ? " !!!" : "";
    Serial.printf("                %-16s %c  %9u  %4u%s\n",
                  tasks[i].pcTaskName,
                  st,
                  stackFreeBytes,
                  (uint32_t)tasks[i].uxCurrentPriority,
                  warn);
  }

  // ── Footer ────────────────────────────────────────────────────────
  Serial.println((__FlashStringHelper*)_prof_bot);
}

REGISTER_PLUGIN(profiler);
