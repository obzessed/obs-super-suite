#pragma once

#ifdef __cplusplus
extern "C" {
#endif

bool on_plugin_load();

void on_plugin_loaded();

void on_plugin_unload();

#ifdef __cplusplus
}
#endif

// Called when ASIO settings are changed to update running sources
void refreshAsioSources();