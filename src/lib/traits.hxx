#pragma once

#include <obs.h>

/**
 * @file obs.hxx
 * @brief C++ wrapper for OBS API with safe reference counting
 *
 * Uses V8-style Handle pattern:
 * - Local<T>   : Move-only owning handle
 * - Ref<T>     : Copyable shared ownership
 * - WeakRef<T> : Weak reference with lock()
 *
 * All nullable results use std::optional.
 */

namespace obs {
// Forward declarations
class Source;
class Scene;
class SceneItem;
class Data;
class DataArray;
class Encoder;
class Output;
class Service;
class Canvas;

//=============================================================================
// HandleTraits Specializations
//=============================================================================

// Source traits
template<>
struct HandleTraits<Source> {
	using RawType = obs_source_t;
	using WeakType = obs_weak_source_t;

	static void release(obs_source_t* p) {
		obs_source_release(p);
	}
	static obs_source_t* get_ref(obs_source_t* p) {
		return obs_source_get_ref(p);
	}
	static obs_weak_source_t* get_weak(obs_source_t* p) {
		return obs_source_get_weak_source(p);
	}
	static obs_source_t* from_weak(obs_weak_source_t* w) {
		return obs_weak_source_get_source(w);
	}
	static void addref_weak(obs_weak_source_t* w) {
		obs_weak_source_addref(w);
	}
	static void release_weak(obs_weak_source_t* w) {
		obs_weak_source_release(w);
	}
};

// Scene uses same traits as Source (obs_scene_t is obs_source_t internally)
template<>
struct HandleTraits<Scene> {
	using RawType = obs_scene_t;
	using WeakType = obs_weak_source_t;

	static void release(obs_scene_t* p) {
		obs_scene_release(p);
	}
	static obs_scene_t* get_ref(obs_scene_t* p) {
		// Scene doesn't have direct get_ref, use source
		obs_source_t* src = obs_scene_get_source(p);
		if (obs_source_get_ref(src)) {
			return p;
		}
		return nullptr;
	}
	static obs_weak_source_t* get_weak(obs_scene_t* p) {
		obs_source_t* src = obs_scene_get_source(p);
		return obs_source_get_weak_source(src);
	}
	static obs_scene_t* from_weak(obs_weak_source_t* w) {
		obs_source_t* src = obs_weak_source_get_source(w);
		if (src) {
			return obs_scene_from_source(src);
		}
		return nullptr;
	}
	static void addref_weak(obs_weak_source_t* w) {
		obs_weak_source_addref(w);
	}
	static void release_weak(obs_weak_source_t* w) {
		obs_weak_source_release(w);
	}
};

// SceneItem traits
template<>
struct HandleTraits<SceneItem> {
	using RawType = obs_sceneitem_t;
	using WeakType = void; // SceneItem doesn't have weak refs

	static void release(obs_sceneitem_t* p) {
		obs_sceneitem_release(p);
	}
	static obs_sceneitem_t* get_ref(obs_sceneitem_t* p) {
		obs_sceneitem_addref(p);
		return p;
	}
	// No weak reference support for SceneItem
	static void* get_weak(obs_sceneitem_t*) { return nullptr; }
	static obs_sceneitem_t* from_weak(void*) { return nullptr; }
	static void addref_weak(void*) {}
	static void release_weak(void*) {}
};

// Data traits (obs_data_t)
template<>
struct HandleTraits<Data> {
	using RawType = obs_data_t;
	using WeakType = void; // Data doesn't have weak refs

	static void release(obs_data_t* p) {
		obs_data_release(p);
	}
	static obs_data_t* get_ref(obs_data_t* p) {
		obs_data_addref(p);
		return p;
	}
	static void* get_weak(obs_data_t*) { return nullptr; }
	static obs_data_t* from_weak(void*) { return nullptr; }
	static void addref_weak(void*) {}
	static void release_weak(void*) {}
};

// DataArray traits
template<>
struct HandleTraits<DataArray> {
	using RawType = obs_data_array_t;
	using WeakType = void;

	static void release(obs_data_array_t* p) {
		obs_data_array_release(p);
	}
	static obs_data_array_t* get_ref(obs_data_array_t* p) {
		obs_data_array_addref(p);
		return p;
	}
	static void* get_weak(obs_data_array_t*) { return nullptr; }
	static obs_data_array_t* from_weak(void*) { return nullptr; }
	static void addref_weak(void*) {}
	static void release_weak(void*) {}
};

// Encoder traits
template<>
struct HandleTraits<Encoder> {
	using RawType = obs_encoder_t;
	using WeakType = obs_weak_encoder_t;

	static void release(obs_encoder_t* p) {
		obs_encoder_release(p);
	}
	static obs_encoder_t* get_ref(obs_encoder_t* p) {
		return obs_encoder_get_ref(p);
	}
	static obs_weak_encoder_t* get_weak(obs_encoder_t* p) {
		return obs_encoder_get_weak_encoder(p);
	}
	static obs_encoder_t* from_weak(obs_weak_encoder_t* w) {
		return obs_weak_encoder_get_encoder(w);
	}
	static void addref_weak(obs_weak_encoder_t* w) {
		obs_weak_encoder_addref(w);
	}
	static void release_weak(obs_weak_encoder_t* w) {
		obs_weak_encoder_release(w);
	}
};

// Output traits
template<>
struct HandleTraits<Output> {
	using RawType = obs_output_t;
	using WeakType = obs_weak_output_t;

	static void release(obs_output_t* p) {
		obs_output_release(p);
	}
	static obs_output_t* get_ref(obs_output_t* p) {
		return obs_output_get_ref(p);
	}
	static obs_weak_output_t* get_weak(obs_output_t* p) {
		return obs_output_get_weak_output(p);
	}
	static obs_output_t* from_weak(obs_weak_output_t* w) {
		return obs_weak_output_get_output(w);
	}
	static void addref_weak(obs_weak_output_t* w) {
		obs_weak_output_addref(w);
	}
	static void release_weak(obs_weak_output_t* w) {
		obs_weak_output_release(w);
	}
};

// Service traits
template<>
struct HandleTraits<Service> {
	using RawType = obs_service_t;
	using WeakType = obs_weak_service_t;

	static void release(obs_service_t* p) {
		obs_service_release(p);
	}
	static obs_service_t* get_ref(obs_service_t* p) {
		return obs_service_get_ref(p);
	}
	static obs_weak_service_t* get_weak(obs_service_t* p) {
		return obs_service_get_weak_service(p);
	}
	static obs_service_t* from_weak(obs_weak_service_t* w) {
		return obs_weak_service_get_service(w);
	}
	static void addref_weak(obs_weak_service_t* w) {
		obs_weak_service_addref(w);
	}
	static void release_weak(obs_weak_service_t* w) {
		obs_weak_service_release(w);
	}
};

// Canvas traits
template<>
struct HandleTraits<Canvas> {
	using RawType = obs_canvas_t;
	using WeakType = obs_weak_canvas_t;

	static void release(obs_canvas_t* p) {
		obs_canvas_release(p);
	}
	static obs_canvas_t* get_ref(obs_canvas_t* p) {
		return obs_canvas_get_ref(p);
	}
	static obs_weak_canvas_t* get_weak(obs_canvas_t* p) {
		return obs_canvas_get_weak_canvas(p);
	}
	static obs_canvas_t* from_weak(obs_weak_canvas_t* w) {
		return obs_weak_canvas_get_canvas(w);
	}
	static void addref_weak(obs_weak_canvas_t* w) {
		obs_weak_canvas_addref(w);
	}
	static void release_weak(obs_weak_canvas_t* w) {
		obs_weak_canvas_release(w);
	}
};

}