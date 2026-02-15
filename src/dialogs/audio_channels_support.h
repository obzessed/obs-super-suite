#pragma once

#include <obs.h>
#include <plugin-support.h>

// OBS Channels (64) Reservations
// 1 - Scene Transition
// 2 - Desktop Audio 1
// 3 - Desktop Audio 2
// 4 - Mix/Aux 1
// 5 - Mix/Aux 2
// 6 - Mix/Aux 3
// 7 - Mix/Aux 4
// 8 - 64 - Unreserved

// 8-... - Downstream Keyer uses channels starting from (8-MAX_CHANNELS).
// 63 - SoundBoard plugin puts a ffmpeg-source here to play its audio.

/*
 * #define MAX_AUDIO_MIXES 6 (Tracks)
 * #define MAX_AUDIO_CHANNELS 8 (Channels per Source)
 * #define MAX_DEVICE_INPUT_CHANNELS 64 (Output Channels)
*/

// Called when ASIO settings are changed to update running sources
extern void refreshAsioSources();

extern void createSources();

extern void audio_sources_cleanup();