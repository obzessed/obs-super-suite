#pragma once

#include "base.hpp"
#include <QWidget>
#include <QMainWindow>

#include <wrl.h>
#include <WebView2.h>
#include <QString>

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
	void setOnReady(BrowserReadyCallback callback) override { m_readyCallback = callback; }

private:
	wrl::ComPtr<ICoreWebView2> m_webview;
	wrl::ComPtr<ICoreWebView2Controller> m_controller;
	BrowserReadyCallback m_readyCallback;
	InitParams m_params;

	void initWebView(HWND hwnd);
	void resizeWebView();
};

// #include <wil/com.h>
// #include <WebView2.h>
// 
// namespace wrl = Microsoft::WRL;

// https://github.com/Atliac/WebView2SampleCMake/blob/master/WebView2SampleCMake/main.cpp

class QWebView2Widget: public QWidget {
	Q_OBJECT

	wrl::ComPtr<ICoreWebView2> m_webview;
	wrl::ComPtr<ICoreWebView2Controller> m_controller;
public:
	QWebView2Widget(QWidget *parent) : QWidget(parent) {}

	void showEvent(QShowEvent* event) override
	{
		QWidget::showEvent(event);
		if (!m_webview) initWebView();
	}

	void resizeEvent(QResizeEvent *event) override
	{
		QWidget::resizeEvent(event);
		if (m_controller) resizeWebView();
	}

	void loadUrl(const QString &url) const
	{
		if (m_webview) {
			m_webview->Navigate(url.toStdWString().c_str());
		}
	}

	void initWebView()
	{
	    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        wrl::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT, ICoreWebView2Environment* env) -> HRESULT {
                env->CreateCoreWebView2Controller(
                    reinterpret_cast<HWND>(winId()),
                    wrl::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT, ICoreWebView2Controller* controller) -> HRESULT {
                            if (controller) {
                                m_controller = controller;
                                m_controller->get_CoreWebView2(&m_webview);
                            }

			    // The demo step is redundant since the values are the default settings
			    ICoreWebView2Settings *Settings;
			    m_webview->get_Settings(&Settings);
			    Settings->put_IsScriptEnabled(TRUE);
			    Settings->put_AreDefaultScriptDialogsEnabled(TRUE);
			    Settings->put_IsWebMessageEnabled(TRUE);

                            resizeWebView();

                            if (m_webview) {
                            	emit onBrowserReady();

                                // Listen for messages from JavaScript
                                m_webview->add_WebMessageReceived(
                                    wrl::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                        [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                            LPWSTR message = nullptr;
                                            args->TryGetWebMessageAsString(&message);
                                            if (message) {
                                                QString msg = QString::fromWCharArray(message);
                                                emit entityClicked(msg);
                                                CoTaskMemFree(message);
                                            }
                                            return S_OK;
                                        }).Get(), nullptr);
                            }
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
	}

	void resizeWebView() const
	{
		if (m_controller) {
			RECT bounds;
			GetClientRect(reinterpret_cast<HWND>(winId()), &bounds);
			m_controller->put_Bounds(bounds);
		}
	}

	void runJavaScript(const QString &script) const
	{
		if (m_webview) {
			std::wstring wscript = script.toStdWString();
			m_webview->ExecuteScript(wscript.c_str(), nullptr);
		}
	}

signals:
	void entityClicked(const QString &message);
	void onBrowserReady();
};