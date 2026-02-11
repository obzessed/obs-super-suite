#pragma once

#include <obs-frontend-api.h>
#include "plugin-support.h"
#include "browser-panel.hpp"

class QCefHelper {
protected:
	static int version_;
	static QCef *instance_;
	static QCefCookieManager *cookie_manager_;

	static void init_() {
		if (!instance_) {
			instance_ = obs_browser_init_panel();
			version_ = obs_browser_qcef_version();

			if (!instance_) {
				obs_log(LOG_ERROR, "error creating cef instance.");
			} else {
				if (!instance_->initialized()) {
					obs_log(LOG_WARNING, "cef is not yet initialized!, waiting for it.");

					if (!instance_->init_browser() && !instance_->wait_for_browser_init()) {
						obs_log(LOG_ERROR, "error initializing browser init.");
					}
				}
			}
		}

		// Note: must be after: OBS_FRONTEND_EVENT_FINISHED_LOADING
		if (!cookie_manager_ && instance_) {
			// created by obs-browser on obs_browser_create_qcef ?
			if (const char *cookie_id = config_get_string(obs_frontend_get_profile_config(), "Panels", "CookieId");
			    cookie_id && cookie_id[0] != '\0') {
				std::string sub_path;
				sub_path += "super-dock-cookies/";
				sub_path += cookie_id;
				cookie_manager_ = instance_->create_cookie_manager(sub_path);
				if (!cookie_manager_) {
					obs_log(LOG_ERROR, "error loading cookie manager.");
				}
			    }  else {
			    	obs_log(LOG_INFO, "ignoring loading of cookie manager.");
			    }
		}

		obs_log(LOG_INFO, "CEF INITIALIZED: %p, %p", instance_, cookie_manager_);
	}
public:
	QCefHelper(QCefHelper &other) = delete;
	void operator=(const QCefHelper &) = delete;

	static int getVersion() { return version_; }

	static std::pair<QCef*, QCefCookieManager*> getInstance() {
		init_();

		if (!instance_) {
			obs_log(LOG_ERROR, "cef: usage before init");
		}
		if (!cookie_manager_) {
			obs_log(LOG_ERROR, "cef cookie manager: usage before init");
		}

		return {instance_, cookie_manager_};
	}

	static void cleanup(const bool full) {
		if (cookie_manager_) {
			cookie_manager_->FlushStore();
			delete cookie_manager_;
			cookie_manager_ = nullptr;
		}

		if (full && instance_) {
			delete instance_;
			instance_ = nullptr;
		}
	}
};