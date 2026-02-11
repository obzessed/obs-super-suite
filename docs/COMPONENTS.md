# Components — obs-super-suite

## Overview

**obs-super-suite** (also known as "OBS Power Console") is a comprehensive frontend/UI-only plugin for OBS Studio. It provides advanced management tools, diagnostic viewers, and custom dockable panels to enhance the OBS user experience.

**Key Characteristics:**
*   **Frontend-Only**: Does NOT register any custom OBS sources, filters, outputs, or encoders. Instead, it manages existing OBS source types programmatically.
*   **Tech Stack**: C++20, Qt6 (Widgets/Core), CMake 3.28+.
*   **Platform**: Windows-focused (due to WebView2 integration), but architecture supports cross-platform build.

## Project Structure

The source code is organized under `src/`:

```
src/
├── plugin-main.c               # OBS module entry point (C)
├── super_suite.h/.cpp          # Main plugin logic (C++), source management, menu registration
├── plugin-support.h/.c.in      # Generated plugin support macros
├── models/
│   └── audio_channel_source_config.h/.cpp  # Config singleton & AsioSourceConfig struct
├── dialogs/                    # Management dialogs & viewers
│   ├── audio_channels.h/.cpp       # Audio source manager dialog
│   ├── audio_source_dialog.h/.cpp  # Add/Edit/Duplicate source dialog
│   ├── channels_viewer.h/.cpp      # Canvas channels tree viewer
│   ├── outputs_viewer.h/.cpp       # Outputs table viewer
│   ├── encoders_viewer.h/.cpp      # Encoders table viewer
│   ├── canvas_manager.h/.cpp       # Canvas manager dialog
│   └── browser_manager.h/.cpp      # Browser dock CRUD manager
├── docks/                      # Dockable panels
│   ├── mixer_dock.h/.cpp           # Custom audio mixer dock
│   ├── browser-dock.hpp/.cpp       # Browser dock widget
│   └── wrapper_test_dock.h/.cpp    # OBS wrapper test dock
├── components/                 # Reusable UI widgets
│   ├── mixer_channel.h/.cpp        # Individual mixer channel strip
│   ├── mixer_meter.hpp/.cpp        # Custom painted level meter
│   └── qwebviewx.hpp/.cpp          # Multi-backend browser abstraction
├── windows/                    # Secondary window management
│   ├── dock_window_manager.h/.cpp  # Secondary window manager
│   └── secondary_window.h/.cpp     # Secondary window widget
├── lib/                        # OBS C++ Wrapper Library
│   ├── handle.hxx                  # Local/Ref/WeakRef handle templates
│   ├── traits.hxx                  # HandleTraits specializations
│   ├── obs.hxx                     # OBS type wrappers + debug utilities
│   └── obs.cxx                     # (empty) out-of-line implementations
├── browsers/backends/          # Browser backend implementations
│   ├── base.hpp                    # BrowserBackend abstract base
│   ├── edge_webview2.hpp           # WebView2 backend (Windows)
│   ├── obs_browser_cef.hpp         # OBS Browser CEF backend
│   └── standalone_cef.hpp          # Standalone CEF backend (stub)
├── utils/                      # Utilities
│   ├── browser-panel.hpp           # OBS browser panel helpers
│   ├── qcef_helper.hpp             # QCef initialization helpers
│   ├── color.hpp/.cpp              # Color utilities
│   └── widgets/                    # General purpose widgets
│       ├── qt-display.hpp/.cpp     # OBS display widget
│       ├── double-slider.hpp/.cpp  # Double precision slider
│       ├── slider-ignore-scroll.hpp # Scroll-ignoring slider
│       └── display-helpers.hpp     # Display helper functions
└── vendor/                     # Third-party code
    ├── master-level-meter/         # (disabled) Master level meter
    └── modular-multiview/          # (disabled) Multiview plugin
```

## Plugin Lifecycle

The plugin follows the standard OBS module lifecycle, bridging C and C++ worlds.

1.  **Entry Point (`src/plugin-main.c`)**:
    *   `obs_module_load()`: Calls C++ `on_plugin_load()`.
    *   `obs_module_post_load()`: Calls C++ `on_plugin_loaded()`.
    *   `obs_module_unload()`: Calls C++ `on_plugin_unload()`.

2.  **Core Logic (`src/super_suite.cpp`)**:
    *   **Load**: Initializes singletons, loads config.
    *   **Post-Load**: Registers **7 Tools menu items** and **2+ docks** via `obs_frontend_api`.
    *   **Unload**: Cleans up resources, saves config.
    *   **Event Handling**: Listens for `OBS_FRONTEND_EVENT_FINISHED_LOADING`, `SCENE_COLLECTION_CHANGED`, etc. to trigger source synchronization.

## Core Features

### 1. Audio Channel Source Manager
*   **Purpose**: Creates and manages global audio sources (WASAPI/ASIO) pinned to OBS canvas channels (1-64) that persist across scenes.
*   **Files**: `super_suite.cpp`, `models/audio_channel_source_config.h/.cpp`, `dialogs/audio_channels.h/.cpp`.
*   **Mechanism**:
    *   Maintains a list of `AsioSourceConfig` entries.
    *   Uses a guard flag `creating_sources` to prevent recursion during batch updates.
    *   Connects to 13 distinct OBS signals (rename, volume, mute, etc.) to keep UI in sync with OBS.
*   **Persistence**: `audio-channels.json` via `AudioChSrcConfig` singleton.

### 2. Super Mixer Dock
*   **Purpose**: A DAW-style channel strip mixer with faders, meters, and inline effects control.
*   **Files**: `docks/mixer_dock.cpp`, `components/mixer_channel.cpp`, `components/mixer_meter.cpp`.
*   **Components**:
    *   `MixerDock`: Horizontal scroll container.
    *   `MixerChannel`: Individual strip with fader (cubic taper), mute/cue/link buttons, and filter list.
    *   `MixerMeter`: Custom painted widget showing RMS/Peak levels with green/yellow/red zones.

### 3. Browser Dock Manager
*   **Purpose**: Advanced management of browser docks with support for multiple backends.
*   **Files**: `dialogs/browser_manager.cpp`, `docks/browser-dock.cpp`, `components/qwebviewx.cpp`.
*   **Backends** (`src/browsers/backends/`):
    1.  **OBS Browser CEF**: Wraps the standard `obs-browser` plugin (default).
    2.  **Edge WebView2**: Uses Microsoft Edge WebView2 control (Windows only).
    3.  **Standalone CEF**: Stub for future standalone CEF integration.
*   **Features**: Custom CSS/JS injection, presets (Google, WhatsApp, Telegram), per-dock user data paths.

### 4. Diagnostic Viewers
A suite of read-only dialogs for inspecting OBS internal state:
*   **Channels Viewer**: Tree view of all canvas channels and assigned sources.
*   **Outputs Viewer**: Table of all active `obs_output_t` instances.
*   **Encoders Viewer**: Table of all active `obs_encoder_t` instances.
*   **Canvas Manager**: List of all OBS canvases with resolution/FPS details.

### 5. Dock Window Manager
*   **Purpose**: Manages secondary dockable windows (floating windows that can host docks).
*   **Files**: `windows/dock_window_manager.cpp`, `windows/secondary_window.cpp`.
*   **Features**: Layout snapshots, transparency control, "stay on top" toggles.

### 6. Encoding Graph Tool
*   **Purpose**: Visualizes the OBS encoding pipeline (Sources -> Tracks -> Encoders -> Outputs).
*   **Files**: `dialogs/encoding_graph_dialog.cpp/.h`.
*   **Features**:
    *   Interactive Node Graph (QGraphicsView).
    *   Double-click nodes (Sources) to open standard OBS properties.
    *   Visualizes Audio Mixer bitmask connections.
    *   Auto-layout with sorting for stability.

### 7. OBS C++ Wrapper Library (`src/lib/`)
A custom header-only library providing modern C++ wrappers for OBS C API objects.
*   **Pattern**: V8-style handle system.
    *   `Local<T>`: Move-only owning handle, auto-releases.
    *   `Ref<T>`: Copyable shared ownership.
    *   `WeakRef<T>`: Weak reference.
*   **Wrappers**: `obs::Source`, `obs::Scene`, `obs::Data`, `obs::Encoder`, etc.
*   **Debug**: `WrapperTestDock` (`src/docks/wrapper_test_dock.cpp`) provides buttons to test reference counting and object liveness.

## Build & Dependencies

*   **System**: CMake 3.28+
*   **Language**: C++20
*   **Dependencies**:
    *   **OBS Studio** (libobs, obs-frontend-api)
    *   **Qt6** (Core, Widgets)
    *   **nlohmann/json** (v3.12.0 via CPM)
    *   **WebView2 SDK** (via NuGet/FetchContent)

## Configuration & Persistence

The plugin uses a dual persistence model:
1.  **File-based**: `audio-channels.json` stores the Audio Channel Source configuration.
2.  **OBS Scene Collection**: `DockWindowManager` and `BrowserManager` state is saved directly into the OBS scene collection JSON via `obs_frontend_add_save_callback`.

## Localization

*   **File**: `data/locale/en-US.ini`
*   **Coverage**: ~100 strings covering all dialogs, menus, and error messages.
