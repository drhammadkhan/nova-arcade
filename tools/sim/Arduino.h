#pragma once
// Minimal Arduino shim for the desktop simulator
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <string>
using std::min; using std::max;
template<typename T,typename A,typename B> static inline T constrain(T v, A lo, B hi){ return v<lo?(T)lo:(v>hi?(T)hi:v);}
struct SerialStub { void begin(int){} template<typename...A> void printf(const char* f, A... a){ ::printf(f,a...);} void println(const char* s){ puts(s);} void print(const char* s){ fputs(s, stdout);} } Serial;
static inline uint32_t micros(){ static auto t0=std::chrono::steady_clock::now(); return (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-t0).count(); }
static inline uint32_t millis(){ return micros()/1000; }
static inline void delay(int){}
static inline uint32_t esp_random(){ return 12345; }
struct EspStub { uint32_t getFreeHeap(){ return 0; } void restart(){} } ESP;
typedef void* QueueHandle_t;
#define pdTRUE 1
static uint16_t sim_q[256]; static int sim_qn=0;
static inline QueueHandle_t xQueueCreate(int,int){ return (void*)1; }
static inline int xQueueSend(QueueHandle_t, const void* v, int){ if(sim_qn<256) sim_q[sim_qn++]=*(const uint16_t*)v; return 1; }
static inline int xQueueReceive(QueueHandle_t, void* v, int){ if(!sim_qn) return 0; *(uint16_t*)v=sim_q[0]; memmove(sim_q,sim_q+1,(--sim_qn)*2); return 1; }
#define TFT_WHITE 0xFFFF
#define TFT_BLACK 0x0000
