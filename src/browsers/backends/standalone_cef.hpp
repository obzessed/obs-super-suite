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
	void setOnNavigationStart(NavigationStartingCallback callback) override { m_navigationStartingCallback = callback; }
	void setOnMutedStateChange(MutedStateChangeCallback callback) override { m_mutedStateChangeCallback = callback; }
	void setOnAudioPlayingChanged(AudioPlayingChangedCallback callback) override { m_audioPlayingChangedCallback = callback; }
	uint32_t getCapabilities() override { return (uint32_t)(BrowserCapabilities::JavaScript | BrowserCapabilities::Transparency | BrowserCapabilities::OSR); }
	void setAudioMuted(bool muted) override;
	[[nodiscard]] bool isAudioMuted() const override;
	[[nodiscard]] bool isPlayingAudio() const override;
private:
	QCefView* m_view = nullptr;
	BrowserReadyCallback m_readyCallback;
	NavigationStartingCallback m_navigationStartingCallback;
	MutedStateChangeCallback m_mutedStateChangeCallback;
	AudioPlayingChangedCallback m_audioPlayingChangedCallback;
	std::string m_startupScript;
};
