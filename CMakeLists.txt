cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)
include(cpm) # Our CMake Package Manager

include(FetchContent)

#CPMAddPackage("gh:fmtlib/fmt#12.1.0")
CPMAddPackage(
  URI "gh:nlohmann/json@3.12.0"
  OPTIONS "JSON_BuildTests OFF"
)
#set(CEF_SDK_VERSION "144.0.13+g9f739aa+chromium-144.0.7559.133")
#CPMAddPackage(
#  URI "gh:CefView/QCefView#v1.2.1"
#  OPTIONS "BUILD_STATIC ON" "STATIC_CRT OFF" "BUILD_DEMO OFF"
#)

#if(TARGET CefViewWing)
#    if(MSVC)
#        target_link_options(CefViewWing PRIVATE "/IGNORE:4199")
#    endif()
#endif()

# Microsoft.Windows.ImplementationLibrary
# The Windows Implementation Libraries (wil) were created to improve productivity and solve problems commonly seen by Windows developers. by: Microsoft WindowsImplementationLibraryTeam
FetchContent_Declare(
    wil
    URL  https://www.nuget.org/api/v2/package/Microsoft.Windows.ImplementationLibrary/1.0.211019.2
    URL_HASH SHA1=FB082B22DC30C11CCDB118A0EB9B8A32E99765B1
)
# Microsoft.Web.WebView2
FetchContent_Declare(
    webview2
    URL  https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.1072.54
    URL_HASH SHA1=aa8ae9db5015a9184011bb195efc5c8caa58a86b
)
FetchContent_MakeAvailable(wil webview2)

include_directories(SYSTEM "${wil_SOURCE_DIR}/include/")
include_directories(SYSTEM "${webview2_SOURCE_DIR}/build/native/include/")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(PLATFORM "x64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(PLATFORM "x86")
endif()
link_libraries("${webview2_SOURCE_DIR}/build/native/${PLATFORM}/WebView2LoaderStatic.lib")

# Define NOMINMAX on Windows to prevent min/max macro conflicts
if(WIN32)
  add_compile_definitions(NOMINMAX)
endif()

add_library(${CMAKE_PROJECT_NAME} MODULE)

# Enable C++20
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

# link user deps
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  #  fmt::fmt
  nlohmann_json::nlohmann_json
#  QCefView
)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

file(GLOB_RECURSE PLUGIN_SOURCES
  src/*.c
  src/*.cc
  src/*.cpp
  src/*.cxx
)
# Exclude test executable from plugin sources
list(FILTER PLUGIN_SOURCES EXCLUDE REGEX "test-.*\\.cpp$")
list(FILTER PLUGIN_SOURCES EXCLUDE REGEX "test-.*\\.cxx$")
list(FILTER PLUGIN_SOURCES EXCLUDE REGEX "test-.*\\.cc$")
list(FILTER PLUGIN_SOURCES EXCLUDE REGEX "test-.*\\.c$")

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${PLUGIN_SOURCES})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  resources.qrc
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# ============================================================================
# Deploy to OBS plugins directory (based on OBS_PLUGINS_DIR env var)
# ============================================================================
if(DEFINED ENV{OBS_PLUGINS_DIR})
  set(OBS_PLUGINS_DIR $ENV{OBS_PLUGINS_DIR})
  message(STATUS "OBS_PLUGINS_DIR: ${OBS_PLUGINS_DIR}")

  add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OBS_PLUGINS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>" "${OBS_PLUGINS_DIR}"
    COMMENT "Deploying plugin to OBS plugins directory: ${OBS_PLUGINS_DIR}"
  )

  # Copy plugin PDB if it exists
  add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>" "${OBS_PLUGINS_DIR}"
    COMMENT "Deploying plugin PDB to OBS plugins directory"
  )

  set(PLUGINS_DATA_DIR "${OBS_PLUGINS_DIR}/../../data/obs-plugins")

  # Copy data folder to OBS plugins directory (in subfolder named after the plugin)
  add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGINS_DATA_DIR}/${CMAKE_PROJECT_NAME}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/data" "${PLUGINS_DATA_DIR}/${CMAKE_PROJECT_NAME}"
    COMMENT "Deploying plugin data folder to: ${PLUGINS_DATA_DIR}/${CMAKE_PROJECT_NAME}"
  )
else()
  message(STATUS "OBS_PLUGINS_DIR not set - skipping deployment to OBS")
endif()
