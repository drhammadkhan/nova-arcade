#pragma once
// Nova Lance soundtrack (original). Songs: 0 silence, 1 title, 2 stage, 3 boss, 4 game over.
#include <ArcadeCore.h>
namespace nl_music {
using namespace audio;
// Lead lines. 0 = rest, 1 = hold previous note, otherwise MIDI note.
static const uint8_t LEADS[][16] = {
  /* 0 stage Am */ {69, 1, 72, 1, 76, 1, 81, 1, 79, 1, 76, 1, 72, 1, 74, 76},
  /* 1 stage F  */ {77, 1, 1, 1, 76, 1, 72, 1, 69, 1, 72, 1, 77, 1, 76, 72},
  /* 2 stage C  */ {79, 1, 1, 1, 76, 1, 79, 1, 84, 1, 83, 1, 79, 1, 76, 1},
  /* 3 stage G  */ {74, 1, 79, 1, 83, 1, 86, 1, 83, 1, 79, 1, 74, 1, 71, 1},
  /* 4 stage E  */ {76, 1, 1, 1, 80, 1, 83, 1, 88, 1, 1, 1, 1, 1, 0, 0},
  /* 5 stage Am2*/ {81, 1, 1, 1, 79, 1, 76, 1, 72, 1, 76, 1, 69, 1, 1, 1},
  /* 6 boss Am  */ {69, 81, 69, 79, 69, 76, 69, 79, 69, 81, 69, 79, 69, 76, 72, 74},
  /* 7 boss Bb  */ {70, 82, 70, 81, 70, 77, 70, 74, 70, 82, 70, 81, 77, 1, 74, 1},
  /* 8 boss E   */ {68, 80, 68, 83, 68, 86, 68, 88, 80, 1, 83, 1, 86, 1, 88, 1},
  /* 9 title Am */ {81, 1, 1, 1, 1, 1, 1, 1, 79, 1, 1, 1, 76, 1, 1, 1},
  /*10 title F  */ {77, 1, 1, 1, 1, 1, 1, 1, 76, 1, 1, 1, 72, 1, 1, 1},
  /*11 title C  */ {79, 1, 1, 1, 1, 1, 1, 1, 84, 1, 1, 1, 83, 1, 1, 1},
  /*12 title G  */ {83, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0},
  /*13 game over*/ {76, 1, 72, 1, 69, 1, 64, 1, 1, 1, 1, 1, 1, 1, 0, 0},
};
static const char* const DRUMS[] = {
  "K.h.S.h.K.hKS.h.",   // 0 stage
  "K.h.S.h.K.S.SSSS",   // 1 fill
  "KhKhShKhKhKhShSh",   // 2 boss
  "..h...h...h...h.",   // 3 title (soft)
  "................",   // 4 none
};
static const Bar STAGE_BARS[] = {
  {AM, 0, 0}, {F_, 1, 0}, {C_, 2, 0}, {G_, 3, 0},
  {AM, 0, 0}, {F_, 1, 0}, {C_, 2, 0}, {E_, 4, 1},
  {AM, 5, 0}, {F_, 1, 0}, {C_, 2, 0}, {G_, 3, 0},
  {AM, 5, 0}, {F_, 1, 0}, {DM, 3, 0}, {E_, 4, 1},
};
static const Bar BOSS_BARS[] = {
  {AM, 6, 2}, {BB, 7, 2}, {AM, 6, 2}, {E_, 8, 1},
};
static const Bar TITLE_BARS[] = {
  {AM, 9, 3}, {F_, 10, 3}, {C_, 11, 3}, {G_, 12, 3},
};
static const Bar OVER_BARS[] = { {AM, 13, 4}, {AM, -1, 4} };

static const SongDef SONGS[] = {
  {nullptr, 0, 120, false, false, 0},
  {TITLE_BARS, 4, 104, true, false, 0},
  {STAGE_BARS, 16, 150, true, false, 0},
  {BOSS_BARS, 4, 168, true, true, 0},
  {OVER_BARS, 2, 96, false, false, 0},
};
}  // namespace nl_music
static const audio::Music MUSIC = {audio::STD_CHORDS, nl_music::LEADS, nl_music::DRUMS, nl_music::SONGS, 5};
enum { SONG_NONE, SONG_TITLE, SONG_STAGE, SONG_BOSS, SONG_GAMEOVER };
