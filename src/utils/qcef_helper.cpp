#include "qcef_helper.hpp"

int QCefHelper::version_ = -1;
QCef* QCefHelper::instance_ = nullptr;
QCefCookieManager* QCefHelper::cookie_manager_ = nullptr;