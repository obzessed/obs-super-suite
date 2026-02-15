#include <plugin-support.h>

#pragma region stdlib_headers
#include <utility>
#include <vector>
#include <string>
#include <map>
#pragma endregion

#pragma region obs_headers
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <callback/signal.h>
#pragma endregion

#pragma region qt_headers
#include <QJsonDocument>
#include <QMessageBox>
#include <QMainWindow>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>
#pragma endregion

#pragma region plugin_headers
#include "super_suite.h"

#include "models/audio_channel_source_config.h"

#include "docks/mixer_dock.h"
#include "docks/wrapper_test_dock.h"
#include "docks/test_midi_dock.hpp"
#include "docks/test_super_dock.hpp"
#include "docks/sourcerer/sourcerer_scenes_dock.hpp"
#include "docks/sourcerer/sourcerer_sources_dock.hpp"

#include "utils/midi/midi_router.hpp"

#include "super/core/control_registry.hpp"

#include "dialogs/canvas_manager.h"
#include "dialogs/audio_channels.h"
#include "dialogs/audio_channels_support.h"
#include "dialogs/outputs_viewer.h"
#include "dialogs/encoders_viewer.h"
#include "dialogs/channels_viewer.h"
#include "dialogs/browser_manager.h"

#include "windows/dock_window_manager.h"
#include "windows/encoding_graph_window.h"
#pragma endregion

static struct GlobalDialogs {
	QPointer<AudioChannelsDialog> audio_channels;
	QPointer<ChannelsDialog> canvas_channels;
	QPointer<OutputsViewer> outputs_viewer;
	QPointer<EncodersViewer> encoders_viewer;
	QPointer<DockWindowManager> dock_window_manager;
	QPointer<CanvasManager> canvas_manager;
	QPointer<BrowserManager> browser_dock_manager;
	QPointer<EncodingGraphWindow> encoding_graph;
} g_dialogs;

static struct GlobalDocks {
	QPointer<MixerDock> super_mixer;
	QPointer<WrapperTestDock> wrapper_test;
	QPointer<SourcererScenesDock> sourcerer_scenes;
	QPointer<SourcererSourcesDock> sourcerer_sources;
	QPointer<TestMidiDock> test_midi;
	QPointer<TestSuperDock> test_super;
} g_docks;

static void save_callback(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		if (g_dialogs.dock_window_manager) {
			QJsonObject data = g_dialogs.dock_window_manager->saveToConfig();
			QJsonDocument doc(data);
			QString jsonStr = doc.toJson(QJsonDocument::Compact);
			obs_data_set_string(save_data, "DockWindowManager", jsonStr.toUtf8().constData());
		}

		if (g_dialogs.browser_dock_manager) {
			QJsonObject data = g_dialogs.browser_dock_manager->saveToConfig();
			QJsonDocument doc(data);
			QString jsonStr = doc.toJson(QJsonDocument::Compact);
			obs_data_set_string(save_data, "BrowserManager", jsonStr.toUtf8().constData());
		}

		if (g_docks.sourcerer_sources) {
			QJsonObject data = g_docks.sourcerer_sources->Save();
			QJsonDocument doc(data);
			QString jsonStr = doc.toJson(QJsonDocument::Compact);
			obs_data_set_string(save_data, "SourcererSources", jsonStr.toUtf8().constData());
		}

		if (g_docks.sourcerer_scenes) {
			QJsonObject data = g_docks.sourcerer_scenes->Save();
			QJsonDocument doc(data);
			QString jsonStr = doc.toJson(QJsonDocument::Compact);
			obs_data_set_string(save_data, "SourcererScenes", jsonStr.toUtf8().constData());
		}

		// MIDI Router bindings
		{
			QJsonObject data = MidiRouter::instance()->save();
			QJsonDocument doc(data);
			QString jsonStr = doc.toJson(QJsonDocument::Compact);
			obs_data_set_string(save_data, "MidiRouter", jsonStr.toUtf8().constData());
		}

		// Test MIDI Dock state
		if (g_docks.test_midi) {
			QJsonObject data = g_docks.test_midi->save_state();
			QJsonDocument doc(data);
			QString jsonStr = doc.toJson(QJsonDocument::Compact);
			obs_data_set_string(save_data, "TestMidiDock", jsonStr.toUtf8().constData());
		}

		// Test Super Dock state
		if (g_docks.test_super) {
			QJsonObject data = g_docks.test_super->save_state();
			QJsonDocument doc(data);
			QString jsonStr = doc.toJson(QJsonDocument::Compact);
			obs_data_set_string(save_data, "TestSuperDock", jsonStr.toUtf8().constData());
		}

		// ControlRegistry persistent variables
		{
			QJsonObject data = super::ControlRegistry::instance().save_variables();
			if (!data.isEmpty()) {
				QJsonDocument doc(data);
				QString jsonStr = doc.toJson(QJsonDocument::Compact);
				obs_data_set_string(save_data, "ControlVariables", jsonStr.toUtf8().constData());
			}
		}
	} else {
		// Loading
		const char *dockWindowJsonStr = obs_data_get_string(save_data, "DockWindowManager");
		if (dockWindowJsonStr && *dockWindowJsonStr) {
			if (!g_dialogs.dock_window_manager) {
				auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
				g_dialogs.dock_window_manager = new DockWindowManager(mainWindow);
			}

			QJsonDocument doc = QJsonDocument::fromJson(QByteArray(dockWindowJsonStr));
			if (!doc.isNull() && doc.isObject()) {
				g_dialogs.dock_window_manager->loadFromConfig(doc.object());
			}
		}

		const char *browserJsonStr = obs_data_get_string(save_data, "BrowserManager");
		if (browserJsonStr && *browserJsonStr) {
			// Defer loading until OBS_FRONTEND_EVENT_FINISHED_LOADING
			auto browser_manager_config_data = QByteArray(browserJsonStr);
			// Load Browser Manager Config
			if (!browser_manager_config_data.isEmpty()) {
				if (!g_dialogs.browser_dock_manager) {
					auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
					g_dialogs.browser_dock_manager = new BrowserManager(mainWindow);
				}
				if (const QJsonDocument doc = QJsonDocument::fromJson(browser_manager_config_data);
				    !doc.isNull() && doc.isObject()) {
					g_dialogs.browser_dock_manager->setDeferredLoad(true);
					g_dialogs.browser_dock_manager->loadFromConfig(doc.object());
				}
			}
		}

		const char *sourcesJsonStr = obs_data_get_string(save_data, "SourcererSources");
		if (sourcesJsonStr && *sourcesJsonStr && g_docks.sourcerer_sources) {
			QJsonDocument doc = QJsonDocument::fromJson(QByteArray(sourcesJsonStr));
			if (!doc.isNull() && doc.isObject()) {
				g_docks.sourcerer_sources->Load(doc.object());
			}
		}

		const char *scenesJsonStr = obs_data_get_string(save_data, "SourcererScenes");
		if (scenesJsonStr && *scenesJsonStr && g_docks.sourcerer_scenes) {
			QJsonDocument doc = QJsonDocument::fromJson(QByteArray(scenesJsonStr));
			if (!doc.isNull() && doc.isObject()) {
				g_docks.sourcerer_scenes->Load(doc.object());
			}
		}

		// MIDI Router bindings
		const char *midiJsonStr = obs_data_get_string(save_data, "MidiRouter");
		if (midiJsonStr && *midiJsonStr) {
			QJsonDocument doc = QJsonDocument::fromJson(QByteArray(midiJsonStr));
			if (!doc.isNull() && doc.isObject()) {
				MidiRouter::instance()->load(doc.object());
			}
		}

		// Test MIDI Dock state
		const char *testMidiJsonStr = obs_data_get_string(save_data, "TestMidiDock");
		if (testMidiJsonStr && *testMidiJsonStr && g_docks.test_midi) {
			QJsonDocument doc = QJsonDocument::fromJson(QByteArray(testMidiJsonStr));
			if (!doc.isNull() && doc.isObject()) {
				g_docks.test_midi->load_state(doc.object());
			}
		}

		// Test Super Dock state
		const char *testSuperJsonStr = obs_data_get_string(save_data, "TestSuperDock");
		if (testSuperJsonStr && *testSuperJsonStr && g_docks.test_super) {
			QJsonDocument doc = QJsonDocument::fromJson(QByteArray(testSuperJsonStr));
			if (!doc.isNull() && doc.isObject()) {
				g_docks.test_super->load_state(doc.object());
			}
		}

		// ControlRegistry persistent variables
		const char *ctrlVarsJsonStr = obs_data_get_string(save_data, "ControlVariables");
		if (ctrlVarsJsonStr && *ctrlVarsJsonStr) {
			QJsonDocument doc = QJsonDocument::fromJson(QByteArray(ctrlVarsJsonStr));
			if (!doc.isNull() && doc.isObject()) {
				super::ControlRegistry::instance().load_variables(doc.object());
			}
		}
	}
}

void load_browser_docks()
{
	if (!g_dialogs.browser_dock_manager) {
		// TODO: recreate of dead
	} else {
		g_dialogs.browser_dock_manager->onOBSBrowserReady();
	}
}

void unload_browser_docks()
{
	delete g_dialogs.browser_dock_manager;
	g_dialogs.browser_dock_manager = nullptr;
}

void on_obs_evt(obs_frontend_event event, void *data)
{
	UNUSED_PARAMETER(data);

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		// save_callback with saving=false is called by OBS frontend automatically
		// during profile load, which happens before or around FINISHED_LOADING.
		// So we don't need manual load here if save_callback works as expected.
		createSources();
		if (g_dialogs.browser_dock_manager) {
			g_dialogs.browser_dock_manager->onOBSBrowserReady();
		}
		if (g_docks.sourcerer_scenes) {
			g_docks.sourcerer_scenes->FrontendReady();
		}

		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED: {
		BrowserManager::cleanup(false);
		load_browser_docks();
		break;
	}
	case OBS_FRONTEND_EVENT_PROFILE_CHANGING: {
		unload_browser_docks();
		break;
	}
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		createSources();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		audio_sources_cleanup();
		break;
	case OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN:
	case OBS_FRONTEND_EVENT_EXIT:
		obs_queue_task(
			OBS_TASK_GRAPHICS,
			[](void *) {
				obs_queue_task(
					OBS_TASK_UI,
					[](void *) {
						unload_browser_docks();
						BrowserManager::cleanup();
					},
					nullptr, false);
			},
			nullptr, false);
		audio_sources_cleanup();
		break;
	default:
		break;
	}
}

static void show_settings_dialog(void *data)
{
	UNUSED_PARAMETER(data);

	if (!g_dialogs.audio_channels) {
		auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		g_dialogs.audio_channels = new AudioChannelsDialog(mainWindow);
	}
	g_dialogs.audio_channels->toggle_show_hide();
}

static void show_channels_view(void *data)
{
	UNUSED_PARAMETER(data);

	if (!g_dialogs.canvas_channels) {
		auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		g_dialogs.canvas_channels = new ChannelsDialog(mainWindow);
	}
	g_dialogs.canvas_channels->show();
	g_dialogs.canvas_channels->raise();
	g_dialogs.canvas_channels->activateWindow();
}

static void show_canvas_manager(void *data)
{
	UNUSED_PARAMETER(data);

	if (!g_dialogs.canvas_manager) {
		auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		g_dialogs.canvas_manager = new CanvasManager(mainWindow);
	}
	g_dialogs.canvas_manager->show();
	g_dialogs.canvas_manager->raise();
	g_dialogs.canvas_manager->activateWindow();
}

static void show_outputs_viewer(void *data)
{
	UNUSED_PARAMETER(data);

	if (!g_dialogs.outputs_viewer) {
		auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		g_dialogs.outputs_viewer = new OutputsViewer(mainWindow);
	}
	g_dialogs.outputs_viewer->show();
	g_dialogs.outputs_viewer->raise();
	g_dialogs.outputs_viewer->activateWindow();
}

static void show_encoders_viewer(void *data)
{
	UNUSED_PARAMETER(data);

	if (!g_dialogs.encoders_viewer) {
		auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		g_dialogs.encoders_viewer = new EncodersViewer(mainWindow);
	}
	g_dialogs.encoders_viewer->show();
	g_dialogs.encoders_viewer->raise();
	g_dialogs.encoders_viewer->activateWindow();
}

static void show_dock_window_manager(void *data)
{
	UNUSED_PARAMETER(data);

	if (!g_dialogs.dock_window_manager) {
		auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		g_dialogs.dock_window_manager = new DockWindowManager(mainWindow);
	}
	g_dialogs.dock_window_manager->show();
	g_dialogs.dock_window_manager->raise();
	g_dialogs.dock_window_manager->activateWindow();
}

static void show_browser_manager(void *data)
{
	UNUSED_PARAMETER(data);

	if (!g_dialogs.browser_dock_manager) {
		auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		g_dialogs.browser_dock_manager = new BrowserManager(mainWindow);
	}
	g_dialogs.browser_dock_manager->show();
	g_dialogs.browser_dock_manager->raise();
	g_dialogs.browser_dock_manager->activateWindow();
}

static void show_encoding_graph(void *data)
{
	UNUSED_PARAMETER(data);

	if (!g_dialogs.encoding_graph) {
		auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		// encoding_graph = new EncodingGraphWindow(mainWindow);
		g_dialogs.encoding_graph = new EncodingGraphWindow(nullptr);
	}
	g_dialogs.encoding_graph->show();
	g_dialogs.encoding_graph->raise();
	g_dialogs.encoding_graph->activateWindow();
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
	obs_frontend_add_tools_menu_item(obs_module_text("AsioChannels"), show_settings_dialog, nullptr);

	// Add Channels View menu item
	obs_frontend_add_tools_menu_item(obs_module_text("ChannelsView.Title"), // "Channels" or similar
					 show_channels_view, nullptr);

	// Add Canvas Manager menu item
	obs_frontend_add_tools_menu_item(obs_module_text("CanvasManager.Title"), show_canvas_manager, nullptr);

	// Add Outputs Viewer menu item
	obs_frontend_add_tools_menu_item(obs_module_text("OutputsViewer.Title"), show_outputs_viewer, nullptr);

	// Add Encoders Viewer menu item
	obs_frontend_add_tools_menu_item(obs_module_text("EncodersViewer.Title"), show_encoders_viewer, nullptr);

	// Add Dock Window Manager menu item
	obs_frontend_add_tools_menu_item(obs_module_text("DockWindowManager.Title"), show_dock_window_manager, nullptr);

	// Add Browser Manager menu item
	obs_frontend_add_tools_menu_item(obs_module_text("BrowserManager.Title"), show_browser_manager, nullptr);

	// Add Encoding Graph menu item
	obs_frontend_add_tools_menu_item(obs_module_text("EncodingGraph.Title"), show_encoding_graph, nullptr);

	obs_frontend_add_save_callback(save_callback, nullptr);

	// Try to load initial state
	// Note: obs_frontend_add_save_callback is called for save, but for load we need to
	// explicitly ask for data or wait for a specific event?
	// standard practice: use a separate config file or use the passed save_data?
	// Actually, the save_callback is called with saving=false when loading?
	// Documentation says: "Functions assigned with this are called when saving/loading... "

	// Create and register docks
	auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	g_docks.super_mixer = new MixerDock(mainWindow);
	g_docks.wrapper_test = new WrapperTestDock(mainWindow);
	g_docks.sourcerer_sources = new SourcererSourcesDock(mainWindow);
	g_docks.sourcerer_scenes = new SourcererScenesDock(mainWindow);
	obs_frontend_add_dock_by_id("SuperMixerDock", obs_module_text("SuperMixer.Title"), g_docks.super_mixer);
	obs_frontend_add_dock_by_id("WrapperTestDock", "OBS Wrapper Test", g_docks.wrapper_test);
	obs_frontend_add_dock_by_id("SourcererSources", "Sourcerer Sources", g_docks.sourcerer_sources);
	obs_frontend_add_dock_by_id("SourcererScenes", "Sourcerer Scenes", g_docks.sourcerer_scenes);

	g_docks.test_midi = new TestMidiDock(mainWindow);
	obs_frontend_add_dock_by_id("TestMidiDock", "Test MIDI Dock", g_docks.test_midi);

	g_docks.test_super = new TestSuperDock(mainWindow);
	obs_frontend_add_dock_by_id("TestSuperDock", "Test Super Dock", g_docks.test_super);
}

void on_plugin_unload()
{
	obs_frontend_remove_event_callback(on_obs_evt, nullptr);

	// Clean up sources FIRST - this disconnects all signal handlers
	// Must happen before deleting dialogs to prevent signal handlers
	// from accessing deleted dialog pointers
	audio_sources_cleanup();

	// these are most probably won't execute, by this time obs gc's the main window which is the parent of these widgets
	{
		if (g_dialogs.audio_channels) {
			g_dialogs.audio_channels->close();
			delete g_dialogs.audio_channels;
		}

		if (g_dialogs.canvas_channels) {
			delete g_dialogs.canvas_channels;
		}

		if (g_dialogs.browser_dock_manager) {
			delete g_dialogs.browser_dock_manager;
		}

		if (g_docks.super_mixer) {
			// Dock is managed by OBS frontend, just remove our reference
			obs_frontend_remove_dock("SuperMixerDock");
			delete g_docks.super_mixer;
		}

		if (g_docks.wrapper_test) {
			obs_frontend_remove_dock("WrapperTestDock");
			delete g_docks.wrapper_test;
		}

		if (g_docks.sourcerer_sources) {
			obs_frontend_remove_dock("SourcererSources");
			delete g_docks.sourcerer_sources;
		}

		if (g_docks.sourcerer_scenes) {
			obs_frontend_remove_dock("SourcererScenes");
			delete g_docks.sourcerer_scenes;
		}

		if (g_docks.test_midi) {
			obs_frontend_remove_dock("TestMidiDock");
			delete g_docks.test_midi;
		}

		if (g_docks.test_super) {
			obs_frontend_remove_dock("TestSuperDock");
			delete g_docks.test_super;
		}
	}

	MidiRouter::cleanup();
	AudioChSrcConfig::cleanup();

	BrowserManager::cleanup();
}

#ifdef __cplusplus
}
#endif
