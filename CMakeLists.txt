cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)
include(cpm) # Our CMake Package Manager

#CPMAddPackage("gh:fmtlib/fmt#12.1.0")
CPMAddPackage(
  URI "gh:nlohmann/json@3.12.0"
  OPTIONS "JSON_BuildTests OFF"
)

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
)
# Exclude test executable from plugin sources
list(FILTER PLUGIN_SOURCES EXCLUDE REGEX "test-.*\\.cpp$")
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
