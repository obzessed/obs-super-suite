cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

include(FetchContent)

include(cpm) # Our CMake Package Manager

#CPMAddPackage("gh:fmtlib/fmt#12.1.0")
CPMAddPackage(
  URI "gh:nlohmann/json@3.12.0"
  OPTIONS "JSON_BuildTests OFF"
)
#CPMAddPackage("gh:Curve/lime#v6.0")
#CPMAddPackage("gh:Curve/channel#v2.3")
#CPMAddPackage("gh:celtera/libremidi#v5.4.3")

#CPMAddPackage(
#  URI "gh:winsoft666/QWebView#v2.0.1"
#  OPTIONS "QWEBVIEW_ENABLE_WEBVIEW2 ON" "QWEBVIEW_STATIC_MSVC_CRT ON" "QWEBVIEW_BUILD_SAMPLES OFF"
#)

# https://cef-builds.spotifycdn.com/index.html

#set(CEF_SDK_VERSION "144.0.13+g9f739aa+chromium-144.0.7559.133")
#set(CEF_SDK_VERSION "142.0.15+g6dfdb28+chromium-142.0.7444.176")

#CPMAddPackage(
#  URI "gh:CefView/QCefView#v2025.12.28"
#  OPTIONS "BUILD_STATIC OFF" "STATIC_CRT OFF" "BUILD_DEMO OFF"
#)

#set(CEFVIEW_WING_NAME "cef-helper-wing")
#CPMAddPackage(
#  URI "gh:CefView/CefViewCore#454c7246d1dd1c1a1888580e8972fac6691a5f44"
#  OPTIONS "STATIC_CRT ON"
#)

#if(TARGET CefViewWing)
#    if(MSVC)
#        target_link_options(CefViewWing PRIVATE "/IGNORE:4199")
#    endif()
#endif()

# Add the 'cmake' directory to the module search path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Use the module (cmake/FindWebView2.cmake)
find_package(WebView2 REQUIRED)

# Define NOMINMAX on Windows to prevent min/max macro conflicts
if(WIN32)
  add_compile_definitions(NOMINMAX)
endif()

add_library(${CMAKE_PROJECT_NAME} MODULE)

#if(TARGET ${CMAKE_PROJECT_NAME})
#    if(MSVC)
#        target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "/IGNORE:4199")
#    endif()
#endif()

# Enable C++20
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

# WinMM for MIDI input support
if(WIN32)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE winmm)
endif()

# link user deps
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  #  fmt::fmt
  nlohmann_json::nlohmann_json
  WebView2::WebView2 # WebView2 SDK
  # Util libraries
#  cr::lime
#  cr::channel
   # CEF
#  CefViewCore
#  QCefView
#  QWebView.CEF
#  libremidi
)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core Gui Svg Xml Network)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Svg Qt6::Xml Qt6::Widgets Qt6::Network)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/plugin-version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin-version.h)

file(GLOB_RECURSE PLUGIN_SOURCES
  src/*.c
  src/*.cc
  src/*.cpp
  src/*.cxx
)
# Exclude test executable from plugin sources
list(FILTER PLUGIN_SOURCES EXCLUDE REGEX "test-.*\\.c$")
list(FILTER PLUGIN_SOURCES EXCLUDE REGEX "test-.*\\.cc$")
list(FILTER PLUGIN_SOURCES EXCLUDE REGEX "test-.*\\.cpp$")
list(FILTER PLUGIN_SOURCES EXCLUDE REGEX "test-.*\\.cxx$")

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${PLUGIN_SOURCES})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  resources.qrc
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# ============================================================================
# Deploy to OBS plugins directory (based on OBS_PLUGINS_DIR env var)
# ============================================================================
if ($ENV{COPY_PLUGIN_TO_DIR} AND DEFINED ENV{OBS_INSTALL_DIR})
  set(OBS_INSTALL_DIR "$ENV{OBS_INSTALL_DIR}")
  set(OBS_PLUGINS_DIR "${OBS_INSTALL_DIR}/obs-plugins/64bit")

  message(STATUS "OBS_INSTALL_DIR: ${OBS_INSTALL_DIR}")
  message(STATUS "OBS_PLUGINS_DIR: ${OBS_PLUGINS_DIR}")

  add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OBS_PLUGINS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>" "${OBS_PLUGINS_DIR}"
    COMMENT "Deploying plugin to OBS plugins directory: ${OBS_PLUGINS_DIR}"
  )

  # Copy plugin PDB if it exists
  if (EXISTS "$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>")
    add_custom_command(
      TARGET ${CMAKE_PROJECT_NAME}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>" "${OBS_PLUGINS_DIR}"
      COMMENT "Deploying plugin PDB to OBS plugins directory"
    )
  endif()

  set(OBS_PLUGINS_DATA_DIR "${OBS_INSTALL_DIR}/data/obs-plugins")

  # Copy data folder to OBS plugins directory (in subfolder named after the plugin)
  add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OBS_PLUGINS_DATA_DIR}/${CMAKE_PROJECT_NAME}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/data" "${OBS_PLUGINS_DATA_DIR}/${CMAKE_PROJECT_NAME}"
    COMMENT "Deploying plugin data folder to: ${OBS_PLUGINS_DATA_DIR}/${CMAKE_PROJECT_NAME}"
  )
else()
  message(STATUS "COPY_PLUGIN_TO_DIR or OBS_INSTALL_DIR is not set - skipping deployment to OBS")
endif()
