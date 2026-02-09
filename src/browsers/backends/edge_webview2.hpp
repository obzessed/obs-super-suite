#pragma once

#include "base.hpp"

#include <QWidget>
#include <QMainWindow>
#include <QString>

#include <wrl.h>
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
	uint32_t getCapabilities() override;

private:
	wrl::ComPtr<ICoreWebView2> m_webview;
	wrl::ComPtr<ICoreWebView2Controller> m_controller;
	BrowserReadyCallback m_readyCallback;
	InitParams m_params;

	void initWebView(HWND hwnd);
	void resizeWebView();
};