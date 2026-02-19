#pragma once

#include <QDockWidget>

#include <obs.h>
#include <obs-frontend-api.h>

class OBSFrontendTweaker {
	static void on_obs_frontend_evt(obs_frontend_event event, void *data);

	static void on_frontend_ready();
public:
	static void OnLoad();

	static void OnLoaded();

	static void OnUnload();
private:
	void dummy();
public:
	static bool CentralWidgetReset();

	static QDockWidget* CentralWidgetMakeDockable();

	static bool CentralWidgetIsVisible();

	static bool CentralWidgetSetVisible(bool visible);

	static bool ProgramOptionsReset();

	static QDockWidget* ProgramOptionsMakeDockable();

	static bool ProgramOptionsIsVisible();

	static bool ProgramOptionsSetVisible(bool visible);
};