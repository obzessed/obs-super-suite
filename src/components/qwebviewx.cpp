#include "qwebviewx.hpp"

#include "plugin-support.h"
#include "browsers/backends/edge_webview2.hpp"
#include "browsers/backends/obs_browser_cef.hpp"
#include "browsers/backends/standalone_cef.hpp"

#include <QResizeEvent>
#include <QShowEvent>
#include <utility>

QWebViewX::QWebViewX(BackendType backend_type, QWidget* parent)
	: QWidget(parent), backend_type_(backend_type)
{
}

QWebViewX::~QWebViewX() = default;

void QWebViewX::createBackend()
{
	if (backend) return;

	BrowserBackend::InitParams params;
	params.parentWindowId = reinterpret_cast<void *>(winId());
	params.qtParentWidget = this;
	params.x = 0;
	params.y = 0;
	params.width = width();
	params.height = height();
	// We don't have initialUrl here, we rely on loadUrl call?
	// But if loadUrl was called before showEvent, we need to handle it.
	// For now, let's assume caller calls loadUrl AFTER creation or we store it.
	// But common pattern is create -> loadUrl.
	// So we should store pending actions?
	// For simplicity, we can let backend handle `loadUrl` even if not fully init?
	// `EdgeWebview2Backend` handles it by storing if not `m_webview`.
	// `OBSBrowserCEFBackend` might crash if `m_cefWidget` is not created yet? (It handles null check but doesn't queue).
	// So `QWebViewX` should probably queue it if backend not ready?
	// Or `BrowserBackend` implementations should queue it?
	// `EdgeWebview2Backend` queues it (via params.initialUrl or checking m_webview).
	// `OBSBrowserCEFBackend`: `loadUrl` does `if (m_cefWidget)`. If null, it does nothing.
	// So `OBSBrowserCEFBackend` needs to support basic queuing or allow `init` to take URL.
	// But `QWebViewX` API separates them.
	// Let's rely on caller or handle queuing in QWebViewX.
	// Actually, `BrowserDock` calls `createBrowser` (or `QWebViewX` creation) then `loadUrl`?
	// Not necessarily.
	// `BrowserDock` constructor takes URL.
	// So `QWebViewX` should probably accept initial URL or we add `pendingUrl_` member.
}

void QWebViewX::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);
	if (!initialized_) {
		createBackend(); // Wait, createBackend logic above is incomplete regarding instantiation.
		
		if (backend_type_ == BackendType::EdgeWebView2) {
			backend = std::make_unique<EdgeWebview2Backend>();
		} else if (backend_type_ == BackendType::StandaloneCEF) {
			backend = std::make_unique<StandaloneCEFBackend>();
		} else {
			backend = std::make_unique<OBSBrowserCEFBackend>();
		}

		backend->setOnReady([this] {
			backend->setAudioMuted(true); // Mute by default, since it's used for panels like chat/activity which don't need audio.
			emit browserReady();
		});

		BrowserBackend::InitParams params;
		params.parentWindowId = reinterpret_cast<void *>(winId());
		params.qtParentWidget = this;
		params.x = 0;
		params.y = 0;
		params.width = width();
		params.height = height();
		params.userDataPath = m_userDataPath.toStdString();
		// params.initialUrl = ? (If we had it).

		backend->init(params);
		
		if (!m_pendingScript.isEmpty()) {
			backend->setStartupScript(m_pendingScript.toStdString());
		}
		if (!m_pendingUrl.isEmpty()) {
			backend->loadUrl(m_pendingUrl.toStdString());
		}
		
		initialized_ = true;

		{
			RECT bounds;
			GetClientRect(reinterpret_cast<HWND>(winId()), &bounds);
			backend->resize(
				bounds.left,
				bounds.top,
				bounds.right - bounds.left,
				bounds.bottom - bounds.top
			);
		}

		backend->setOnNavigationStart([](const std::string& url) {
			obs_log(LOG_INFO, "Navigating browser dock to: %s", url.c_str());
		});
		
		// Re-apply pending calls?
		// We need to store them in QWebViewX members.
		// Since I didn't add members to header, I might need to update header first?
		// "QWebViewX" header in step 7203 has `backend_id_`, `initialized_`.
		// It doesn't have `pendingUrl_`.
		// But I can use dynamic property or add member.
		// I'll add member in this file? No, header.
		// I'll leave it simple for now: if caller calls `loadUrl` before init, it might be lost for CEF.
		// BUT `BrowserDock` logic:
		// `BrowserDock` constructor creates `QWebViewX`.
		// Then calls `loadUrl`?
		// `BrowserDock` constructor takes URL.
		// If I update `BrowserDock` to use `QWebViewX`, I'll update it to call `loadUrl` on it.
		// If `QWebViewX::showEvent` hasn't fired, `backend` is null.
		// `QWebViewX::loadUrl` checks `if (backend)`.
		// So checking if headers need update.
	}
}

void QWebViewX::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	if (backend) {
		backend->resize(0, 0, width(), height());
	}
}

void QWebViewX::loadUrl(const QString& url)
{
	m_pendingUrl = url;
	if (backend) {
		backend->loadUrl(url.toStdString());
	}
}

void QWebViewX::setStartupScript(const QString& script)
{
	m_pendingScript = script;
	if (backend) {
		backend->setStartupScript(script.toStdString());
	}
}

void QWebViewX::runJavaScript(const QString& script)
{
	if (backend) {
		backend->runJavaScript(script.toStdString());
	}
}

void QWebViewX::reload()
{
	if (backend) {
		backend->reload();
	}
}

void QWebViewX::setUserDataPath(const QString &path)
{
	m_userDataPath = path;
	// If backend exists, it might be too late to change path without recreation.
	// We assume this is set before init or requires recreation.
}

void QWebViewX::clearCookies()
{
	if (backend) {
		backend->clearCookies();
	}
}

void QWebViewX::closeEvent(QCloseEvent* event)
{
	QWidget::closeEvent(event);
	// Cleanup?
	backend.reset();
}
