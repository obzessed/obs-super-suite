#pragma once

#include "../dialogs/browser_manager.h"
#include "../components/qwebviewx.hpp"

#include <QWidget>
#include <QVBoxLayout>

class BrowserDock : public QWidget {
	Q_OBJECT

private:
	QWebViewX* webView_ = nullptr;
	QVBoxLayout *layout = nullptr;

	QString script_;
	QString css_;
	QString url_;

	BrowserManager& manager_;

	bool deferred_;
	BackendType backend_;
public:
	BrowserDock(BrowserManager& manager, const char *url, const char *script = nullptr, const char *css = nullptr, BackendType backend = BackendType::CEF, bool deferred_load = false, QWidget *parent = nullptr);
	~BrowserDock() override;
	
	void reload(const char *url = nullptr, const char *script = nullptr, const char *css = nullptr);
	void onOBSBrowserReady();

private:
	bool createBrowser();
};

