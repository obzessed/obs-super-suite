#pragma once

// Feature Toggles (Enable/Disable to isolate crashes)
#define ENABLE_SUPER_MIXER_DOCK 0
#define ENABLE_DAW_MIXER_DOCK 0
#define ENABLE_S_MIXER_DOCK 1
#define ENABLE_VOLUME_METER_DOCK 0
#define ENABLE_SOURCERER_DOCKS 0
#define ENABLE_MIDI_DOCKS 0
#define ENABLE_TEST_SUPER_DOCK 0
#define ENABLE_AUDIO_MATRIX 0
#define ENABLE_TWEAKS_PANEL 0
#define ENABLE_GRAPH_EDITORS 0
#define ENABLE_BROWSER_DOCKS 0
#define ENABLE_CHANNELS_VIEWER 0
#define ENABLE_OUTPUTS_VIEWER 0
#define ENABLE_ENCODERS_VIEWER 0
#define ENABLE_DOCK_WINDOW_MANAGER 0
#define ENABLE_ENCODING_GRAPH 0

#ifdef __cplusplus
extern "C" {
#endif

bool on_plugin_load();

void on_plugin_loaded();

void on_plugin_unload();

#ifdef __cplusplus
}
#endif