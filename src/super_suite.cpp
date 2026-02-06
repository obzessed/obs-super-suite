#include <plugin-support.h>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <callback/signal.h>

#include <QMessageBox>
#include <QMainWindow>
#include <QPointer>

#include "super_suite.h"
#include "asio_config.h"
#include "asio_settings.h"
#include "channels_view.h"
#include "mixer_dock.h"

#include <vector>
#include <utility>
#include <map>
#include <string>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

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

// Store ASIO sources with channel, canvas UUID, and source pointer
struct AsioSourceEntry {
	int channel;
	QString canvasUuid;
	obs_source_t *source;
};
static std::vector<AsioSourceEntry> asio_sources;
static QPointer<AsioSettingsDialog> settings_dialog;
static QPointer<ChannelsView> channels_view;
static QPointer<MixerDock> mixer_dock;

// Guard flag to prevent signal handlers from modifying config during createSources()
static bool creating_sources = false;

// Helper: Check if a source type is available (plugin installed)
static bool source_type_exists(const char *type_id)
{
	bool found = false;
	size_t idx = 0;
	const char *id = nullptr;
	
	while (obs_enum_source_types(idx++, &id)) {
		if (id && strcmp(id, type_id) == 0) {
			found = true;
			break;
		}
	}
	return found;
}

// Helper: Get canvas from UUID (empty = main canvas)
static obs_canvas_t* get_canvas_for_uuid(const QString &uuid)
{
	if (uuid.isEmpty()) {
		return obs_get_main_canvas();
	}
	obs_canvas_t *canvas = obs_get_canvas_by_uuid(uuid.toUtf8().constData());
	return canvas ? canvas : obs_get_main_canvas(); // Fallback to main if not found
}

// Apply audio control settings to a source
static void apply_audio_settings(obs_source_t *source, const AsioSourceConfig &cfg)
{
	if (!source) return;
	
	obs_source_set_muted(source, cfg.muted);
	obs_source_set_monitoring_type(source, (obs_monitoring_type)cfg.monitoringType);
	obs_source_set_volume(source, cfg.volume);
	obs_source_set_balance_value(source, cfg.balance);
	
	// Force mono via source flags
	uint32_t flags = obs_source_get_flags(source);
	if (cfg.forceMono) {
		flags |= OBS_SOURCE_FLAG_FORCE_MONO;
	} else {
		flags &= ~OBS_SOURCE_FLAG_FORCE_MONO;
	}
	obs_source_set_flags(source, flags);
	
	// Apply audio mixers (tracks)
	obs_source_set_audio_mixers(source, cfg.audioMixers);
	
	// Apply show in mixer state (audio_active controls mixer visibility, not final mix)
	obs_source_set_audio_active(source, cfg.audioActive);
}

template<typename T> T *calldata_get_pointer(const calldata_t *data, const char *name)
{
	void *ptr = nullptr;
	calldata_get_ptr(data, name, &ptr);
	return reinterpret_cast<T *>(ptr);
}

const char *calldata_get_string(const calldata_t *data, const char *name)
{
	const char *value = nullptr;
	calldata_get_string(data, name, &value);
	return value;
}

// Helper: Find config index by source pointer
// Uses source name matching which is stable even during createSources()
static int find_config_index_for_source(obs_source_t *source)
{
	if (!source) return -1;
	
	const char *sourceName = obs_source_get_name(source);
	if (!sourceName) return -1;
	
	const auto &sources = AsioConfig::get()->getSources();
	for (int i = 0; i < sources.size(); i++) {
		if (sources[i].name == QString::fromUtf8(sourceName)) {
			return i;
		}
	}
	return -1;
}

// Helper: Get channel for a source from asio_sources (for UI updates)
static int get_channel_for_source(obs_source_t *source)
{
	for (const auto &entry : asio_sources) {
		if (entry.source == source) {
			return entry.channel;
		}
	}
	return -1;
}

// Signal callback: source renamed
static void on_source_rename(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(data);

	obs_source_t *source = calldata_get_pointer<obs_source_t>(cd, "source");
	const char *new_name = calldata_string(cd, "new_name");
	const char *prev_name = calldata_string(cd, "prev_name");

	if (!source || !new_name || !prev_name) return;
	
	// Skip if we're in the middle of creating sources
	if (creating_sources) return;

	// Find config by previous name (source has already been renamed by OBS)
	auto &sources = AsioConfig::get()->getSources();
	for (int i = 0; i < sources.size(); i++) {
		if (sources[i].name == QString::fromUtf8(prev_name)) {
			sources[i].name = QString::fromUtf8(new_name);
			AsioConfig::get()->save();
			
			// Update UI using the source's UUID
			if (settings_dialog) {
				settings_dialog->updateSourceName(sources[i].sourceUuid, QString::fromUtf8(new_name));
			}
			
			obs_log(LOG_INFO, "ASIO source renamed: '%s' -> '%s'", prev_name, new_name);
			break;
		}
	}
}

// Signal callback: source settings updated
static void on_source_update(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(data);

	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	if (!source) return;
	
	// Skip if we're in the middle of creating sources
	if (creating_sources) return;

	// Find config by source name
	int idx = find_config_index_for_source(source);
	if (idx < 0) return;
	
	auto &sources = AsioConfig::get()->getSources();
	obs_data_t *settings = obs_source_get_settings(source);
	if (settings) {
		const char *json = obs_data_get_json(settings);
		if (json) {
			QJsonParseError error;
			QJsonDocument doc = QJsonDocument::fromJson(QByteArray(json), &error);
			if (error.error == QJsonParseError::NoError && doc.isObject()) {
				sources[idx].sourceSettings = doc.object();
				AsioConfig::get()->save();
				if (settings_dialog) {
					settings_dialog->updateSourceSettings(sources[idx].sourceUuid, sources[idx].sourceSettings);
					settings_dialog->updateSpeakerLayoutByUuid(sources[idx].sourceUuid);
				}
				obs_log(LOG_INFO, "ASIO source settings updated for '%s'",
					obs_source_get_name(source));
			}
		}
		obs_data_release(settings);
	}
}

// Helper function to save filter state for a source
static void save_source_filters(obs_source_t *source)
{
	// Skip if we're in the middle of creating sources
	if (creating_sources) return;
	
	int idx = find_config_index_for_source(source);
	if (idx < 0) return;
	
	auto &sources = AsioConfig::get()->getSources();
	obs_data_array_t *filterArray = obs_source_backup_filters(source);
	if (filterArray) {
		// Wrap in object with "filters" key for consistent parsing
		obs_data_t *wrapper = obs_data_create();
		obs_data_set_array(wrapper, "filters", filterArray);
		const char *json = obs_data_get_json(wrapper);
		if (json) {
			QJsonParseError error;
			QJsonDocument doc = QJsonDocument::fromJson(QByteArray(json), &error);
			if (error.error == QJsonParseError::NoError && doc.isObject()) {
				QJsonObject root = doc.object();
				if (root.contains("filters") && root["filters"].isArray()) {
					sources[idx].sourceFilters = root["filters"].toArray();
					AsioConfig::get()->save();
					if (settings_dialog) {
						settings_dialog->updateSourceFilters(sources[idx].sourceUuid, sources[idx].sourceFilters);
					}
					obs_log(LOG_INFO, "Saved filters for '%s'",
						obs_source_get_name(source));
				}
			}
		}
		obs_data_release(wrapper);
		obs_data_array_release(filterArray);
	}
}

// Signal callback: filter added or removed
static void on_filter_changed(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(data);

	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	if (!source) return;
	
	// Skip if we're in the middle of creating sources
	if (creating_sources) return;

	save_source_filters(source);
}

// Signal callback: filter settings updated
static void on_filter_settings_update(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(cd);
	
	// 'data' is the parent source
	obs_source_t *parent_source = (obs_source_t *)data;
	if (!parent_source) return;
	
	// Skip if we're in the middle of creating sources
	if (creating_sources) return;

	save_source_filters(parent_source);
}

// Signal callback: filter added
static void on_filter_added(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(data);

	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	obs_source_t *filter = (obs_source_t *)calldata_ptr(cd, "filter");
	
	if (!source || !filter) return;

	// Connect to filter's update signal to catch setting changes
	signal_handler_t *sh = obs_source_get_signal_handler(filter);
	if (sh) {
		signal_handler_connect(sh, "update", on_filter_settings_update, source);
	}

	// Skip config update if we're in the middle of creating sources
	if (creating_sources) return;

	save_source_filters(source);
}

// Signal callback: mute state changed
static void on_mute_changed(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	if (!source) return;
	
	// Skip if we're in the middle of creating sources
	if (creating_sources) return;

	int idx = find_config_index_for_source(source);
	if (idx < 0) return;

	bool muted = calldata_bool(cd, "muted");
	auto &sources = AsioConfig::get()->getSources();
	sources[idx].muted = muted;
	AsioConfig::get()->save();
	if (settings_dialog) {
		settings_dialog->updateSourceMuted(sources[idx].sourceUuid, muted);
	}
}

// Signal callback: volume changed
static void on_volume_changed(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	if (!source) return;
	
	// Skip if we're in the middle of creating sources
	if (creating_sources) return;

	int idx = find_config_index_for_source(source);
	if (idx < 0) return;

	float volume = (float)calldata_float(cd, "volume");
	auto &sources = AsioConfig::get()->getSources();
	sources[idx].volume = volume;
	AsioConfig::get()->save();
	if (settings_dialog) {
		settings_dialog->updateSourceVolume(sources[idx].sourceUuid, volume);
	}
}

// Signal callback: audio monitoring type changed
static void on_audio_monitoring_changed(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	if (!source) return;
	
	// Skip if we're in the middle of creating sources
	if (creating_sources) return;

	int idx = find_config_index_for_source(source);
	if (idx < 0) return;

	int monitoringType = (int)calldata_int(cd, "type");
	auto &sources = AsioConfig::get()->getSources();
	sources[idx].monitoringType = monitoringType;
	AsioConfig::get()->save();
	if (settings_dialog) {
		settings_dialog->updateSourceMonitoring(sources[idx].sourceUuid, monitoringType);
	}
}

// Signal callback: audio balance changed
static void on_audio_balance_changed(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	if (!source) return;
	
	// Skip if we're in the middle of creating sources
	if (creating_sources) return;

	int idx = find_config_index_for_source(source);
	if (idx < 0) return;

	float balance = (float)calldata_float(cd, "balance");
	auto &sources = AsioConfig::get()->getSources();
	sources[idx].balance = balance;
	AsioConfig::get()->save();
	if (settings_dialog) {
		settings_dialog->updateSourceBalance(sources[idx].sourceUuid, balance);
	}
}

// Signal callback: source flags updated (for mono)
static void on_update_flags(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	if (!source) return;
	
	// Skip if we're in the middle of creating sources
	if (creating_sources) return;

	int idx = find_config_index_for_source(source);
	if (idx < 0) return;

	uint32_t flags = (uint32_t)calldata_int(cd, "flags");
	bool forceMono = (flags & OBS_SOURCE_FLAG_FORCE_MONO) != 0;
	auto &sources = AsioConfig::get()->getSources();
	sources[idx].forceMono = forceMono;
	AsioConfig::get()->save();
	if (settings_dialog) {
		settings_dialog->updateSourceMono(sources[idx].sourceUuid, forceMono);
	}
}

// Signal callback: audio mixers changed (track selection)
static void on_audio_mixers_changed(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	if (!source) return;
	
	// Skip if we're in the middle of creating sources
	if (creating_sources) return;

	int idx = find_config_index_for_source(source);
	if (idx < 0) return;

	uint32_t mixers = (uint32_t)calldata_int(cd, "mixers");
	auto &sources = AsioConfig::get()->getSources();
	sources[idx].audioMixers = mixers;
	AsioConfig::get()->save();
	if (settings_dialog) {
		settings_dialog->updateSourceAudioMixers(sources[idx].sourceUuid, mixers);
	}
}

// Signal callback: audio activated (show in mixer)
static void on_audio_activate(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	if (!source) return;
	
	// Skip if we're in the middle of creating sources
	if (creating_sources) return;

	int idx = find_config_index_for_source(source);
	if (idx < 0) return;

	auto &sources = AsioConfig::get()->getSources();
	sources[idx].audioActive = true;
	AsioConfig::get()->save();
	if (settings_dialog) {
		settings_dialog->updateSourceAudioActive(sources[idx].sourceUuid, true);
	}
}

// Signal callback: audio deactivated (hide from mixer)
static void on_audio_deactivate(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	if (!source) return;
	
	// Skip if we're in the middle of creating sources
	if (creating_sources) return;

	int idx = find_config_index_for_source(source);
	if (idx < 0) return;

	auto &sources = AsioConfig::get()->getSources();
	sources[idx].audioActive = false;
	AsioConfig::get()->save();
	if (settings_dialog) {
		settings_dialog->updateSourceAudioActive(sources[idx].sourceUuid, false);
	}
}

// Connect signals to all existing filters of a source
static void connect_existing_filters(obs_source_t *source)
{
	// We need to iterate over existing filters and attach update listener
	// Note: obs_source_backup_filters returns data array, not sources.
	// To get actual filter sources we'd need to use obs_source_enum_filters if available,
	// but standard API might not expose a safe way to iterate filter *sources* directly easily without callback?
	// Actually, obs_source_enum_filters takes a callback.
	
	obs_source_enum_filters(source, [](obs_source_t *parent, obs_source_t *filter, void *param) {
		UNUSED_PARAMETER(parent);
		UNUSED_PARAMETER(param);
		
		signal_handler_t *sh = obs_source_get_signal_handler(filter);
		if (sh) {
			// Check if already connected? signal_handler_connect is safe to call multiple times?
			// It might duplicate. But we only call this on creation.
			signal_handler_connect(sh, "update", on_filter_settings_update, parent);
		}
	}, nullptr);
}

// Connect signal handlers to a source
static void connect_source_signals(obs_source_t *source)
{
	signal_handler_t *sh = obs_source_get_signal_handler(source);
	if (sh) {
		signal_handler_connect(sh, "rename", on_source_rename, nullptr);
		signal_handler_connect(sh, "update", on_source_update, nullptr);
		signal_handler_connect(sh, "filter_add", on_filter_added, nullptr);
		signal_handler_connect(sh, "filter_remove", on_filter_changed, nullptr);
		signal_handler_connect(sh, "reorder_filters", on_filter_changed, nullptr);
		
		// Audio signals
		signal_handler_connect(sh, "mute", on_mute_changed, nullptr);
		signal_handler_connect(sh, "volume", on_volume_changed, nullptr);
		signal_handler_connect(sh, "audio_monitoring", on_audio_monitoring_changed, nullptr);
		signal_handler_connect(sh, "audio_balance", on_audio_balance_changed, nullptr);
		signal_handler_connect(sh, "update_flags", on_update_flags, nullptr);
		signal_handler_connect(sh, "audio_mixers", on_audio_mixers_changed, nullptr);
		signal_handler_connect(sh, "audio_activate", on_audio_activate, nullptr);
		signal_handler_connect(sh, "audio_deactivate", on_audio_deactivate, nullptr);
	}
	
	// Also connect to any existing filters (restored from config)
	connect_existing_filters(source);
}

// Disconnect signal handlers from a source
static void disconnect_source_signals(obs_source_t *source)
{
	signal_handler_t *sh = obs_source_get_signal_handler(source);
	if (sh) {
		signal_handler_disconnect(sh, "rename", on_source_rename, nullptr);
		signal_handler_disconnect(sh, "update", on_source_update, nullptr);
		signal_handler_disconnect(sh, "filter_add", on_filter_added, nullptr);
		signal_handler_disconnect(sh, "filter_remove", on_filter_changed, nullptr);
		signal_handler_disconnect(sh, "reorder_filters", on_filter_changed, nullptr);
		
		// Audio signals
		signal_handler_disconnect(sh, "mute", on_mute_changed, nullptr);
		signal_handler_disconnect(sh, "volume", on_volume_changed, nullptr);
		signal_handler_disconnect(sh, "audio_monitoring", on_audio_monitoring_changed, nullptr);
		signal_handler_disconnect(sh, "audio_balance", on_audio_balance_changed, nullptr);
		signal_handler_disconnect(sh, "update_flags", on_update_flags, nullptr);
		signal_handler_disconnect(sh, "audio_mixers", on_audio_mixers_changed, nullptr);
		signal_handler_disconnect(sh, "audio_activate", on_audio_activate, nullptr);
		signal_handler_disconnect(sh, "audio_deactivate", on_audio_deactivate, nullptr);
	}
	
	// Disconnect from filters
	obs_source_enum_filters(source, [](obs_source_t *parent, obs_source_t *filter, void *param) {
		UNUSED_PARAMETER(parent);
		UNUSED_PARAMETER(param);
		signal_handler_t *sh = obs_source_get_signal_handler(filter);
		if (sh) {
			signal_handler_disconnect(sh, "update", on_filter_settings_update, parent);
		}
	}, nullptr);
}

// Helper to find existing source by name from our managed list
static obs_source_t* find_managed_source_by_name(const char* name) {
	for (const auto& entry : asio_sources) {
		if (entry.source) {
			const char* srcName = obs_source_get_name(entry.source);
			if (srcName && strcmp(srcName, name) == 0) {
				return entry.source;
			}
		}
	}
	return nullptr;
}

void createSources()
{
	// Set guard flag to prevent signal handlers from modifying config
	creating_sources = true;

	// 1. Detach all current sources from their canvas channels
	// We do this first so that channels are free to be reassigned
	for (auto &entry : asio_sources) {
		if (entry.source) {
			obs_canvas_t *canvas = get_canvas_for_uuid(entry.canvasUuid);
			if (canvas) {
				if (entry.channel > 0 && entry.channel < MAX_CHANNELS) {
					obs_canvas_set_channel(canvas, entry.channel - 1, nullptr); // OBS uses 0-indexed channels
				}
				obs_canvas_release(canvas);
			}
		}
	}

	// 2. Build map of existing managed sources by UUID for reuse
	// UUID is stable across name changes and unique per source
	std::map<std::string, std::pair<int, obs_source_t*>> reusable_sources; // uuid -> (oldChannel, source)
	for (auto &entry : asio_sources) {
		if (entry.source) {
			const char *uuid = obs_source_get_uuid(entry.source);
			if (uuid) {
				reusable_sources[uuid] = {entry.channel, entry.source};
			}
		}
	}

	// 3. Prepare new list
	std::vector<AsioSourceEntry> new_asio_sources;
	auto &configs = AsioConfig::get()->getSources();
	
	obs_log(LOG_INFO, "createSources: %zu existing sources, %d configs, %zu reusable",
		asio_sources.size(), configs.size(), reusable_sources.size());

	for (int i = 0; i < configs.size(); i++) {
		auto &cfg = configs[i];
		if (!cfg.enabled) {
			continue;
		}

		obs_source_t *source = nullptr;
		std::string configName = cfg.name.toUtf8().constData();
		std::string configUuid = cfg.sourceUuid.toUtf8().constData();
		int channel = cfg.outputChannel;
		
		// Only valid channels (1-MAX_CHANNELS) will be assigned; -1 or invalid = no channel
		bool validChannel = channel >= 1 && channel <= MAX_CHANNELS;

		// Try to find existing source by UUID
		auto it = configUuid.empty() ? reusable_sources.end() : reusable_sources.find(configUuid);
		if (it != reusable_sources.end()) {
			source = it->second.second;
			int oldChannel = it->second.first;
			
			// Sync name: if config name differs from source name, update source
			const char* currentName = obs_source_get_name(source);
			if (currentName && configName != currentName) {
				obs_source_set_name(source, configName.c_str());
				obs_log(LOG_INFO, "Renamed source '%s' -> '%s'", currentName, configName.c_str());
			}
			
			// If source was on a channel and is now unbound, clear the old channel
			if (oldChannel > 0 && !validChannel) {
				obs_canvas_t *oldCanvas = get_canvas_for_uuid(cfg.canvas);
				obs_canvas_set_channel(oldCanvas, oldChannel - 1, nullptr);
				obs_log(LOG_INFO, "Cleared channel %d (source '%s' now unbound)", oldChannel, configName.c_str());
				obs_canvas_release(oldCanvas);
			}
			
			obs_log(LOG_INFO, "Reused source '%s' by UUID (channel %d -> %d)", 
				configName.c_str(), oldChannel, channel);
			reusable_sources.erase(it);
		}

		if (!source) {
			// Check if source type is available (plugin might not be installed)
			if (!source_type_exists(cfg.sourceType.toUtf8().constData())) {
				obs_log(LOG_WARNING, "Source type '%s' not available, skipping '%s'",
					cfg.sourceType.toUtf8().constData(), configName.c_str());
				continue;
			}
			
			// Create new source
			// Parse stored settings from JSON
			QJsonDocument doc(cfg.sourceSettings);
			obs_data_t *settings = obs_data_create_from_json(
				doc.toJson(QJsonDocument::Compact).constData());
			if (!settings) settings = obs_data_create();

			// a hacky way to fix the dup source creation where the built-in sources (Desktop-Audio and Mix/Aux)
			// channel assigned source is saved and restored before us.
			// special check for sources of channels 2-7 (Desktop-Audio and Mix/Aux)
			if (asio_sources.empty()) { // is first attempt (loading from saved)
				if (channel >= 2 && channel <= 7) {
					obs_canvas_t *canvas = get_canvas_for_uuid(cfg.canvas);
					if (const auto src = obs_canvas_get_channel(canvas, channel - 1)) {
						obs_log(LOG_WARNING, "Source PreExisting at channel: %d", channel);
						obs_canvas_set_channel(canvas, channel - 1, nullptr); // OBS uses 0-indexed channels
						// just rename it to prevent clashes
						obs_source_set_name(src, QString::fromUtf8("%1_").arg(configName.c_str()).toUtf8().constData());
						obs_source_release(src);
					}
					obs_canvas_release(canvas);
				}
			}

			source = obs_source_create(
				cfg.sourceType.toUtf8().constData(),
				configName.c_str(),
				settings, nullptr
			);
			
			obs_data_release(settings);

			if (source) {
				obs_source_set_hidden(source, true);
				
				// Check if OBS renamed the source (happens if duplicate name existed)
				const char *actualName = obs_source_get_name(source);
				if (actualName && configName != actualName) {
					obs_log(LOG_WARNING, "OBS renamed source '%s' -> '%s' (duplicate existed)", 
						configName.c_str(), actualName);
					cfg.name = QString::fromUtf8(actualName);
					configName = actualName;
					
					// Update settings dialog if open
					if (settings_dialog) {
						settings_dialog->updateSourceNameByIndex(i, cfg.name);
					}
				}

				// Restore filters from config
				if (!cfg.sourceFilters.isEmpty()) {
					QJsonObject wrapper;
					wrapper["filters"] = cfg.sourceFilters;
					QJsonDocument doc(wrapper);
					
					obs_data_t *filterData = obs_data_create_from_json(
						doc.toJson(QJsonDocument::Compact).constData());
						
					if (filterData) {
						obs_data_array_t *filterArray = obs_data_get_array(filterData, "filters");
						if (filterArray) {
							obs_source_restore_filters(source, filterArray);
							obs_data_array_release(filterArray);
							obs_log(LOG_INFO, "Restored filters for '%s'", configName.c_str());
						}
						obs_data_release(filterData);
					}
				}

				// Connect signal handlers
				connect_source_signals(source);
			}
		}

		if (source) {
			// Apply audio settings from config
			apply_audio_settings(source, cfg);
			
			// Store source UUID in config
			const char *uuid = obs_source_get_uuid(source);
			if (uuid) {
				cfg.sourceUuid = QString::fromUtf8(uuid);
				// Update settings dialog if open
				if (settings_dialog) {
					settings_dialog->updateSourceUuid(i, cfg.sourceUuid);
				}
			}
			
			// Assign to canvas channel (only if valid channel 1-MAX_CHANNELS)
			if (validChannel) {
				obs_canvas_t *canvas = get_canvas_for_uuid(cfg.canvas);
				obs_canvas_set_channel(canvas, channel - 1, source); // OBS uses 0-indexed channels
				obs_log(LOG_INFO, "Audio source '%s' (uuid: %s) assigned to channel %d (canvas: %s)",
					configName.c_str(), uuid ? uuid : "?", channel, cfg.canvas.isEmpty() ? "main" : cfg.canvas.toUtf8().constData());
				obs_canvas_release(canvas);
			} else {
				obs_log(LOG_INFO, "Audio source '%s' (uuid: %s) created (no channel assigned)",
					configName.c_str(), uuid ? uuid : "?");
			}
			new_asio_sources.push_back({channel, cfg.canvas, source});
		} else {
			obs_log(LOG_ERROR, "Failed to get/create ASIO source '%s'.",
				configName.c_str());
		}
	}

	// 4. Clean up unused sources - must call obs_source_remove to actually destroy them
	for (auto &[uuid, channelSourcePair] : reusable_sources) {
		auto &[oldChannel, src] = channelSourcePair;
		const char *srcName = obs_source_get_name(src);
		disconnect_source_signals(src);
		obs_source_set_audio_active(src, false); // else it won't free up the source
		
		// obs_source_remove removes from OBS's internal source list and triggers destruction
		// obs_source_release decrements our reference count
		obs_source_remove(src);
		const bool removed = obs_source_removed(src);
		obs_source_release(src);
		obs_log(LOG_INFO, "Removed source '%s' (uuid: %s, was on channel %d), %d",
			srcName ? srcName : "?", uuid.c_str(), oldChannel, removed);
	}
	
	asio_sources = std::move(new_asio_sources);

	// Clear guard flag - signal handlers can now modify config safely
	creating_sources = false;

	// Show error if no sources were created but there are configs
	if (asio_sources.empty() && !configs.isEmpty()) {
		// Only show error if we really have 0 sources active (all failed)
		// But maybe some failed and some succeeded.
	}
}

void cleanup()
{
	// Release and remove all managed sources
	for (auto &entry : asio_sources) {
		if (entry.source) {
			// Disconnect signals first
			disconnect_source_signals(entry.source);
			obs_source_set_audio_active(entry.source, false); // else it won't free up the source
			// Clear the canvas channel
			if (obs_canvas_t *canvas = get_canvas_for_uuid(entry.canvasUuid)) {
				if (entry.channel > 0) {
					obs_canvas_set_channel(canvas, entry.channel - 1, nullptr);
				}
				obs_canvas_release(canvas);
			}
			// Remove from OBS source list and release our reference
			obs_source_remove(entry.source);
			obs_source_release(entry.source);
		}
	}
	asio_sources.clear();
}

void refreshAsioSources()
{
	// Reload config and recreate sources
	AsioConfig::get()->load();
	createSources();
}

void on_obs_evt(obs_frontend_event event, void *data)
{
	UNUSED_PARAMETER(data);

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		createSources();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
	case OBS_FRONTEND_EVENT_EXIT:
		cleanup();
		break;
	default:
		break;
	}
}

static void show_settings_dialog(void* data)
{
	UNUSED_PARAMETER(data);

	if (!settings_dialog) {
		auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		settings_dialog = new AsioSettingsDialog(mainWindow);
	}
	settings_dialog->toggle_show_hide();
}

static void show_channels_view(void* data)
{
	UNUSED_PARAMETER(data);
	
	if (!channels_view) {
		auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		channels_view = new ChannelsView(mainWindow);
	}
	channels_view->show();
	channels_view->raise();
	channels_view->activateWindow();
}

#ifdef __cplusplus
extern "C" {
#endif

bool on_plugin_load()
{
	// Check if we have all the deps loaded
	return true;
}

void on_plugin_loaded()
{
	obs_frontend_add_event_callback(on_obs_evt, nullptr);

	// Add Tools menu item
	obs_frontend_add_tools_menu_item(
		obs_module_text("AsioChannels"),
		show_settings_dialog,
		nullptr
	);

	// Add Channels View menu item
	obs_frontend_add_tools_menu_item(
		obs_module_text("ChannelsView.Title"), // "Channels" or similar
		show_channels_view,
		nullptr
	);
	
	// Create and register the Super Mixer dock
	auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	mixer_dock = new MixerDock(mainWindow);
	obs_frontend_add_dock_by_id("SuperMixerDock", obs_module_text("SuperMixer.Title"), mixer_dock);
}

void on_plugin_unload()
{
	obs_frontend_remove_event_callback(on_obs_evt, nullptr);

	// Clean up sources FIRST - this disconnects all signal handlers
	// Must happen before deleting dialogs to prevent signal handlers
	// from accessing deleted dialog pointers
	cleanup();

	// these are most probably won't execute, by this time obs gc's the main window which is the parent of these widgets
	{
		if (settings_dialog) {
			settings_dialog->close();
			delete settings_dialog;
		}

		if (channels_view) {
			delete channels_view;
		}

		if (mixer_dock) {
			// Dock is managed by OBS frontend, just remove our reference
			obs_frontend_remove_dock("SuperMixerDock");
			delete mixer_dock;
		}
	}

	// Clean up config singleton last
	AsioConfig::cleanup();
}

#ifdef __cplusplus
}
#endif
