#include <plugin-support.h>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <callback/signal.h>

#include <QMessageBox>
#include <QMainWindow>

#include "super_suite.h"
#include "asio_config.h"
#include "asio_settings.h"

#include <vector>
#include <utility>
#include <map>
#include <string>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// Store multiple ASIO sources with their channel assignments
static std::vector<std::pair<int, obs_source_t *>> asio_sources; // (channel, source)
static AsioSettingsDialog *settings_dialog = nullptr;

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

// Signal callback: source renamed
static void on_source_rename(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(data);

	obs_source_t *source = calldata_get_pointer<obs_source_t>(cd, "source");
	const char *new_name = calldata_string(cd, "new_name");
	const char *prev_name = calldata_string(cd, "prev_name");

	if (!source || !new_name || !prev_name) return;

	// Find this source in our list and update config
	for (size_t i = 0; i < asio_sources.size(); i++) {
		if (asio_sources[i].second == source) {
			auto &sources = AsioConfig::get()->getSources();
			if ((int)i < sources.size()) {
				sources[(int)i].name = QString::fromUtf8(new_name);
				AsioConfig::get()->save();
				obs_log(LOG_INFO, "ASIO source renamed: '%s' -> '%s'", prev_name, new_name);
			}
			break;
		}
	}
}

// Signal callback: source settings updated
static void on_source_update(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(data);

	const obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	if (!source) return;

	// Find this source in our list and save settings to config
	for (size_t i = 0; i < asio_sources.size(); i++) {
		if (asio_sources[i].second == source) {
			auto &sources = AsioConfig::get()->getSources();
			if ((int)i < sources.size()) {
				obs_data_t *settings = obs_source_get_settings(source);
				if (settings) {
					const char *json = obs_data_get_json(settings);
					if (json) {
						QJsonParseError error;
						QJsonDocument doc = QJsonDocument::fromJson(QByteArray(json), &error);
						if (error.error == QJsonParseError::NoError && doc.isObject()) {
							sources[(int)i].sourceSettings = doc.object();
							AsioConfig::get()->save();
							if (settings_dialog) {
								settings_dialog->updateSourceSettings(asio_sources[i].first, sources[(int)i].sourceSettings);
							}
							obs_log(LOG_INFO, "ASIO source settings updated for '%s'",
								obs_source_get_name(source));
						}
					}
					obs_data_release(settings);
				}
			}
			break;
		}
	}
}

// Helper function to save filter state for a source
static void save_source_filters(obs_source_t *source, int index)
{
	auto &sources = AsioConfig::get()->getSources();
	if (index >= sources.size()) return;

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
					sources[index].sourceFilters = root["filters"].toArray();
					AsioConfig::get()->save();
					if (settings_dialog) {
						settings_dialog->updateSourceFilters(asio_sources[index].first, sources[index].sourceFilters);
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

	// Find this source in our list and save filters to config
	for (size_t i = 0; i < asio_sources.size(); i++) {
		if (asio_sources[i].second == source) {
			save_source_filters(source, (int)i);
			break;
		}
	}
}

// Signal callback: filter settings updated
static void on_filter_settings_update(void *data, calldata_t *cd)
{
	UNUSED_PARAMETER(cd);
	
	// 'data' is the parent source
	obs_source_t *parent_source = (obs_source_t *)data;
	if (!parent_source) return;

	// Verify parent source is still valid and in our list
	for (size_t i = 0; i < asio_sources.size(); i++) {
		if (asio_sources[i].second == parent_source) {
			save_source_filters(parent_source, (int)i);
			break;
		}
	}
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

	// Save filters
	for (size_t i = 0; i < asio_sources.size(); i++) {
		if (asio_sources[i].second == source) {
			save_source_filters(source, (int)i);
			break;
		}
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
	for (const auto& pair : asio_sources) {
		if (pair.second) {
			const char* srcName = obs_source_get_name(pair.second);
			if (srcName && strcmp(srcName, name) == 0) {
				return pair.second;
			}
		}
	}
	return nullptr;
}

void createSources()
{
	// 1. Detach all current sources from their output channels
	// We do this first so that channels are free to be reassigned
	for (auto &[channel, src] : asio_sources) {
		if (src) {
			obs_set_output_source(channel, nullptr);
		}
	}

	// 2. Build map of existing managed sources for reuse
	std::map<std::string, obs_source_t*> reusable_sources;
	for (auto &[channel, src] : asio_sources) {
		if (src) {
			const char* name = obs_source_get_name(src);
			if (name) reusable_sources[name] = src;
		}
	}

	// 3. Prepare new list
	std::vector<std::pair<int, obs_source_t *>> new_asio_sources;
	const auto &configs = AsioConfig::get()->getSources();

	for (const auto &cfg : configs) {
		if (!cfg.enabled) {
			continue;
		}

		obs_source_t *source = nullptr;
		std::string name = cfg.name.toUtf8().constData();

		// Try to reuse existing source
		auto it = reusable_sources.find(name);
		if (it != reusable_sources.end()) {
			source = it->second;
			reusable_sources.erase(it); // Mark as used
			
			// Update settings if needed (optional, or we assume config matches)
			// For now, simpler to leave settings as is on the running source, 
			// unless we explicitly want to force config over current state.
			// Given we listen to updates, they should be in sync.
		} else {
			// Create new source
			// Parse stored settings from JSON
			QJsonDocument doc(cfg.sourceSettings);
			obs_data_t *settings = obs_data_create_from_json(
				doc.toJson(QJsonDocument::Compact).constData());
			if (!settings) settings = obs_data_create();

			source = obs_source_create(
				"asio_input_capture",
				name.c_str(),
				settings, nullptr
			);
			
			obs_data_release(settings);

			if (source) {
				obs_source_set_hidden(source, true);

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
							obs_log(LOG_INFO, "Restored filters for '%s'", name.c_str());
						}
						obs_data_release(filterData);
					}
				}

				// Connect signal handlers
				connect_source_signals(source);
			}
		}

		if (source) {
			// Validate channel range
			int channel = cfg.outputChannel;
			if (channel < ASIO_START_CHANNEL || channel > ASIO_END_CHANNEL) {
				channel = ASIO_START_CHANNEL;
			}

			// Assign to new channel
			obs_set_output_source(channel, source);
			new_asio_sources.emplace_back(channel, source);

			obs_log(LOG_INFO, "ASIO source '%s' assigned to channel %d",
				name.c_str(), channel);
		} else {
			obs_log(LOG_ERROR, "Failed to get/create ASIO source '%s'.",
				name.c_str());
		}
	}

	// 4. Clean up unused sources
	for (auto &[name, src] : reusable_sources) {
		disconnect_source_signals(src);
		obs_source_release(src);
		obs_log(LOG_INFO, "Released unused ASIO source '%s'", name.c_str());
	}
	
	asio_sources = std::move(new_asio_sources);

	// Show error if no sources were created but there are configs
	if (asio_sources.empty() && !configs.isEmpty()) {
		// Only show error if we really have 0 sources active (all failed)
		// But maybe some failed and some succeeded.
	}
}

void cleanup()
{
	// Simply release our references - OBS manages output channel references
	for (auto &[channel, src] : asio_sources) {
		if (src) {
			// Disconnect signals before releasing
			disconnect_source_signals(src);
			// Clear the output channel
			obs_set_output_source(channel, nullptr);
			// Release our reference
			obs_source_release(src);
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
}

void on_plugin_unload()
{
	obs_frontend_remove_event_callback(on_obs_evt, nullptr);

	// Delete settings dialog first (before cleaning up sources)
	// Don't call close() as it triggers saveToConfig which may access cleaned up resources
	if (settings_dialog) {
		settings_dialog->hide(); // Just hide, don't trigger close event
		delete settings_dialog;
		settings_dialog = nullptr;
	}

	// Clean up sources
	cleanup();

	// Clean up config singleton last
	AsioConfig::cleanup();
}

#ifdef __cplusplus
}
#endif
