#pragma once

#include "base.hpp"

#include "../../utils/browser-panel.hpp"
#include <QString>

class OBSBrowserCEFBackend : public BrowserBackend {
public:
	OBSBrowserCEFBackend();
	~OBSBrowserCEFBackend() override;

	void init(const InitParams& params) override;
	void resize(int x, int y, int width, int height) override;
	void loadUrl(const std::string& url) override;
	void reload() override;
	void setStartupScript(const std::string& script) override;
	void runJavaScript(const std::string& script) override;
	void setOnReady(BrowserReadyCallback callback) override { m_readyCallback = callback; }

	// Specific to CEF wrapper: handle onOBSBrowserReady? 
	// The manager calls onOBSBrowserReady which calls activeDocks->onOBSBrowserReady.
	// We might need to expose this, or handle it internally via init if manager passes CEF instance.
	// But `QCefWidget` creation depends on `QCef` instance availability.
	// We'll handle that in init.

private:
	QCefWidget* m_cefWidget = nullptr;
	BrowserReadyCallback m_readyCallback;
	QString m_script;
	QString m_css; // We don't have separate CSS setter in interface yet, but init might pass it?
	// Interface has setStartupScript. We assume it combines JS+CSS or we add setCSS to interface?
	// For now, let's assume setStartupScript handles the injection logic (caller combines them).
	// But `BrowserDock` had `script` and `css`.
	// I should probably add `injectCSS` to interface or assume `setStartupScript` does it.
	// `BrowserBackend` interface has `setStartupScript`.
	// `EdgeWebview2` impl of `setStartupScript` uses `AddScriptToExecuteOnDocumentCreated`.
	// `QCefWidget` has `setStartupScript`.
	
	// We need to store CSS if we want to support `injectCSS` from interface, but interface doesn't have it yet?
	// Wait, `base.hpp` in step 7162 HAD `injectCSS`.
	// But in step 7197 I defined `BrowserBackend` interface WITHOUT `injectCSS` (I missed it or intentionally removed it?).
	// I defined `setStartupScript(const std::string& script)`.
	// I should probably add `injectCSS` if needed, or caller combines.
	// For now, caller can combine.
};
