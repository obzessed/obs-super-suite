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
	} else {
		dstr_cat(&s,
			"var s = document.getElementById('super-suite-custom-css');"
			"if (s) s.remove();");
	}

	if (!script.isEmpty()) {
		dstr_cat(&s, "\n");
		dstr_cat(&s, script.toUtf8().constData());
	} else {
		dstr_cat(&s, "\n");
	}

	std::string result(s.array ? s.array : "");
	dstr_free(&s);
	return result;
}

bool BrowserDock::createBrowser()
{
	// QWebViewX handles backend creation.
	// We just instantiate QWebViewX.
	
	if (webView_) return true;

	webView_ = new QWebViewX(backend_, this);
	
	// Connect readiness if needed?
	// QWebViewX emits browserReady.
	
	// Set initial script
	webView_->setStartupScript(QString::fromStdString(get_injection_script(script_, css_)));
	
	// Load URL
	webView_->loadUrl(url_);
	
	layout->addWidget(webView_);
	
	return true;
}

BrowserDock::BrowserDock(BrowserManager& manager, const char *url, const char *script, const char *css, BackendType backend, bool deferred_load, QWidget *parent) : QWidget(parent), manager_(manager)
{
	url_ = url;
	script_ = script;
	css_ = css;
	backend_ = backend;
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
	if (webView_) {
		layout->removeWidget(webView_);
		webView_->setParent(nullptr);
		delete webView_;
		webView_ = nullptr;
	}
}

void BrowserDock::reload(const char *url, const char *script, const char *css)
{
	// Update internal state
	url_ = url;
	script_ = script;
	css_ = css;
	
	if (webView_) {
		webView_->setStartupScript(QString::fromStdString(get_injection_script(script_, css_)));

		webView_->loadUrl(url_);
	}
}

void BrowserDock::onOBSBrowserReady()
{
	if (deferred_ && !webView_) {
		createBrowser();
		deferred_ = false;
	}
}
