#include "obs_browser_cef.hpp"
#include "../../utils/qcef_helper.hpp"

#include <stdexcept>
#include <QVBoxLayout>

OBSBrowserCEFBackend::OBSBrowserCEFBackend() = default;

OBSBrowserCEFBackend::~OBSBrowserCEFBackend() {
	if (m_cefWidget) {
		m_cefWidget->setParent(nullptr);
		m_cefWidget->deleteLater();
	}
}

void OBSBrowserCEFBackend::init(const InitParams& params) {
	if (!params.qtParentWidget) {
		throw std::runtime_error("OBSBrowserCEFBackend requires a Qt parent widget");
	}

	auto* parent = static_cast<QWidget*>(params.qtParentWidget);

	const auto [cef, panel_cookies] = get_cef_instance();
	if (cef && panel_cookies) {
		std::string url = params.initialUrl.empty() ? "about:blank" : params.initialUrl;
		m_cefWidget = cef->create_widget(parent, url, panel_cookies);

		if (m_cefWidget) {
			// Ensure it fills the parent if no layout provided?
			// Parent QWebViewX should handle layout?
			// But QWebViewX implementation (which I haven't written yet) will probably use a layout.
			// If QWebViewX manages layout, we just return.
			// But we need to add it to QWebViewX's layout?
			// QWebViewX will likely add `backend->getWidget()`?
			// But `BrowserBackend` interface does NOT return a widget.
			// It returns void init.
			// So `QWebViewX` passes itself as parent.
			// The child widget (m_cefWidget) is now a child of QWebViewX.
			// QWebViewX needs to resize it manually in resizeEvent, OR use a layout.
			// Since `BrowserBackend` interface has `resize(x,y,w,h)`, QWebViewX will call that.
			// So we implement resize here.

			m_cefWidget->setStartupScript(m_script.toStdString());

			if (m_readyCallback) {
				m_readyCallback();
			}
		}
	}
}

void OBSBrowserCEFBackend::resize(int x, int y, int width, int height) {
	if (m_cefWidget) {
		m_cefWidget->setGeometry(x, y, width, height);
	}
}

void OBSBrowserCEFBackend::loadUrl(const std::string& url) {
	if (m_cefWidget) {
		m_cefWidget->setURL(url);
	}
}

void OBSBrowserCEFBackend::reload() {
	if (m_cefWidget) {
		m_cefWidget->reloadPage();
	}
}

void OBSBrowserCEFBackend::setStartupScript(const std::string& script) {
	m_script = QString::fromStdString(script);
	if (m_cefWidget) {
		m_cefWidget->setStartupScript(script);
	}
}

void OBSBrowserCEFBackend::runJavaScript(const std::string& script) {
	if (m_cefWidget) {
		m_cefWidget->executeJavaScript(script);
	}
}

uint32_t OBSBrowserCEFBackend::getCapabilities() {
	return (uint32_t)(BrowserCapabilities::JavaScript | BrowserCapabilities::Transparency | BrowserCapabilities::OSR);
}

void OBSBrowserCEFBackend::clearCookies() {
	// Not supported by QCefWidget wrapper directly yet?
	// Or maybe accessible via getRequest?
	// For now, no-op or log?
	// User asked for it, we should try.
	// But `QCefWidget` is minimal.
	// We'll leave empty for now as implementation detail of the wrapper.
}
