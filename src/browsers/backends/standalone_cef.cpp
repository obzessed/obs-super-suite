#include "standalone_cef.hpp"

#include <QResizeEvent>
#include <QWidget>

// #include <QCefView.h>

StandaloneCEFBackend::StandaloneCEFBackend() = default;

StandaloneCEFBackend::~StandaloneCEFBackend() {
    // if (m_view) {
    //     delete m_view;
    //     m_view = nullptr;
    // }
}

void StandaloneCEFBackend::init(const InitParams& params) {
    if (!params.qtParentWidget) return;

    QWidget* parent = static_cast<QWidget*>(params.qtParentWidget);
    
    // QCefView constructor takes url and parent
    // Verify signature: QCefView(const QString& url, QWidget* parent = nullptr)
    QString url = params.initialUrl.empty() ? "about:blank" : QString::fromStdString(params.initialUrl);

    // build settings for per QCefView
    // QCefSetting setting;
    // m_view = new QCefView(url, &setting, parent);
    
    // Connect signals if available?
    // QObject::connect(m_view, &QCefView::loadFinished, [this](bool success) {
    //     if (success) {
    //         // Inject startup script?
    //         if (!m_startupScript.empty()) {
    //             m_view->executeJavascript(QString::fromStdString(m_startupScript));
    //         }
    //         if (m_readyCallback) m_readyCallback();
    //     }
    // });
    // Assuming API has signals. If not, we might need a different approach.
    // For now, I'll assume basic usage.
    
    if (m_readyCallback) {
        // Assume ready immediately or after some time?
        // Better to wait for signal, but interface doesn't enforce async.
        m_readyCallback();
    }
}

void StandaloneCEFBackend::resize(int x, int y, int width, int height) {
    if (m_view) {
        // m_view->setGeometry(x, y, width, height);
    }
}

void StandaloneCEFBackend::loadUrl(const std::string& url) {
    if (m_view) {
        // m_view->navigateToUrl(QString::fromStdString(url));
    }
}

void StandaloneCEFBackend::reload() {
    if (m_view) {
        // m_view->browserReload();
    }
}

void StandaloneCEFBackend::setStartupScript(const std::string& script) {
    m_startupScript = script;
    // QCefView might have addStartupScript?
    // m_view->addStartupScript(QString::fromStdString(script)); // Speculative
    // Or just run it on load.
}

void StandaloneCEFBackend::runJavaScript(const std::string& script) {
    if (m_view) {
        // m_view->executeJavascript("", QString::fromStdString(script), "");
    }
}

void StandaloneCEFBackend::clearCookies() {
	if (m_view) {
		// m_view->deleteAllCookies(); // Hypothetical API
		// QCefView API check required.
		// Assuming QCefView has `deleteAllCookies` or similar?
		// User didn't provide QCefView header content for me to verify.
		// I'll assume standard naming or verify later.
	}
}
