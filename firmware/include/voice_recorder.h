#pragma once
// Voice Recorder and WAV player for boards with a microphone driver
// (BOARD_HAS_MIC, board_pins.h). voice_recorder.cpp; everything compiles out
// on the other boards.
#include "board_pins.h"
#if BOARD_HAS_MIC
#include <Arduino.h>
enum class VoiceRec:uint8_t {Idle,Recording,Error};
enum class VoicePlay:uint8_t {Idle,Playing,Paused,Error};
struct VoiceStatus {
  VoiceRec rec=VoiceRec::Idle;VoicePlay play=VoicePlay::Idle;
  uint32_t recSeconds=0,recBytes=0,dropped=0;uint8_t peak=0;
  uint32_t playMs=0,playTotalMs=0,playRate=0;uint8_t playChannels=0;
  char recFile[64]{},recWhy[96]{},playFile[128]{},playWhy[96]{};
};
constexpr unsigned VoiceListMax=6;
// The newest recordings by name (names carry the date) and the card's free space.
struct VoiceListing {uint32_t revision=0;bool ready=false;uint8_t count=0;uint32_t total=0;uint64_t freeBytes=0;char name[VoiceListMax][48]{};uint32_t bytes[VoiceListMax]{};};
VoiceStatus voiceStatus();
bool voiceRecording();          // recording, or saving its last samples
bool voicePlaying();            // playing or paused
bool voiceBusy();               // either: alert tones wait (playback) or stay off (recording)
// Main loop only (the ES7210 shares I2C with touch and the PMU). False with why.
bool voiceRecordStart(char* why,size_t size);
void voiceRecordStop();
bool voicePlay(const char* path,char* why,size_t size);
void voicePause(bool paused);
void voicePlayStop();
void voiceRefresh();            // re-read /recordings and the free space in the background
VoiceListing voiceListing();
bool voiceDelete(const char* path,char* why,size_t size);
void voiceLoop();               // main loop: microphone power and the end-of-recording notice
bool voiceShutdown(uint32_t waitMs); // sleep / power off: stop and close the files first
bool voiceCommand(const String& command);
#endif
