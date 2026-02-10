#pragma once

#include <obs-frontend-api.h>
#include "plugin-support.h"
#include "browser-panel.hpp"

static QCef *x_cef_ = nullptr;
static QCefCookieManager *x_cef_ck_mgr = nullptr;

typedef QCef* (create_qcef_ft)();

static void _init_cef()
{
	if (!x_cef_) {
		x_cef_ = obs_browser_init_panel();
		if (!x_cef_) {
			obs_log(LOG_ERROR, "error creating cef instance.");
		} else {
			if (!x_cef_->initialized()) {
				obs_log(LOG_ERROR, "cef is not yet initialized!, waiting for it.");

				x_cef_->init_browser();

				if (!x_cef_->wait_for_browser_init()) {
					obs_log(LOG_ERROR, "error initializing browser init.");
				}
			}
		}
	}

	// Note: must be after: OBS_FRONTEND_EVENT_FINISHED_LOADING
	if (!x_cef_ck_mgr && x_cef_) {
		// created by obs-browser on obs_browser_create_qcef ?
		if (const char *cookie_id = config_get_string(obs_frontend_get_profile_config(), "Panels", "CookieId");
		    cookie_id && cookie_id[0] != '\0') {
			std::string sub_path;
			sub_path += "super-dock-cookies/";
			sub_path += cookie_id;
			x_cef_ck_mgr = x_cef_->create_cookie_manager(sub_path);
			if (!x_cef_ck_mgr) {
				obs_log(LOG_ERROR, "error loading cookie manager.");
			}
		    }  else {
		    	obs_log(LOG_INFO, "ignoring loading of cookie manager.");
		    }
	}

	obs_log(LOG_INFO, "CEF INITIALIZED: %p, %p", x_cef_, x_cef_ck_mgr);
}

static std::pair<QCef*, QCefCookieManager*> get_cef_instance()
{
	_init_cef();

	if (!x_cef_) {
		obs_log(LOG_ERROR, "cef: usage before init");
	}
	if (!x_cef_ck_mgr) {
		obs_log(LOG_ERROR, "cef cookie manager: usage before init");
	}

	return {x_cef_, x_cef_ck_mgr};
}

static void cleanup_cef(bool full)
{
	if (x_cef_ck_mgr) {
		x_cef_ck_mgr->FlushStore();
		delete x_cef_ck_mgr;
		x_cef_ck_mgr = nullptr;
	}

	if (full && x_cef_) {
		delete x_cef_;
		x_cef_ = nullptr;
	}
}