#include "frontend_helper.hpp"

#include <obs-frontend-api.h>

// #define LOG_TAG "obs_frontend_helper"
#define L(...) blog(LOG_INFO, __VA_ARGS__)

static struct obs_frontend_helper_state {
	// You can store any state you need here. This struct is passed to the event callback.
} g_state = {};

void OBSFrontendHelper::on_obs_frontend_evt(const obs_frontend_event event, void *data)
{
	L("%s: %d", __FUNCSIG__, event);

	UNUSED_PARAMETER(data);

	auto state = static_cast<obs_frontend_helper_state *>(data);

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		OBSFrontendHelper::on_frontend_ready();
		break;
	default:
		break;
	}
}

void OBSFrontendHelper::OnLoad()
{
	L("%s", __FUNCSIG__);

	// Called when the plugin is loaded, before OBS finishes initializing
	obs_frontend_add_event_callback(on_obs_frontend_evt, &g_state);
}

void OBSFrontendHelper::OnLoaded()
{
	L("%s", __FUNCSIG__);

	// Called after OBS has finished initializing and is ready
}

void OBSFrontendHelper::on_frontend_ready()
{
	L("%s", __FUNCSIG__);

	// Called when OBS signals that the frontend is ready (after FINISHED_LOADING)
}

void OBSFrontendHelper::OnUnload()
{
	L("%s", __FUNCSIG__);

	// Called when the plugin is being unloaded, before OBS starts shutting down
}