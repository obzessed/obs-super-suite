#pragma once

#include "base.hpp"

class QCefView;

class StandaloneCEFBackend : public BrowserBackend {
public:
	StandaloneCEFBackend();
	~StandaloneCEFBackend() override;

	void init(const InitParams& params) override;
	void resize(int x, int y, int width, int height) override;
	void loadUrl(const std::string& url) override;
	void reload() override;
	void setStartupScript(const std::string& script) override;
	void runJavaScript(const std::string& script) override;
	void clearCookies() override;
	void setOnReady(BrowserReadyCallback callback) override { m_readyCallback = callback; }
	uint32_t getCapabilities() override { return (uint32_t)(BrowserCapabilities::JavaScript | BrowserCapabilities::Transparency | BrowserCapabilities::OSR); }

private:
	QCefView* m_view = nullptr;
	BrowserReadyCallback m_readyCallback;
	std::string m_startupScript;
};
