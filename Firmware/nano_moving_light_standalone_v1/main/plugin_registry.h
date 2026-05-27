#pragma once

// ── Plugin Registry ───────────────────────────────────────────────
// Auto-registration system for plugins.
// Each plugin calls REGISTER_PLUGIN(name) once, which adds it to the
// global _plugins[] table via a static C++ initializer.
//
// main.ino iterates _plugins[] — it never needs to be changed.
// To add a plugin: #include "plugins/<name>/<name>.h" in config.h
// To remove a plugin: comment that include out.
// ─────────────────────────────────────────────────────────────────

#define MAX_PLUGINS 8

struct Plugin {
  void (*setup)();
  void (*loop)();
};

// Storage defined here (single TU via #pragma once — Arduino builds
// all headers as one translation unit through main.ino).
Plugin _plugins[MAX_PLUGINS] = {};
int    _pluginCount = 0;

// Registrar: construct one static instance per plugin to auto-register.
struct PluginRegistrar {
  PluginRegistrar(void(*s)(), void(*l)()) {
    if (_pluginCount < MAX_PLUGINS)
      _plugins[_pluginCount++] = {s, l};
  }
};

// Place at the bottom of any plugin header to auto-register it.
// The static initializer runs before setup(), in include order.
#define REGISTER_PLUGIN(name) \
  static PluginRegistrar _##name##_reg(name##_setup, name##_loop)
