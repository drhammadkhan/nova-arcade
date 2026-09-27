#pragma once
// =====================================================================
//  ArcadeCore - shared engine for Nova Arcade games on the ESP32-S3 2.8"
//  board. Include this ONCE, from your game's .ino file.
//
//    #include <ArcadeCore.h>
//    void setup() { arcade::begin("mygame", &MY_MUSIC); }
//    void loop()  { arcade::run(step, draw); }
//
//  step(const Pad&) runs at a fixed 60 Hz; draw() is called once per
//  48-row strip (see arcade/Render.h). SELECT+UP/DOWN changes volume,
//  SELECT+START (or the Stadia button) returns to the launcher.
// =====================================================================
#include <Arduino.h>
#ifndef ARCADE_SIM
#include <Wire.h>
#endif
#include "arcade/Config.h"
#include "arcade/System.h"

using gfx::SW;
using gfx::SH;
