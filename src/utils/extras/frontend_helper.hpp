#pragma once

#include <obs.h>
#include <obs-frontend-api.h>

class OBSFrontendHelper {
	static void on_obs_frontend_evt(obs_frontend_event event, void *data);

	static void on_frontend_ready();
public:
	static void OnLoad();

	static void OnLoaded();

	static void OnUnload();
private:
};