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
#include "docks/volume_meter_demo_dock.hpp"
#include "docks/daw_mixer_demo_dock.hpp"
#include "docks/s_mixer_demo_dock.hpp"
#include "docks/sourcerer/sourcerer_scenes_dock.hpp"
#include "docks/sourcerer/sourcerer_sources_dock.hpp"
#include "windows/graph_editor_window.hpp"
#include "windows/surface_editor_window.hpp"
#include "windows/audio_matrix.hpp"

#include "utils/midi/midi_router.hpp"
#include "utils/extras/frontend_helper.hpp"
#include "utils/extras/frontend_tweaks.hpp"
#include "utils/extras/libobs_tweaks.hpp"

#include "super/core/control_registry.hpp"

#include "dialogs/canvas_manager.h"
#include "dialogs/audio_channels.h"
#include "dialogs/audio_channels_support.h"
#if ENABLE_BROWSER_DOCKS
#include "dialogs/browser_manager.h"
#endif

#if ENABLE_DOCK_WINDOW_MANAGER
#include "windows/dock_window_manager.h"
#endif
#if ENABLE_ENCODING_GRAPH
#include "windows/encoding_graph_window.h"
#endif
#if ENABLE_OUTPUTS_VIEWER
#include "dialogs/outputs_viewer.h"
#endif
#if ENABLE_ENCODERS_VIEWER
#include "dialogs/encoders_viewer.h"
#endif
#if ENABLE_CHANNELS_VIEWER
#include "dialogs/channels_viewer.h"
#endif

#if ENABLE_TWEAKS_PANEL
#include "windows/tweaks_panel.hpp"
#endif

#include "windows/qt_inspector.hpp"
#pragma endregion

static struct GlobalDialogs {
	QPointer<AudioChannelsDialog> audio_channels;
	QPointer<CanvasManager> canvas_manager;
#if ENABLE_CHANNELS_VIEWER
	QPointer<ChannelsDialog> canvas_channels;
#endif
#if ENABLE_OUTPUTS_VIEWER
	QPointer<OutputsViewer> outputs_viewer;
#endif
#if ENABLE_ENCODERS_VIEWER
	QPointer<EncodersViewer> encoders_viewer;
#endif
#if ENABLE_DOCK_WINDOW_MANAGER
	QPointer<DockWindowManager> dock_window_manager;
#endif
#if ENABLE_ENCODING_GRAPH
	QPointer<EncodingGraphWindow> encoding_graph;
#endif
#if ENABLE_BROWSER_DOCKS
	QPointer<BrowserManager> browser_dock_manager;
#endif
#if ENABLE_GRAPH_EDITORS
	QPointer<GraphEditorWindow> graph_editor;
	QPointer<SurfaceEditorWindow> surface_editor;
#endif
#if ENABLE_TWEAKS_PANEL
	QPointer<TweaksPanel> tweaks_panel;
#endif
	QPointer<QtInspector> qt_inspector;
} g_dialogs;

static struct GlobalInstances {
#if ENABLE_TWEAKS_PANEL
	QPointer<TweaksImpl> tweaks_impl;
#endif
	bool placeholder; // Ensure struct isn't empty if macro is off
} g_instances;

static struct GlobalDocks {
#if ENABLE_SUPER_MIXER_DOCK
	QPointer<MixerDock> super_mixer;
#endif
#if ENABLE_TEST_SUPER_DOCK
	QPointer<WrapperTestDock> wrapper_test;
	QPointer<TestSuperDock> test_super;
#endif
#if ENABLE_SOURCERER_DOCKS
	QPointer<SourcererScenesDock> sourcerer_scenes;
	QPointer<SourcererSourcesDock> sourcerer_sources;
#endif
#if ENABLE_MIDI_DOCKS
	QPointer<TestMidiDock> test_midi;
#endif
#if ENABLE_VOLUME_METER_DOCK
	QPointer<VolumeMeterDemoDock> volume_meter_demo;
#endif
#if ENABLE_DAW_MIXER_DOCK
	QPointer<DawMixerDemoDock> daw_mixer_demo;
#endif
#if ENABLE_S_MIXER_DOCK
	QPointer<SMixerDemoDock> s_mixer_demo;
#endif
} g_docks;

#if ENABLE_VOLUME_METER_DOCK
int volumeMeterDemoStyle = -1;
#endif

static void save_callback(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		// Saving
#if ENABLE_DOCK_WINDOW_MANAGER
		if (g_dialogs.dock_window_manager) {
			QJsonObject data = g_dialogs.dock_window_manager->saveToConfig();
			QJsonDocument doc(data);
			QString jsonStr = doc.toJson(QJsonDocument::Compact);
			obs_data_set_string(save_data, "DockWindowManager", jsonStr.toUtf8().constData());
		}
#endif

#if ENABLE_BROWSER_DOCKS
		if (g_dialogs.browser_dock_manager) {
			QJsonObject data = g_dialogs.browser_dock_manager->saveToConfig();
			QJsonDocument doc(data);
			QString jsonStr = doc.toJson(QJsonDocument::Compact);
			obs_data_set_string(save_data, "BrowserManager", jsonStr.toUtf8().constData());
		}
#endif

#if ENABLE_SOURCERER_DOCKS
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
#endif

		// MIDI Router bindings
		{
			QJsonObject data = MidiRouter::instance()->save();
			QJsonDocument doc(data);
			QString jsonStr = doc.toJson(QJsonDocument::Compact);
			obs_data_set_string(save_data, "MidiRouter", jsonStr.toUtf8().constData());
		}

#if ENABLE_MIDI_DOCKS
		// Test MIDI Dock state
		if (g_docks.test_midi) {
			QJsonObject data = g_docks.test_midi->save_state();
			QJsonDocument doc(data);
			QString jsonStr = doc.toJson(QJsonDocument::Compact);
			obs_data_set_string(save_data, "TestMidiDock", jsonStr.toUtf8().constData());
		}
#endif

#if ENABLE_TEST_SUPER_DOCK
		// Test Super Dock state
		if (g_docks.test_super) {
			QJsonObject data = g_docks.test_super->save_state();
			QJsonDocument doc(data);
			QString jsonStr = doc.toJson(QJsonDocument::Compact);
			obs_data_set_string(save_data, "TestSuperDock", jsonStr.toUtf8().constData());
		}
#endif

		// ControlRegistry persistent variables
		{
			QJsonObject data = super::ControlRegistry::instance().save_variables();
			if (!data.isEmpty()) {
				QJsonDocument doc(data);
				QString jsonStr = doc.toJson(QJsonDocument::Compact);
				obs_data_set_string(save_data, "ControlVariables", jsonStr.toUtf8().constData());
			}
		}

#if ENABLE_TWEAKS_PANEL
		// Tweaks settings
		if (g_instances.tweaks_impl) {
			obs_data_set_int(save_data, "TweaksProgramOptions",
					 g_instances.tweaks_impl->GetProgramOptionsState());
			obs_data_set_int(save_data, "TweaksProgramLayout",
					 g_instances.tweaks_impl->GetProgramLayoutState());
			obs_data_set_int(save_data, "TweaksPreviewLayout",
					 g_instances.tweaks_impl->GetPreviewLayoutState());
		}
#endif

#if ENABLE_VOLUME_METER_DOCK
		// Volume Meter Demo style
		if (g_docks.volume_meter_demo)
			obs_data_set_int(save_data, "VolumeMeterDemoStyle",
					 g_docks.volume_meter_demo->getSelectedStyleIndex());
#endif
	} else {
		// Loading
#if ENABLE_DOCK_WINDOW_MANAGER
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
#endif

#if ENABLE_BROWSER_DOCKS
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
#endif

#if ENABLE_SOURCERER_DOCKS
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
#endif

		// MIDI Router bindings
		const char *midiJsonStr = obs_data_get_string(save_data, "MidiRouter");
		if (midiJsonStr && *midiJsonStr) {
			QJsonDocument doc = QJsonDocument::fromJson(QByteArray(midiJsonStr));
			if (!doc.isNull() && doc.isObject()) {
				MidiRouter::instance()->load(doc.object());
			}
		}

#if ENABLE_MIDI_DOCKS
		// Test MIDI Dock state
		const char *testMidiJsonStr = obs_data_get_string(save_data, "TestMidiDock");
		if (testMidiJsonStr && *testMidiJsonStr && g_docks.test_midi) {
			QJsonDocument doc = QJsonDocument::fromJson(QByteArray(testMidiJsonStr));
			if (!doc.isNull() && doc.isObject()) {
				g_docks.test_midi->load_state(doc.object());
			}
		}
#endif

#if ENABLE_TEST_SUPER_DOCK
		// Test Super Dock state
		const char *testSuperJsonStr = obs_data_get_string(save_data, "TestSuperDock");
		if (testSuperJsonStr && *testSuperJsonStr && g_docks.test_super) {
			QJsonDocument doc = QJsonDocument::fromJson(QByteArray(testSuperJsonStr));
			if (!doc.isNull() && doc.isObject()) {
				g_docks.test_super->load_state(doc.object());
			}
		}
#endif

		// ControlRegistry persistent variables
		const char *ctrlVarsJsonStr = obs_data_get_string(save_data, "ControlVariables");
		if (ctrlVarsJsonStr && *ctrlVarsJsonStr) {
			QJsonDocument doc = QJsonDocument::fromJson(QByteArray(ctrlVarsJsonStr));
			if (!doc.isNull() && doc.isObject()) {
				super::ControlRegistry::instance().load_variables(doc.object());
			}
		}

#if ENABLE_TWEAKS_PANEL
		// Tweaks settings
		if (g_instances.tweaks_impl) {
			g_instances.tweaks_impl->SetProgramOptionsState(
				obs_data_get_int(save_data, "TweaksProgramOptions"));
			g_instances.tweaks_impl->SetProgramLayoutState(
				obs_data_get_int(save_data, "TweaksProgramLayout"));
			g_instances.tweaks_impl->SetPreviewLayoutState(
				obs_data_get_int(save_data, "TweaksPreviewLayout"));
			g_instances.tweaks_impl->ApplyTweaks();
		}

		// Volume Meter Demo style
		volumeMeterDemoStyle = obs_data_get_int(save_data, "VolumeMeterDemoStyle");
#endif
	}
}

void load_browser_docks()
{
#if ENABLE_BROWSER_DOCKS
	if (!g_dialogs.browser_dock_manager) {
		// TODO: recreate of dead
	} else {
		g_dialogs.browser_dock_manager->onOBSBrowserReady();
	}
#endif
}

void unload_browser_docks()
{
#if ENABLE_BROWSER_DOCKS
	delete g_dialogs.browser_dock_manager;
	g_dialogs.browser_dock_manager = nullptr;
#endif
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
#if ENABLE_BROWSER_DOCKS
		if (g_dialogs.browser_dock_manager) {
			g_dialogs.browser_dock_manager->onOBSBrowserReady();
		}
#endif
#if ENABLE_SOURCERER_DOCKS
		if (g_docks.sourcerer_scenes) {
			g_docks.sourcerer_scenes->FrontendReady();
		}
#endif

#if ENABLE_TWEAKS_PANEL
		if (g_instances.tweaks_impl)
			g_instances.tweaks_impl->FrontendReady();
#endif

		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED: {
#if ENABLE_BROWSER_DOCKS
		BrowserManager::cleanup(false);
		load_browser_docks();
#endif
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
		blog(LOG_INFO, "[super_suite] OBS_FRONTEND_EVENT_EXIT received — starting cleanup");

		obs_queue_task(
			OBS_TASK_GRAPHICS,
			[](void *) {
				obs_queue_task(
					OBS_TASK_UI,
					[](void *) {
						unload_browser_docks();
#if ENABLE_BROWSER_DOCKS
						BrowserManager::cleanup();
#endif
					},
					nullptr, false);
			},
			nullptr, false);

		// Clear mixer dock channels BEFORE audio_sources_cleanup so that
		// volmeters and signal handlers are detached while sources still exist.
#if ENABLE_SUPER_MIXER_DOCK
		if (g_docks.super_mixer)
			g_docks.super_mixer->clearChannels();
#endif
#if ENABLE_S_MIXER_DOCK
		blog(LOG_INFO, "[super_suite] calling s_mixer_demo->prepareForShutdown()...");
		if (g_docks.s_mixer_demo)
			g_docks.s_mixer_demo->prepareForShutdown();
		blog(LOG_INFO, "[super_suite] s_mixer_demo->prepareForShutdown() done");
#endif
#if ENABLE_DAW_MIXER_DOCK
		if (g_docks.daw_mixer_demo)
			g_docks.daw_mixer_demo->clearChannels();
#endif
#if ENABLE_VOLUME_METER_DOCK
		if (g_docks.volume_meter_demo)
			g_docks.volume_meter_demo->clearMeters();
#endif

		blog(LOG_INFO, "[super_suite] calling audio_sources_cleanup()...");
		audio_sources_cleanup();
		blog(LOG_INFO, "[super_suite] EXIT cleanup COMPLETE");
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

#if ENABLE_CHANNELS_VIEWER
	if (!g_dialogs.canvas_channels) {
		auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		g_dialogs.canvas_channels = new ChannelsDialog(mainWindow);
	}
	g_dialogs.canvas_channels->show();
	g_dialogs.canvas_channels->raise();
	g_dialogs.canvas_channels->activateWindow();
#endif
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

#if ENABLE_OUTPUTS_VIEWER
	if (!g_dialogs.outputs_viewer) {
		auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		g_dialogs.outputs_viewer = new OutputsViewer(mainWindow);
	}
	g_dialogs.outputs_viewer->show();
	g_dialogs.outputs_viewer->raise();
	g_dialogs.outputs_viewer->activateWindow();
#endif
}

static void show_encoders_viewer(void *data)
{
	UNUSED_PARAMETER(data);

#if ENABLE_ENCODERS_VIEWER
	if (!g_dialogs.encoders_viewer) {
		auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		g_dialogs.encoders_viewer = new EncodersViewer(mainWindow);
	}
	g_dialogs.encoders_viewer->show();
	g_dialogs.encoders_viewer->raise();
	g_dialogs.encoders_viewer->activateWindow();
#endif
}

static void show_dock_window_manager(void *data)
{
	UNUSED_PARAMETER(data);

#if ENABLE_DOCK_WINDOW_MANAGER
	if (!g_dialogs.dock_window_manager) {
		auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		g_dialogs.dock_window_manager = new DockWindowManager(mainWindow);
	}
	g_dialogs.dock_window_manager->show();
	g_dialogs.dock_window_manager->raise();
	g_dialogs.dock_window_manager->activateWindow();
#endif
}

static void show_browser_manager(void *data)
{
	UNUSED_PARAMETER(data);

#if ENABLE_BROWSER_DOCKS
	if (!g_dialogs.browser_dock_manager) {
		auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		g_dialogs.browser_dock_manager = new BrowserManager(mainWindow);
	}
	g_dialogs.browser_dock_manager->show();
	g_dialogs.browser_dock_manager->raise();
	g_dialogs.browser_dock_manager->activateWindow();
#endif
}

static void show_encoding_graph(void *data)
{
	UNUSED_PARAMETER(data);

#if ENABLE_ENCODING_GRAPH
	if (!g_dialogs.encoding_graph) {
		auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		// encoding_graph = new EncodingGraphWindow(mainWindow);
		g_dialogs.encoding_graph = new EncodingGraphWindow(nullptr);
	}
	g_dialogs.encoding_graph->show();
	g_dialogs.encoding_graph->raise();
	g_dialogs.encoding_graph->activateWindow();
#endif
}

static void show_graph_editor(void *)
{
#if ENABLE_GRAPH_EDITORS
	if (!g_dialogs.graph_editor)
		g_dialogs.graph_editor = new GraphEditorWindow(nullptr);
	g_dialogs.graph_editor->show();
	g_dialogs.graph_editor->raise();
	g_dialogs.graph_editor->activateWindow();
#endif
}

static void show_surface_editor(void *)
{
#if ENABLE_GRAPH_EDITORS
	if (!g_dialogs.surface_editor)
		g_dialogs.surface_editor = new SurfaceEditorWindow(nullptr);
	g_dialogs.surface_editor->show();
	g_dialogs.surface_editor->raise();
	g_dialogs.surface_editor->activateWindow();
#endif
}

static void show_tweaks_panel(void *)
{
#if ENABLE_TWEAKS_PANEL
	if (!g_dialogs.tweaks_panel)
		g_dialogs.tweaks_panel = new TweaksPanel(g_instances.tweaks_impl, nullptr);
	g_dialogs.tweaks_panel->show();
	g_dialogs.tweaks_panel->raise();
	g_dialogs.tweaks_panel->activateWindow();
#endif
}

static void show_qt_inspector(void *)
{
	if (!g_dialogs.qt_inspector)
		g_dialogs.qt_inspector = new QtInspector(nullptr);
	g_dialogs.qt_inspector->show();
	g_dialogs.qt_inspector->raise();
	g_dialogs.qt_inspector->activateWindow();
}

#ifdef __cplusplus
extern "C" {
#endif

bool on_plugin_load()
{
	LibOBSTweaker::OnLoad();
	OBSFrontendTweaker::OnLoad();
	OBSFrontendHelper::OnLoad();

	// Check if we have all the deps loaded
	return true;
}

void on_plugin_loaded()
{
	LibOBSTweaker::OnLoaded();
	OBSFrontendTweaker::OnLoaded();
	OBSFrontendHelper::OnLoaded();

	obs_frontend_add_event_callback(on_obs_evt, nullptr);

	// Add Tools menu item
	obs_frontend_add_tools_menu_item(obs_module_text("AsioChannels"), show_settings_dialog, nullptr);

#if ENABLE_CHANNELS_VIEWER
	// Add Channels View menu item
	obs_frontend_add_tools_menu_item(obs_module_text("ChannelsView.Title"), // "Channels" or similar
					 show_channels_view, nullptr);
#endif

	// Add Canvas Manager menu item
	obs_frontend_add_tools_menu_item(obs_module_text("CanvasManager.Title"), show_canvas_manager, nullptr);

#if ENABLE_OUTPUTS_VIEWER
	// Add Outputs Viewer menu item
	obs_frontend_add_tools_menu_item(obs_module_text("OutputsViewer.Title"), show_outputs_viewer, nullptr);
#endif

#if ENABLE_ENCODERS_VIEWER
	// Add Encoders Viewer menu item
	obs_frontend_add_tools_menu_item(obs_module_text("EncodersViewer.Title"), show_encoders_viewer, nullptr);
#endif

#if ENABLE_DOCK_WINDOW_MANAGER
	// Add Dock Window Manager menu item
	obs_frontend_add_tools_menu_item(obs_module_text("DockWindowManager.Title"), show_dock_window_manager, nullptr);
#endif

#if ENABLE_ENCODING_GRAPH
	// Add Encoding Graph menu item
	obs_frontend_add_tools_menu_item(obs_module_text("EncodingGraph.Title"), show_encoding_graph, nullptr);
#endif

#if ENABLE_BROWSER_DOCKS
	// Add Browser Manager menu item
	obs_frontend_add_tools_menu_item(obs_module_text("BrowserManager.Title"), show_browser_manager, nullptr);
#endif

#if ENABLE_GRAPH_EDITORS
	// Graph Editor & Surface Editor
	obs_frontend_add_tools_menu_item("Graph Editor", show_graph_editor, nullptr);
	obs_frontend_add_tools_menu_item("Surface Editor", show_surface_editor, nullptr);
#endif

#if ENABLE_TWEAKS_PANEL
	// Add Tweaks Panel menu item
	auto actionTweaks = (QAction *)obs_frontend_add_tools_menu_qaction("Super Suite Tweaks");
	QObject::connect(actionTweaks, &QAction::triggered, []() {
		static TweaksPanel *tweaks = nullptr;
		if (!tweaks) tweaks = new TweaksPanel(g_instances.tweaks_impl, static_cast<QWidget *>(obs_frontend_get_main_window()));
		tweaks->show();
		tweaks->raise();
	});
#endif

#if ENABLE_AUDIO_MATRIX
	// Add Audio Matrix Router menu item
	auto actionMatrix = (QAction *)obs_frontend_add_tools_menu_qaction("Audio Matrix Router");
	QObject::connect(actionMatrix, &QAction::triggered, []() {
		static AudioMatrix *matrix = nullptr;
		if (!matrix) matrix = new AudioMatrix(static_cast<QWidget *>(obs_frontend_get_main_window()));
		matrix->show();
		matrix->raise();
	});
#endif

	// Add Qt Inspector menu item
	auto actionInspector = (QAction *)obs_frontend_add_tools_menu_qaction("Qt Inspector");
	QObject::connect(actionInspector, &QAction::triggered, []() {
		static QtInspector *inspector = nullptr;
		if (!inspector) inspector = new QtInspector(static_cast<QWidget *>(obs_frontend_get_main_window()));
		inspector->show();
		inspector->raise();
	});

	obs_frontend_add_save_callback(save_callback, nullptr);

	// Try to load initial state
	// Note: obs_frontend_add_save_callback is called for save, but for load we need to
	// explicitly ask for data or wait for a specific event?
	// standard practice: use a separate config file or use the passed save_data?
	// Actually, the save_callback is called with saving=false when loading?
	// Documentation says: "Functions assigned with this are called when saving/loading... "

	// Create and register docks
	auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());

#if ENABLE_SUPER_MIXER_DOCK
	g_docks.super_mixer = new MixerDock(mainWindow);
	obs_frontend_add_dock_by_id("SuperMixerDock", obs_module_text("SuperMixer.Title"), g_docks.super_mixer);
#endif

#if ENABLE_TEST_SUPER_DOCK
	g_docks.wrapper_test = new WrapperTestDock(mainWindow);
	obs_frontend_add_dock_by_id("WrapperTestDock", "OBS Wrapper Test", g_docks.wrapper_test);
	
	g_docks.test_super = new TestSuperDock(mainWindow);
	obs_frontend_add_dock_by_id("TestSuperDock", "Test Super Dock", g_docks.test_super);
#endif

#if ENABLE_SOURCERER_DOCKS
	g_docks.sourcerer_sources = new SourcererSourcesDock(mainWindow);
	g_docks.sourcerer_scenes = new SourcererScenesDock(mainWindow);
	obs_frontend_add_dock_by_id("SourcererSources", "Sourcerer Sources", g_docks.sourcerer_sources);
	obs_frontend_add_dock_by_id("SourcererScenes", "Sourcerer Scenes", g_docks.sourcerer_scenes);
#endif

#if ENABLE_MIDI_DOCKS
	g_docks.test_midi = new TestMidiDock(mainWindow);
	obs_frontend_add_dock_by_id("TestMidiDock", "Test MIDI Dock", g_docks.test_midi);
#endif

#if ENABLE_VOLUME_METER_DOCK
	g_docks.volume_meter_demo = new VolumeMeterDemoDock(mainWindow);
	obs_frontend_add_dock_by_id("VolumeMeterDemoDock", "Volume Meter Demo", g_docks.volume_meter_demo);
#endif

#if ENABLE_DAW_MIXER_DOCK
	g_docks.daw_mixer_demo = new DawMixerDemoDock(mainWindow);
	obs_frontend_add_dock_by_id("DawMixerDemoDock", "DAW Mixer Demo", g_docks.daw_mixer_demo);
#endif

#if ENABLE_S_MIXER_DOCK
	g_docks.s_mixer_demo = new SMixerDemoDock(mainWindow);
	obs_frontend_add_dock_by_id("SMixerDemoDock", "Super Mixer Demo", g_docks.s_mixer_demo);
#endif

#if ENABLE_VOLUME_METER_DOCK
	// Restore style
	if (volumeMeterDemoStyle >= 0 && volumeMeterDemoStyle < 4)
		g_docks.volume_meter_demo->setSelectedStyleIndex(volumeMeterDemoStyle);
#endif

	// Create Global Instances
#if ENABLE_TWEAKS_PANEL
	g_instances.tweaks_impl = new TweaksImpl();
#endif
}

void on_plugin_unload()
{
	LibOBSTweaker::OnUnload();
	OBSFrontendTweaker::OnUnload();
	OBSFrontendHelper::OnUnload();

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

#if ENABLE_CHANNELS_VIEWER
		if (g_dialogs.canvas_channels) {
			delete g_dialogs.canvas_channels;
		}
#endif

#if ENABLE_BROWSER_DOCKS
		if (g_dialogs.browser_dock_manager) {
			delete g_dialogs.browser_dock_manager;
		}
#endif

#if ENABLE_SUPER_MIXER_DOCK
		if (g_docks.super_mixer) {
			// Dock is managed by OBS frontend, just remove our reference
			obs_frontend_remove_dock("SuperMixerDock");
			delete g_docks.super_mixer;
		}
#endif

#if ENABLE_TEST_SUPER_DOCK
		if (g_docks.wrapper_test) {
			obs_frontend_remove_dock("WrapperTestDock");
			delete g_docks.wrapper_test;
		}
#endif

#if ENABLE_SOURCERER_DOCKS
		if (g_docks.sourcerer_sources) {
			obs_frontend_remove_dock("SourcererSources");
			delete g_docks.sourcerer_sources;
		}

		if (g_docks.sourcerer_scenes) {
			obs_frontend_remove_dock("SourcererScenes");
			delete g_docks.sourcerer_scenes;
		}
#endif

#if ENABLE_MIDI_DOCKS
		if (g_docks.test_midi) {
			obs_frontend_remove_dock("TestMidiDock");
			delete g_docks.test_midi;
		}
#endif

#if ENABLE_TEST_SUPER_DOCK
		if (g_docks.test_super) {
			obs_frontend_remove_dock("TestSuperDock");
			delete g_docks.test_super;
		}
#endif

#if ENABLE_VOLUME_METER_DOCK
		if (g_docks.volume_meter_demo) {
			obs_frontend_remove_dock("VolumeMeterDemoDock");
			delete g_docks.volume_meter_demo;
		}
#endif

#if ENABLE_DAW_MIXER_DOCK
		if (g_docks.daw_mixer_demo) {
			obs_frontend_remove_dock("DawMixerDemoDock");
			delete g_docks.daw_mixer_demo;
		}
#endif

#if ENABLE_S_MIXER_DOCK
		if (g_docks.s_mixer_demo) {
			obs_frontend_remove_dock("SMixerDemoDock");
			delete g_docks.s_mixer_demo;
		}
#endif
	}

	MidiRouter::cleanup();
	AudioChSrcConfig::cleanup();

#if ENABLE_TWEAKS_PANEL
	delete g_instances.tweaks_impl;
#endif

#if ENABLE_BROWSER_DOCKS
	BrowserManager::cleanup();
#endif
}

#ifdef __cplusplus
}
#endif
