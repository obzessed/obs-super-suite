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
void EdgeWebview2Backend::setAudioMuted(bool muted)
{
	if (m_webview) {
		if (const auto wv2_8 = m_webview.try_query<ICoreWebView2_8>()) {
			wv2_8->put_IsMuted(muted);
		}
	}
}
bool EdgeWebview2Backend::isAudioMuted() const
{
	if (m_webview) {
		BOOL muted;
		const auto wv2_8 = m_webview.try_query<ICoreWebView2_8>();
		if (!wv2_8) {
			return false; // Not supported, assume not muted
		}
		if (SUCCEEDED(wv2_8->get_IsMuted(&muted))) {
			return muted;
		}
	}

	return false;
}
bool EdgeWebview2Backend::isPlayingAudio() const
{
	if (m_webview) {
		BOOL playing;
		const auto wv2_8 = m_webview.try_query<ICoreWebView2_8>();
		if (!wv2_8) {
			return false; // Not supported, assume not playing
		}
		if (SUCCEEDED(wv2_8->get_IsDocumentPlayingAudio(&playing))) {
			return playing;
		}
	}
	return false;
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
                                ICoreWebView2Settings* wv2_settings;
                                m_webview->get_Settings(&wv2_settings);
                                wv2_settings->put_IsScriptEnabled(TRUE);
                                wv2_settings->put_AreDefaultScriptDialogsEnabled(TRUE);
                                wv2_settings->put_IsWebMessageEnabled(TRUE);
                                wv2_settings->put_IsZoomControlEnabled(TRUE);
                                wv2_settings->put_IsBuiltInErrorPageEnabled(FALSE);

                                {
					// register an ICoreWebView2NavigationStartingEventHandler
					EventRegistrationToken token;
					m_webview->add_NavigationStarting(
						wrl::Callback<ICoreWebView2NavigationStartingEventHandler>(
					    [this](ICoreWebView2 *webview,
					       ICoreWebView2NavigationStartingEventArgs *args) -> HRESULT
					    {
						PWSTR uri;
						args->get_Uri(&uri);
						std::wstring source(uri);
					    	if (m_navigationStartingCallback) {
					    		m_navigationStartingCallback(QString::fromStdWString(source).toStdString());
					    	}
						CoTaskMemFree(uri);
						return S_OK;
					    })
					    .Get(),
					&token);

					// Set an event handler for the host to return received message back to the web content
					// m_webview->add_WebMessageReceived(
					// wrl::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
					//     [](ICoreWebView2 *webview,
					//        ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT
					//     {
					// 	PWSTR message;
					// 	args->TryGetWebMessageAsString(&message);
					// 	// processMessage(&message);
					// 	webview->PostWebMessageAsString(message);
					// 	CoTaskMemFree(message);
					// 	return S_OK;
					//     })
					//     .Get(),
					// &token);
                                }

                            	// Audio Callbacks
                                if (const auto webview2_8 = m_webview.try_query<ICoreWebView2_8>()) {
                                	EventRegistrationToken token;
					// Register a handler for the IsDocumentPlayingAudioChanged event.
				    webview2_8->add_IsDocumentPlayingAudioChanged(
					wrl::Callback<ICoreWebView2IsDocumentPlayingAudioChangedEventHandler>(
					    [this, webview2_8](ICoreWebView2* sender, IUnknown* args) -> HRESULT
					    {
						    if (m_audioPlayingChangedCallback) {
							m_audioPlayingChangedCallback();
						    }
						return S_OK;
					    })
					    .Get(),
					&token);

				    // Register a handler for the IsMutedChanged event.
				    webview2_8->add_IsMutedChanged(
					wrl::Callback<ICoreWebView2IsMutedChangedEventHandler>(
					    [this, webview2_8](ICoreWebView2* sender, IUnknown* args) -> HRESULT
					    {
						    if (m_mutedStateChangeCallback) {
							    BOOL muted;
						    if (SUCCEEDED(webview2_8->get_IsMuted(&muted))) {
							    m_mutedStateChangeCallback(muted);
						    }
						    }
						return S_OK;
					    })
					    .Get(),
					&token);
				}

                            	// Context Menu Callbacks
                            	if (const auto webview2_11 = m_webview.try_query<ICoreWebView2_11>()) {
                            		EventRegistrationToken token;
										    // Register a handler for the ContextMenuRequested event.
					webview2_11->add_ContextMenuRequested(
					    wrl::Callback<ICoreWebView2ContextMenuRequestedEventHandler>(
						    [this](ICoreWebView2* sender, ICoreWebView2ContextMenuRequestedEventArgs* args) -> HRESULT {
						    	wil::com_ptr<ICoreWebView2ContextMenuItemCollection> items;
						    	if (!SUCCEEDED(args->get_MenuItems(&items))) {
						    		return S_OK; // If we fail to get menu items, just skip custom handling
						    	}

						    	wil::com_ptr<ICoreWebView2ContextMenuItem> current;
							UINT32 itemsCount;

							if (SUCCEEDED(items->get_Count(&itemsCount))) {
								for (UINT32 i = 0; i < itemsCount; i++) {
									if (SUCCEEDED(items->GetValueAtIndex(i, &current))) {
										COREWEBVIEW2_CONTEXT_MENU_ITEM_KIND kind;
										if (SUCCEEDED(current->get_Kind(&kind))) {
											wil::unique_cotaskmem_string label;
											if (SUCCEEDED(current->get_Label(&label))) {
												if (kind == COREWEBVIEW2_CONTEXT_MENU_ITEM_KIND_COMMAND)
												{
													current->put_IsEnabled(false); // Disable all default command items
												}
											}
										}
									}
								}
							}

						    	// args->put_Handled(true);

						  //   	auto showMenu = [this, args =
								// wil::com_ptr<ICoreWebView2ContextMenuRequestedEventArgs>(args)]
								// {
								//     wil::com_ptr<ICoreWebView2ContextMenuItemCollection> items;
								//     CHECK_FAILURE(args->get_MenuItems(&items));
								//     CHECK_FAILURE(args->put_Handled(true));
								//     HMENU hPopupMenu = CHECK_POINTER(CreatePopupMenu());
								//     AddMenuItems(hPopupMenu, items);
								//     HWND hWnd;
								//     m_controller->get_ParentWindow(&hWnd);
								//     POINT locationInControlCoordinates;
								//     POINT locationInScreenCoordinates;
								//     CHECK_FAILURE(args->get_Location(&locationInControlCoordinates));
								//     // get_Location returns coordinates in relation to upper left Bounds
								//     // of the WebView2.Controller. Will need to convert to Screen
								//     // coordinates to display the popup menu in the correct location.
								//     ConvertToScreenCoordinates(locationInControlCoordinates,
								// 			       locationInScreenCoordinates);
								//     UINT32 selectedCommandId = TrackPopupMenu(
								// 			   hPopupMenu,
								// 			   TPM_TOPALIGN | TPM_LEFTALIGN | TPM_RETURNCMD,
								// 			   locationInScreenCoordinates.x,
								// 			   locationInScreenCoordinates.y,
								// 			   0,
								// 			   hWnd,
								// 			   NULL);
								//     if (selectedCommandId != 0) {
								// 	CHECK_FAILURE(args->put_SelectedCommandId(selectedCommandId));
								//     }
								// };
								// wil::com_ptr<ICoreWebView2Deferral> deferral;
								// CHECK_FAILURE(args->GetDeferral(&deferral));
								// m_sampleWindow->RunAsync([deferral, showMenu]() {
								//     showMenu();
								//     CHECK_FAILURE(deferral->Complete());
								// });

						    	return S_OK;
						    })
						    .Get(),
					    &token);
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
