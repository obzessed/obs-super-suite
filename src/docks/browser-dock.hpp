#pragma once

#include "../dialogs/browser_manager.h"

#include <QWidget>
#include <QVBoxLayout>

class BrowserDock : public QWidget {
	Q_OBJECT

private:
	QCefWidget *cefWidget = nullptr;
	QVBoxLayout *layout = nullptr;

	QString script_;
	QString css_;
	QString url_;

	BrowserManager& manager_;

	bool deferred_;
public:
	BrowserDock(BrowserManager& manager, const char *url, const char *script = nullptr, const char *css = nullptr, bool deferred_load = false, QWidget *parent = nullptr);
	~BrowserDock() override;
	
	void reload(const char *url = nullptr, const char *script = nullptr, const char *css = nullptr);
	void onOBSBrowserReady();

private:
	bool createBrowser();
};

