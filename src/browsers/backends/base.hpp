#pragma once

#include <string>
#include <functional>

#define BBKE_OBS_BROWSER_CEF "obs-browser-cef"
#define BBKE_EGDE_WEBVIEW2 "edge-webview2"
#define BBKE_STANDALONE_CEF "standalone-cef"

enum class BackendType {
	ObsBrowserCEF,
	StandaloneCEF,
	EdgeWebView2
};

struct BackendHelpers {
	static std::string ToString(BackendType type) {
		switch (type) {
			case BackendType::EdgeWebView2: return BBKE_EGDE_WEBVIEW2;
			case BackendType::StandaloneCEF: return BBKE_STANDALONE_CEF;
			case BackendType::ObsBrowserCEF: default: return BBKE_OBS_BROWSER_CEF;
		}
	}

	static BackendType FromString(const std::string& str) {
		if (str == BBKE_EGDE_WEBVIEW2) return BackendType::EdgeWebView2;
		if (str == BBKE_STANDALONE_CEF) return BackendType::StandaloneCEF;
		return BackendType::ObsBrowserCEF;
	}
};

class BrowserBackend {
public:
	virtual ~BrowserBackend() = default;

	struct InitParams {
		void* parentWindowId; // HWND or WId
		void* qtParentWidget; // QWidget* (optional, for Qt-based backends)
		int x;
		int y;
		int width;
		int height;
		std::string initialUrl;
	};

	using BrowserReadyCallback = std::function<void()>;

	// Initialize the backend (create window/control)
	virtual void init(const InitParams& params) = 0;

	// Resize the backend control
	virtual void resize(int x, int y, int width, int height) = 0;

	// Navigation
	virtual void loadUrl(const std::string& url) = 0;
	virtual void reload() = 0;

	// Scripting
	virtual void setStartupScript(const std::string& script) = 0; // Injects JS/CSS combination
	virtual void runJavaScript(const std::string& script) = 0;

	// Callbacks
	virtual void setOnReady(BrowserReadyCallback callback) = 0;
};