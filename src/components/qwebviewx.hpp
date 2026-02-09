#pragma once

#include <QWidget>
#include <memory>
#include <string>

#include "../browsers/backends/base.hpp"

class QWebViewX : public QWidget {
	Q_OBJECT

public:
	explicit QWebViewX(BackendType backend_type, QWidget* parent = nullptr);
	~QWebViewX() override;

	void loadUrl(const QString& url);
	void setStartupScript(const QString& script);
	void runJavaScript(const QString& script);
	void reload();
	void setUserDataPath(const QString& path);
	void clearCookies();

signals:
	void browserReady();

protected:
	void resizeEvent(QResizeEvent* event) override;
	void showEvent(QShowEvent* event) override;
	void closeEvent(QCloseEvent* event) override;

private:
	std::unique_ptr<BrowserBackend> backend;
	BackendType backend_type_;
	bool initialized_ = false;

	// Internal helper to create backend
	void createBackend();

	QString m_pendingUrl;
	QString m_pendingScript;
	QString m_userDataPath;
};
