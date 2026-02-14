#pragma once

#include "base.hpp"

#include <QWidget>
#include <QMainWindow>
#include <QString>

#include <wrl.h>
#include <wil/com.h>
#include <WebView2.h>

namespace wrl = Microsoft::WRL;

class EdgeWebview2Backend : public BrowserBackend {
public:
	EdgeWebview2Backend();
	~EdgeWebview2Backend() override;

	void init(const InitParams& params) override;
	void resize(int x, int y, int width, int height) override;
	void loadUrl(const std::string& url) override;
	void reload() override;
	void setStartupScript(const std::string& script) override;
	void runJavaScript(const std::string& script) override;
	void clearCookies() override;
	void setOnReady(BrowserReadyCallback callback) override { m_readyCallback = callback; }
	void setOnNavigationStart(NavigationStartingCallback callback) override { m_navigationStartingCallback = callback; }
	void setOnMutedStateChange(MutedStateChangeCallback callback) override { m_mutedStateChangeCallback = callback; }
	void setOnAudioPlayingChanged(AudioPlayingChangedCallback callback) override { m_audioPlayingChangedCallback = callback; }
	uint32_t getCapabilities() override;
	void setAudioMuted(bool muted) override;
	[[nodiscard]] bool isAudioMuted() const override;
	[[nodiscard]] bool isPlayingAudio() const override;

private:
	wil::com_ptr<ICoreWebView2> m_webview;
	wil::com_ptr<ICoreWebView2Controller> m_controller;
	BrowserReadyCallback m_readyCallback;
	NavigationStartingCallback m_navigationStartingCallback;
	MutedStateChangeCallback m_mutedStateChangeCallback;
	AudioPlayingChangedCallback m_audioPlayingChangedCallback;
	InitParams m_params;

	void initWebView(HWND hwnd);
	void resizeWebView();
};