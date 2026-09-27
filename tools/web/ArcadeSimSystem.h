#pragma once
// Browser build (ARCADE_WEB): replaces tools/sim/ArcadeSimSystem.h, because
// -Itools/web comes first on the include path. Preferences are kept in the
// browser's localStorage so volume, difficulty and hi-scores survive reloads.
#include <emscripten.h>
#include <string>

EM_JS(int, arcadeWebPrefGet, (const char* ns, const char* key, int def), {
  try {
    var v = localStorage.getItem("nova-arcade/" + UTF8ToString(ns) + "/" + UTF8ToString(key));
    return v === null ? def : (parseInt(v, 10) >>> 0);
  } catch (e) { return def; }
});
EM_JS(void, arcadeWebPrefPut, (const char* ns, const char* key, int v), {
  try { localStorage.setItem("nova-arcade/" + UTF8ToString(ns) + "/" + UTF8ToString(key), String(v >>> 0)); } catch (e) {}
});
// SELECT+START, the system button or "quit to menu": hand control back to the page
EM_JS(void, arcadeWebExit, (), {
  if (Module.onExit) Module.onExit();
});

struct Preferences {
  std::string ns;
  bool begin(const char* name, bool) { ns = name; return true; }
  uint32_t getUInt(const char* k, uint32_t d) { return (uint32_t)arcadeWebPrefGet(ns.c_str(), k, (int)d); }
  void putUInt(const char* k, uint32_t v) { arcadeWebPrefPut(ns.c_str(), k, (int)v); }
  uint8_t getUChar(const char* k, uint8_t d) { return (uint8_t)getUInt(k, d); }
  void putUChar(const char* k, uint8_t v) { putUInt(k, v); }
  size_t getBytes(const char*, void*, size_t) { return 0; }
  size_t putBytes(const char*, const void*, size_t n) { return n; }
  std::string getString(const char*, const char* d = "") { return std::string(d); }
  size_t getString(const char*, char* v, size_t n) { if (n) v[0] = 0; return 0; }
  size_t putString(const char*, const char*) { return 1; }
  bool end() { return true; }
};
