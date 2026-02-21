#pragma once

// Feature Toggles (Enable/Disable to isolate crashes)
#define ENABLE_SUPER_MIXER_DOCK 0
#define ENABLE_DAW_MIXER_DOCK 0
#define ENABLE_VOLUME_METER_DOCK 0
#define ENABLE_MIDI_DOCKS 0
#define ENABLE_TEST_SUPER_DOCK 0
#define ENABLE_GRAPH_EDITORS 0

#define ENABLE_S_MIXER_DOCK 1
#define ENABLE_SOURCERER_DOCKS 1
#define ENABLE_BROWSER_DOCKS 1
#define ENABLE_CHANNELS_VIEWER 1
#define ENABLE_OUTPUTS_VIEWER 1
#define ENABLE_ENCODERS_VIEWER 1
#define ENABLE_DOCK_WINDOW_MANAGER 1
#define ENABLE_AUDIO_MATRIX 1
#define ENABLE_ENCODING_GRAPH 1
#define ENABLE_TWEAKS_PANEL 1

#ifdef __cplusplus
extern "C" {
#endif

bool on_plugin_load();

void on_plugin_loaded();

void on_plugin_unload();

#ifdef __cplusplus
}
#endif