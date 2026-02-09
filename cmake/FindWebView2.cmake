# FindWebView2.cmake
#
# Finds or downloads the WebView2 SDK and Windows Implementation Library (WIL)
#
# This module defines the following targets:
#   WebView2::WebView2 - The main target to link against

## Microsoft.Windows.ImplementationLibrary
#FetchContent_Declare(
#    wil
#    URL  https://www.nuget.org/api/v2/package/Microsoft.Windows.ImplementationLibrary/1.0.211019.2
#    URL_HASH SHA1=FB082B22DC30C11CCDB118A0EB9B8A32E99765B1
#)
## Microsoft.Web.WebView2
#FetchContent_Declare(
#    webview2
#    URL  https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.1072.54
#    URL_HASH SHA1=aa8ae9db5015a9184011bb195efc5c8caa58a86b
#)
#FetchContent_MakeAvailable(wil webview2)
#
#include_directories(SYSTEM "${wil_SOURCE_DIR}/include/")
#include_directories(SYSTEM "${webview2_SOURCE_DIR}/build/native/include/")
#
#if(CMAKE_SIZEOF_VOID_P EQUAL 8)
#  set(PLATFORM "x64")
#elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
#  set(PLATFORM "x86")
#endif()
#link_libraries("${webview2_SOURCE_DIR}/build/native/${PLATFORM}/WebView2LoaderStatic.lib")

include(FetchContent)
include(FindPackageHandleStandardArgs)

# 1. Architecture Detection
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WV2_ARCH "x64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(WV2_ARCH "x86")
else()
    set(WV2_ARCH "arm64")
endif()

FetchContent_Declare(
    wil
    URL  https://www.nuget.org/api/v2/package/Microsoft.Windows.ImplementationLibrary/1.0.260126.7
    URL_HASH SHA1=7d6e442ba79c1b5ddbeeba5f74ab994db142d755
)
FetchContent_Declare(
    webview2
    URL  https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.3719.77
    URL_HASH SHA1=fbefa6918dd53ba5ad1947b2eb51d5dd85dfe6f8
)
FetchContent_MakeAvailable(wil webview2)

# 4. Create the Interface Target if it doesn't exist
if(NOT TARGET WebView2::WebView2)
    add_library(WebView2::WebView2 INTERFACE IMPORTED)

    set_target_properties(WebView2::WebView2 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${wil_SOURCE_DIR}/include;${webview2_SOURCE_DIR}/build/native/include"
        INTERFACE_LINK_LIBRARIES "${webview2_SOURCE_DIR}/build/native/${WV2_ARCH}/WebView2LoaderStatic.lib;version.lib;user32.lib"
    )
endif()

# 5. Handle the standard "find_package" arguments
# Since we are downloading them, we consider it found if the source dirs exist
find_package_handle_standard_args(WebView2
    DEFAULT_MSG
    wil_SOURCE_DIR
    webview2_SOURCE_DIR
)