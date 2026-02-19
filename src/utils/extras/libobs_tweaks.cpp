#include "libobs_tweaks.hpp"

#include <obs-frontend-api.h>

// #define LOG_TAG "obs_frontend_helper"
#define L(...) blog(LOG_INFO, __VA_ARGS__)

static struct libobs_tweaker_state {
	// You can store any state you need here. This struct is passed to the event callback.
} g_state = {};

void LibOBSTweaker::OnLoad()
{
	L("%s", __FUNCSIG__);
}

void LibOBSTweaker::OnLoaded()
{
	L("%s", __FUNCSIG__);
}

void LibOBSTweaker::OnUnload()
{
	L("%s", __FUNCSIG__);
}
