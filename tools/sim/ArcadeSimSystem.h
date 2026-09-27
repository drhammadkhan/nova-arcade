#pragma once
#include <map>
struct Preferences {
  std::map<std::string,uint32_t> m;
  bool begin(const char*, bool){ return true; }
  uint32_t getUInt(const char* k, uint32_t d){ auto i=m.find(k); return i==m.end()?d:i->second; }
  void putUInt(const char* k, uint32_t v){ m[k]=v; }
  uint8_t getUChar(const char* k, uint8_t d){ return (uint8_t)getUInt(k,d); }
  void putUChar(const char* k, uint8_t v){ m[k]=v; }
  size_t getBytes(const char*, void*, size_t){ return 0; }
  size_t putBytes(const char*, const void*, size_t n){ return n; }
  std::string getString(const char*, const char* d=""){ return std::string(d); }
  size_t getString(const char*, char* v, size_t n){ if(n) v[0]=0; return 0; }
  size_t putString(const char*, const char*){ return 1; }
  bool end(){ return true; }
};
