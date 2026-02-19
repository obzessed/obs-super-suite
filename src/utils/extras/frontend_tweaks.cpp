#include "frontend_tweaks.hpp"

#include "QLayout"

#include <QWidget>
#include <QMainWindow>
#include <QPointer>
#include <QMenuBar>
#include <QStatusBar>
#include <QLayoutItem>
#include <QSlider>

#include <obs-frontend-api.h>

// #define LOG_TAG "obs_frontend_helper"
#define L(...) blog(LOG_INFO, __VA_ARGS__)
#define E(...) blog(LOG_ERROR, __VA_ARGS__)
#define W(...) blog(LOG_WARNING, __VA_ARGS__)
#define D(...) blog(LOG_DEBUG, __VA_ARGS__)

struct obs_xd_widget_state {
	QPointer<QWidget> target;
	QPointer<QDockWidget> dock;
	QPointer<QWidget> dummy_stub;
	QLayoutItem* layout_item;
	struct {
		QPointer<QWidget> parent_widget;
		QRect geometry;
		int index_in_parent;
	} original;
};

static struct obs_frontend_tweaker_state {
	// You can store any state you need here. This struct is passed to the event callback.
	struct {
		struct {
			struct {
				QPointer<QMainWindow> self;
				QPointer<QWidget> central_widget;
				QPointer<QMenuBar> menu_bar;
				QPointer<QStatusBar> status_bar;
				QPointer<QWidget> canvas_editor;
				QPointer<QWidget> canvas_editor_parent;
				QPointer<QBoxLayout> preview_layout;
				QPointer<QWidget> program_options;
				QPointer<QWidget> program_options_parent;
			} basic; // OBSBasic
		} windows;

		struct {
			struct {
				obs_xd_widget_state canvas_editor;
				obs_xd_widget_state program_options;
			} basic; // OBSBasic
		} dockables;
	} widgets;
} g_state = {};

void OBSFrontendTweaker::on_obs_frontend_evt(const obs_frontend_event event, void *data)
{
	L("%s: %d", __FUNCSIG__, event);

	UNUSED_PARAMETER(data);

	auto state = static_cast<obs_frontend_tweaker_state *>(data);

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		on_frontend_ready();
		break;
	default:
		break;
	}
}

void OBSFrontendTweaker::OnLoad()
{
	L("%s", __FUNCSIG__);

	// Called when the plugin is loaded, before OBS finishes initializing
	obs_frontend_add_event_callback(on_obs_frontend_evt, &g_state);
}

void OBSFrontendTweaker::OnLoaded()
{
	L("%s", __FUNCSIG__);

	// Called after OBS has finished initializing and is ready
}

void OBSFrontendTweaker::on_frontend_ready()
{
	L("%s", __FUNCSIG__);

	// cache the targets widgets in out store before we do any modifications, so we can refer to them later if needed
	if (const auto mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window())) {
		g_state.widgets.windows.basic.self = mainWindow;

		g_state.widgets.windows.basic.central_widget = mainWindow->centralWidget();
		g_state.widgets.windows.basic.menu_bar = mainWindow->menuBar();
		g_state.widgets.windows.basic.status_bar = mainWindow->statusBar();

		if (const auto canvasEditor = mainWindow->findChild<QWidget *>("canvasEditor")) {
			// Found the canvas editor widget
			// L("Found canvas editor: %p", canvasEditor);
			g_state.widgets.windows.basic.canvas_editor = canvasEditor;
			g_state.widgets.windows.basic.canvas_editor_parent = canvasEditor->parentWidget();

			if (const auto *previewLayout =
				canvasEditor->findChild<QBoxLayout *>("previewLayout")) {

				QWidget* programOptions = nullptr;

				for (int i = 0; i < previewLayout->count(); i++) {
					const auto child =
						qobject_cast<QWidget *>(previewLayout->itemAt(i)->widget());

					// find which one has tBar->setProperty("class", "slider-tbar"); where tBar is a QSlider
					for (const auto subChild : child->findChildren<QSlider *>()) {
						if (subChild->property("class").toString() == "slider-tbar") {
							// child -> programOptions, subChild - > tBar
							programOptions = child;
							break;
						}
					}

					if (programOptions) {
						break;
					}
				}

				if (programOptions) {
					g_state.widgets.windows.basic.program_options = programOptions;
					g_state.widgets.windows.basic.program_options_parent = programOptions->parentWidget();
				} else {
					W("Program options widget not found inside preview layout");
				}
			} else {
				W("Preview layout not found inside canvas editor");
			}
		} else {
			W("Canvas editor widget not found by name, defaulting to central widget");
		}

		L("Main window cached: %p", mainWindow);
	} else {
		E("Failed to get main window");
	}

	// Called when OBS signals that the frontend is ready (after FINISHED_LOADING)
}

void OBSFrontendTweaker::OnUnload()
{
	L("%s", __FUNCSIG__);

	// Called when the plugin is being unloaded, before OBS starts shutting down
}
bool OBSFrontendTweaker::CentralWidgetReset()
{
	auto xd = g_state.widgets.dockables.basic.canvas_editor;
	if (const auto dock = xd.dock; dock && xd.dummy_stub && xd.target) {
		dock->hide();
		dock->setWidget(nullptr);
		dock->deleteLater();

		xd.dock = nullptr;

		const auto child = g_state.widgets.windows.basic.canvas_editor;

		// reparent to main window central widget
		if (const auto parent = g_state.widgets.windows.basic.canvas_editor_parent) {
			child->setParent(parent);
			child->resize(xd.dummy_stub->size());

			auto item = parent->layout()->replaceWidget(xd.dummy_stub, child);
			xd.dummy_stub->deleteLater();

			E("Replaced dummy stub with canvas editor: %p", item);

			child->setGeometry(xd.original.geometry);
			child->show();
		} else {
			W("Original parent for canvas editor not cached, cannot reset central widget properly");
		}
	} else {
		W("Canvas editor not currently docked, no need to reset");
	}

	CentralWidgetSetVisible(true);

	return false;
}

QDockWidget* OBSFrontendTweaker::CentralWidgetMakeDockable()
{
	CentralWidgetSetVisible(false); // hide the container

	if (const auto w = g_state.widgets.windows.basic.canvas_editor) {
		QWidget* dummy_stub = new QWidget(); // Create a dummy widget to take the original place in the layout
		dummy_stub->setObjectName(QString(w->objectName()) + "_DockPlaceholder");
		dummy_stub->setFixedSize(w->size());
		dummy_stub->setGeometry(w->geometry());
		auto layout_item = w->parentWidget()->layout()->replaceWidget(w, dummy_stub);
		dummy_stub->setVisible(false); // Hide the dummy stub

		E("Replaced canvas editor with dummy stub: %p", layout_item);

		// Create Dock
		const auto mainWin = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		auto dock = new QDockWidget(w->objectName(), mainWin);
		dock->setObjectName(QString(w->objectName()).remove(" ") + "Dock");
		dock->setWidget(w);
		dock->setFloating(true);
		mainWin->addDockWidget(Qt::RightDockWidgetArea, dock);
		dock->show();

		g_state.widgets.dockables.basic.canvas_editor = {
			.target = w,
			.dock = dock,
			.dummy_stub = dummy_stub,
			.layout_item = layout_item,
			.original = {
				.parent_widget = w->parentWidget(),
				.geometry = w->geometry()
			}
		};
	} else {
		W("Canvas editor widget not cached, cannot make central widget dockable");
	}

	return g_state.widgets.dockables.basic.canvas_editor.dock;
}

bool OBSFrontendTweaker::CentralWidgetIsVisible()
{
	if (const auto w = g_state.widgets.windows.basic.central_widget) {
		return w->isVisible();
	}

	E("Central widget not cached, cannot determine visibility");

	return false;
}
bool OBSFrontendTweaker::CentralWidgetSetVisible(const bool visible)
{
	if (const auto w = g_state.widgets.windows.basic.central_widget) {
		const auto current = w->isVisible();
		w->setVisible(visible);
		return current; // Return previous state
	}

	E("Central widget not cached, cannot set visibility");

	return false;
}
bool OBSFrontendTweaker::ProgramOptionsReset()
{
	auto xd = g_state.widgets.dockables.basic.program_options;
	if (const auto dock = xd.dock; dock && xd.dummy_stub && xd.target) {
		dock->hide();
		dock->setWidget(nullptr);
		dock->deleteLater();

		xd.dock = nullptr;

		const auto child = g_state.widgets.windows.basic.program_options;

		// reparent to main window central widget
		if (const auto parent = g_state.widgets.windows.basic.program_options_parent) {
			child->setParent(parent);
			child->resize(xd.dummy_stub->size());

			auto item = parent->layout()->replaceWidget(xd.dummy_stub, child);
			xd.dummy_stub->deleteLater();

			E("Replaced dummy stub with program options: %p", item);

			child->setGeometry(xd.original.geometry);
			child->show();
		} else {
			W("Original parent for program options not cached, cannot reset program options properly");
		}
	} else {
		W("Program options not currently docked, no need to reset");
	}

	ProgramOptionsSetVisible(true);

	return false;
}

QDockWidget *OBSFrontendTweaker::ProgramOptionsMakeDockable()
{
	ProgramOptionsSetVisible(true); // we need it visible to get the geometry and size for the dummy stub

	if (const auto w = g_state.widgets.windows.basic.program_options) {
		QWidget* dummy_stub = new QWidget(); // Create a dummy widget to take the original place in the layout
		dummy_stub->setObjectName(QString(w->objectName()) + "_DockPlaceholder");
		dummy_stub->setFixedSize(w->size());
		dummy_stub->setGeometry(w->geometry());
		auto layout_item = w->parentWidget()->layout()->replaceWidget(w, dummy_stub);
		dummy_stub->setVisible(false); // Hide the dummy stub

		ProgramOptionsSetVisible(false);

		E("Program options with dummy stub: %p", layout_item);

		// Create Dock
		const auto mainWin = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		auto dock = new QDockWidget(w->objectName(), mainWin);
		dock->setObjectName(QString(w->objectName()).remove(" ") + "Dock");
		dock->setWidget(w);
		dock->setFloating(true);
		mainWin->addDockWidget(Qt::RightDockWidgetArea, dock);
		dock->show();

		g_state.widgets.dockables.basic.program_options = {
			.target = w,
			.dock = dock,
			.dummy_stub = dummy_stub,
			.layout_item = layout_item,
			.original = {
				.parent_widget = w->parentWidget(),
				.geometry = w->geometry()
			}
		};
	} else {
		W("Program options widget not cached, cannot make central widget dockable");
	}

	return g_state.widgets.dockables.basic.program_options.dock;
}

bool OBSFrontendTweaker::ProgramOptionsIsVisible()
{

	if (const auto w = g_state.widgets.windows.basic.program_options) {
		return w->isVisible();
	}

	E("Program options widget not cached, cannot determine visibility");

	return false;
}

bool OBSFrontendTweaker::ProgramOptionsSetVisible(bool visible)
{
	if (const auto w = g_state.widgets.windows.basic.program_options) {
		const auto current = w->isVisible();
		w->setVisible(visible);
		return current; // Return previous state
	}

	E("Program options widget not cached, cannot set visibility");

	return false;
}
