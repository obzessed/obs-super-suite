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
	void setOnReady(BrowserReadyCallback callback) override { m_readyCallback = callback; }

private:
	QCefView* m_view = nullptr;
	BrowserReadyCallback m_readyCallback;
	std::string m_startupScript;
};
