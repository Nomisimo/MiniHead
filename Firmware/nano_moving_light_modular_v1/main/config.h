// ── Module Registry ───────────────────────────────────────────────
// Add or remove modules here. Each entry = one .h file.
// The module must implement: NAME_setup() and NAME_loop()
// ─────────────────────────────────────────────────────────────────

#pragma once

// Forward-declare all module functions
// #include "modules/wifi_control/wifi_control.h"
#include "modules/startup_animation.h"

// ── Module table ─────────────────────────────────────────────────
// To disable a module: comment out its line
// To add a module:     add a new line + forward-declare above

typedef void (*ModuleFn)();

struct Module {
  ModuleFn setup;
  ModuleFn loop;
};

Module modules[] = {
  { startup_animation_setup, startup_animation_loop },
//  { wifi_control_setup, wifi_control_loop },
  // { my_new_module_setup, my_new_module_loop },
};

const int MODULE_COUNT = sizeof(modules) / sizeof(modules[0]);
