#include "edge_webview2.hpp"

// #include <wil/com.h>
// #include <WebView2.h>

// https://github.com/Atliac/WebView2SampleCMake/blob/master/WebView2SampleCMake/main.cpp

#include "util/platform.h"

#include <stdexcept>

EdgeWebview2Backend::EdgeWebview2Backend() = default;

EdgeWebview2Backend::~EdgeWebview2Backend() {
    // Controller and WebView are smartly released by ComPtr
}

void EdgeWebview2Backend::init(const InitParams& params) {
    m_params = params;
    initWebView(static_cast<HWND>(params.parentWindowId));
}

void EdgeWebview2Backend::resize(int x, int y, int width, int height) {
    if (m_controller) {
        RECT bounds;
        bounds.left = x;
        bounds.top = y;
        bounds.right = x + width;
        bounds.bottom = y + height;
        m_controller->put_Bounds(bounds);
    }
}

void EdgeWebview2Backend::loadUrl(const std::string& url) {
    if (m_webview) {
        std::wstring wurl(url.begin(), url.end());
        m_webview->Navigate(wurl.c_str());
    } else {
        // Queue it? Or update params so it loads on ready
        m_params.initialUrl = url;
    }
}

void EdgeWebview2Backend::reload() {
    if (m_webview) {
        m_webview->Reload();
    }
}

void EdgeWebview2Backend::setStartupScript(const std::string& script) {
    if (m_webview) {
        std::wstring wscript(script.begin(), script.end());
        m_webview->AddScriptToExecuteOnDocumentCreated(wscript.c_str(), nullptr);
    }
}

void EdgeWebview2Backend::runJavaScript(const std::string& script) {
    if (m_webview) {
        std::wstring wscript(script.begin(), script.end());
        m_webview->ExecuteScript(wscript.c_str(), nullptr);
    }
}
uint32_t EdgeWebview2Backend::getCapabilities()
{
	return (uint32_t)(BrowserCapabilities::JavaScript | BrowserCapabilities::Transparency | BrowserCapabilities::OSR);
}

void EdgeWebview2Backend::clearCookies() {
    if (m_webview) {
        // TODO
    }
}

void EdgeWebview2Backend::initWebView(HWND hwnd) {
    std::wstring userDataFolder;
    if (!m_params.userDataPath.empty()) {
        userDataFolder = std::wstring(m_params.userDataPath.begin(), m_params.userDataPath.end());
    }

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, 
        userDataFolder.empty() ? nullptr : userDataFolder.c_str(), 
        nullptr,
        wrl::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, hwnd](HRESULT, ICoreWebView2Environment* env) -> HRESULT {
                env->CreateCoreWebView2Controller(
                    hwnd,
                    wrl::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT, ICoreWebView2Controller* controller) -> HRESULT {
                            if (controller) {
                                m_controller = controller;
                                m_controller->get_CoreWebView2(&m_webview);
                            }

                            if (m_webview) {
                                ICoreWebView2Settings* Settings;
                                m_webview->get_Settings(&Settings);
                                Settings->put_IsScriptEnabled(TRUE);
                                Settings->put_AreDefaultScriptDialogsEnabled(TRUE);
                                Settings->put_IsWebMessageEnabled(TRUE);

                                {
                                	// Step 4 - Navigation events
					// register an ICoreWebView2NavigationStartingEventHandler to cancel any non-https
					// navigation
					EventRegistrationToken token;
					m_webview->add_NavigationStarting(
						wrl::Callback<ICoreWebView2NavigationStartingEventHandler>(
					    [this](ICoreWebView2 *webview,
					       ICoreWebView2NavigationStartingEventArgs *args) -> HRESULT
					    {
						PWSTR uri;
						args->get_Uri(&uri);
						std::wstring source(uri);
						// if (source.substr(0, 5) != L"https")
						// {
						//     args->put_Cancel(true);
						// }
					    	if (m_navigationStartingCallback) {
					    		m_navigationStartingCallback(QString::fromStdWString(source).toStdString());
					    	}
						CoTaskMemFree(uri);
						return S_OK;
					    })
					    .Get(),
					&token);

                                	// Step 6 - Communication between host and web content
				// Set an event handler for the host to return received message back to the web content
				m_webview->add_WebMessageReceived(
				wrl::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
				    [](ICoreWebView2 *webview,
				       ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT
				    {
					PWSTR message;
					args->TryGetWebMessageAsString(&message);
					// processMessage(&message);
					webview->PostWebMessageAsString(message);
					CoTaskMemFree(message);
					return S_OK;
				    })
				    .Get(),
				&token);

                                	// startup script
				    m_webview->AddScriptToExecuteOnDocumentCreated(L"Object.freeze(Object);", nullptr);

				    // Schedule an async task to get the document URL
				m_webview->ExecuteScript(
					L"window.document.URL;",
				wrl::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
				    [](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT
				    {
					LPCWSTR URL = resultObjectAsJson;
					// doSomethingWithURL(URL);
					return S_OK;
				    })
				    .Get());
                                }

                                // Set initial bounds
                                RECT bounds;
                                bounds.left = m_params.x;
                                bounds.top = m_params.y;
                                bounds.right = m_params.x + m_params.width;
                                bounds.bottom = m_params.y + m_params.height;
                                m_controller->put_Bounds(bounds);

                            	resizeWebView();

                                if (!m_params.initialUrl.empty()) {
                                    loadUrl(m_params.initialUrl);
                                }
                                
                                if (m_readyCallback) {
                                    m_readyCallback();
                                }
                            }
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
}

void EdgeWebview2Backend::resizeWebView() {
	if (m_controller) {
		RECT bounds;
		GetClientRect(static_cast<HWND>(m_params.parentWindowId), &bounds);
		m_controller->put_Bounds(bounds);
	}
}
