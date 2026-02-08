#include "browser-dock.hpp"

#include "plugin-support.h"
#include <util/dstr.h>

#include <QVBoxLayout>
#include <obs-frontend-api.h>

static std::string get_injection_script(const QString &script, const QString &css)
{
	dstr s = {nullptr};

	if (!css.isEmpty()) {
		dstr_cat(&s,
			"var s = document.createElement('style');"
			"s.id = 'super-suite-custom-css';"
			"s.innerHTML = `");
		dstr_cat(&s, css.toUtf8().constData());
		dstr_cat(&s, "`;"
			"document.head.appendChild(s);");
	}

	if (!script.isEmpty()) {
		dstr_cat(&s, "\n");
		dstr_cat(&s, script.toUtf8().constData());
	}

	std::string result(s.array ? s.array : "");
	dstr_free(&s);
	return result;
}

bool BrowserDock::createBrowser()
{
	if (const auto [cef, panel_cookies] = this->manager_.getQCef(); cef && panel_cookies) {
		cefWidget = cef->create_widget(this, url_.toUtf8().constData(), panel_cookies);

		cefWidget->setStartupScript(get_injection_script(script_, css_));

		layout->addWidget(cefWidget);

		return true;
	}

	obs_log(LOG_ERROR, "error creating browser widget. qcef or it's cookie manager is not initialized yet.");

	return false;
}

BrowserDock::BrowserDock(BrowserManager& manager, const char *url, const char *script, const char *css, bool deferred_load, QWidget *parent) : QWidget(parent), manager_(manager)
{
	url_ = url;
	script_ = script;
	css_ = css;
	deferred_ = deferred_load;

	setMinimumSize(200, 100);

	layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);

	if (!deferred_) {
		createBrowser();
	}
}

BrowserDock::~BrowserDock()
{
	layout->removeWidget(cefWidget);
	cefWidget->setParent(nullptr);
	cefWidget->deleteLater();
}

void BrowserDock::reload(const char *url, const char *script, const char *css)
{
	if (!cefWidget) return;

	// Update internal state
	url_ = url;
	script_ = script;
	css_ = css;

	cefWidget->setURL(url_.toStdString());
	
	// Always update startup script with latest combination of script and CSS
	cefWidget->setStartupScript(get_injection_script(script_, css_));
	
	cefWidget->reloadPage();
}

void BrowserDock::onOBSBrowserReady()
{
	if (deferred_ && !cefWidget) {
		createBrowser();
		deferred_ = false;
	}
}
