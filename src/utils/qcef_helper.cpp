#include "qcef_helper.hpp"

int QCefHelper::g_version_ = -1;

QCef* QCefHelper::g_instance_ = nullptr;

QCefCookieManager* QCefHelper::g_cookie_manager_ = nullptr;