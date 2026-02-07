#pragma once

#include <obs.h>

#include "handle.hxx"
#include "plugin-support.h"
#include "traits.hxx"

#include <optional>
#include <functional>
#include <string>

namespace obs {
namespace detail {
	/// OBS wrapped object
	template<typename T>
	class OBSObject {
	protected:
		using RawType = HandleTraits<T>::RawType;

		explicit OBSObject(RawType* obj) : inner_(obj) {}

	public:
		[[nodiscard]] T* raw() const { return inner_; }
	protected:
		RawType* inner_ = nullptr;
	};
}

//=============================================================================
// Wrapper Classes
//=============================================================================

/**
 * @brief Wrapper for obs_data_t settings/properties
 */
class Data {
public:
    // Create empty data object
    static Local<Data> create() {
        return Local<Data>(obs_data_create());
    }

    // Create from JSON string
    static std::optional<Local<Data>> fromJson(const std::string& json) {
        auto* data = obs_data_create_from_json(json.c_str());
        if (!data) return std::nullopt;
        return Local<Data>(data);
    }

    // Get JSON string
    static std::string toJson(const Local<Data>& data) {
        const char* json = obs_data_get_json(data.raw());
        return json ? std::string(json) : std::string();
    }

    // Setters (static functions taking reference)
    static void setString(Local<Data>& data, const char* name, const char* val) {
        obs_data_set_string(data.raw(), name, val);
    }
    static void setInt(Local<Data>& data, const char* name, long long val) {
        obs_data_set_int(data.raw(), name, val);
    }
    static void setDouble(Local<Data>& data, const char* name, double val) {
        obs_data_set_double(data.raw(), name, val);
    }
    static void setBool(Local<Data>& data, const char* name, bool val) {
        obs_data_set_bool(data.raw(), name, val);
    }

    // Getters
    static std::string getString(const Local<Data>& data, const char* name) {
        const char* str = obs_data_get_string(data.raw(), name);
        return str ? std::string(str) : std::string();
    }
    static long long getInt(const Local<Data>& data, const char* name) {
        return obs_data_get_int(data.raw(), name);
    }
    static double getDouble(const Local<Data>& data, const char* name) {
        return obs_data_get_double(data.raw(), name);
    }
    static bool getBool(const Local<Data>& data, const char* name) {
        return obs_data_get_bool(data.raw(), name);
    }
};

/**
 * @brief Wrapper for obs_source_t
 */
class Source : public detail::OBSObject<Source> {
public: // public for now
    explicit Source(obs_source_t* src) : OBSObject(src) {}

public:
    // Factory: Create new source
    static std::optional<Local<Source>> create(
        const std::string& id,
        const std::string& name,
        Local<Data> settings = Local<Data>(),
        Local<Data> hotkeys = Local<Data>()
    ) {
        auto* src = obs_source_create(
            id.c_str(),
            name.c_str(),
            settings.raw(),
            hotkeys.raw()
        );
        if (!src) return std::nullopt;
        return Local<Source>(src);
    }

    // Factory: Create private source
    static std::optional<Local<Source>> createPrivate(
        const std::string& id,
        const std::string& name,
        Local<Data> settings = Local<Data>()
    ) {
        auto* src = obs_source_create_private(
            id.c_str(),
            name.c_str(),
            settings.raw()
        );
        if (!src) return std::nullopt;
        return Local<Source>(src);
    }

    // Find by name
    static std::optional<Local<Source>> findByName(const std::string& name) {
        auto* src = obs_get_source_by_name(name.c_str());
        if (!src) return std::nullopt;
        return Local<Source>(src);
    }

    // Find by UUID
    static std::optional<Local<Source>> findByUuid(const std::string& uuid) {
        auto* src = obs_get_source_by_uuid(uuid.c_str());
        if (!src) return std::nullopt;
        return Local<Source>(src);
    }

    // Enumerate all sources
    static void forEach(const std::function<bool(Local<Source>&, size_t)>& callback) {
        struct IterState {
            size_t index = 0;
            const std::function<bool(Local<Source>&, size_t)>* callback{};
        };

        auto cb = [](void* param, obs_source_t* src) -> bool {
            auto* state = static_cast<IterState*>(param);
	    const size_t index = state->index++;

            // Get our own reference for the callback
            // auto* ref = obs_source_get_ref(src);
            // if (!ref) {
            // 	obs_log(LOG_WARNING, "Source::forEach: failed to get ref of source at index: %d", index);
            // 	return true; // Skip if can't get ref
            // }

            Local<Source> wrapped(src, true); // this adds a ref to have it's ownership
            return (*state->callback)(wrapped, index);
        };

        IterState state{0, &callback};
        obs_enum_sources(cb, &state);
    }

    // Instance methods (take raw pointer or Local)
    static std::string getName(obs_source_t* src) {
        const char* n = obs_source_get_name(src);
        return n ? std::string(n) : std::string();
    }

    static std::string getUuid(obs_source_t* src) {
        const char* u = obs_source_get_uuid(src);
        return u ? std::string(u) : std::string();
    }

    static std::string getId(obs_source_t* src) {
        const char* id = obs_source_get_id(src);
        return id ? std::string(id) : std::string();
    }

    static std::string getUnversionedId(obs_source_t* src) {
        const char* id = obs_source_get_unversioned_id(src);
        return id ? std::string(id) : std::string();
    }

    static uint32_t getWidth(obs_source_t* src) {
        return obs_source_get_width(src);
    }

    static uint32_t getHeight(obs_source_t* src) {
        return obs_source_get_height(src);
    }

    static bool isActive(obs_source_t* src) {
        return obs_source_active(src);
    }

    static bool isShowing(obs_source_t* src) {
        return obs_source_showing(src);
    }

    static Local<Data> getSettings(obs_source_t* src) {
        return Local<Data>(obs_source_get_settings(src));
    }

    static void update(obs_source_t* src, Local<Data>& settings) {
        obs_source_update(src, settings.raw());
    }

    // Instance methods
    [[nodiscard]] std::string id() const { return getId(inner_); }
    [[nodiscard]] std::string unversionedId() const { return getUnversionedId(inner_); }

    [[nodiscard]] std::string name() const { return getName(inner_); }
    [[nodiscard]] std::string uuid() const { return getUuid(inner_); }

    [[nodiscard]] uint32_t height() const { return getHeight(inner_); }
    [[nodiscard]] uint32_t width() const { return getWidth(inner_); }

    [[nodiscard]] bool active() const { return isActive(inner_); }
    [[nodiscard]] bool showing() const { return isShowing(inner_); }
};

/**
 * @brief Wrapper for obs_scene_t
 */
class Scene {
public:
    // Create new scene
    static Local<Scene> create(const std::string& name) {
        return Local<Scene>(obs_scene_create(name.c_str()));
    }

    // Create private scene
    static Local<Scene> createPrivate(const std::string& name) {
        return Local<Scene>(obs_scene_create_private(name.c_str()));
    }

    // Find by name
    static std::optional<Local<Scene>> findByName(const std::string& name) {
        auto* scene = obs_get_scene_by_name(name.c_str());
        if (!scene) return std::nullopt;
        return Local<Scene>(scene);
    }

    // Get scene from source
    static std::optional<Local<Scene>> fromSource(const Local<Source>& source) {
        auto* scene = obs_scene_from_source(source.raw());
        if (!scene) return std::nullopt;
        // Scene from source doesn't add ref, but we need to manage it
        // Actually obs_scene_from_source just returns internal pointer
        // We need to get a ref via the source
        obs_source_get_ref(source.raw());
        return Local<Scene>(scene);
    }

    // Get source from scene
    static obs_source_t* getSource(obs_scene_t* scene) {
        return obs_scene_get_source(scene);
    }

    // Enumerate all scenes
    static void forEach(const std::function<bool(Local<Source>&, size_t)>& callback) {
        struct IterState {
            size_t index = 0;
            const std::function<bool(Local<Source>&, size_t)>* callback;
        };

        auto cb = [](void* param, obs_source_t* src) -> bool {
            auto* state = static_cast<IterState*>(param);
            auto* ref = obs_source_get_ref(src);
            if (!ref) return true;

            Local<Source> wrapped(ref);
            return (*state->callback)(wrapped, state->index++);
        };

        IterState state{0, &callback};
        obs_enum_scenes(cb, &state);
    }
};

/**
 * @brief Wrapper for obs_canvas_t
 */
class Canvas {
public:
    // Get main canvas
    static Local<Canvas> getMain() {
        return Local<Canvas>(obs_get_main_canvas());
    }

    // Find by name
    static std::optional<Local<Canvas>> findByName(const std::string& name) {
        auto* canvas = obs_get_canvas_by_name(name.c_str());
        if (!canvas) return std::nullopt;
        return Local<Canvas>(canvas);
    }

    // Find by UUID
    static std::optional<Local<Canvas>> findByUuid(const std::string& uuid) {
        auto* canvas = obs_get_canvas_by_uuid(uuid.c_str());
        if (!canvas) return std::nullopt;
        return Local<Canvas>(canvas);
    }

    // Enumerate all canvases
    static void forEach(const std::function<bool(Local<Canvas>&, size_t)>& callback) {
        struct IterState {
            size_t index = 0;
            const std::function<bool(Local<Canvas>&, size_t)>* callback;
        };

        auto cb = [](void* param, obs_canvas_t* canvas) -> bool {
            auto* state = static_cast<IterState*>(param);
            auto* ref = obs_canvas_get_ref(canvas);
            if (!ref) return true;

            Local<Canvas> wrapped(ref);
            return (*state->callback)(wrapped, state->index++);
        };

        IterState state{0, &callback};
        obs_enum_canvases(cb, &state);
    }

    // Instance methods (take raw pointer or Local)
    static std::string getName(const obs_canvas_t * cvs) {
    	const char* n = obs_canvas_get_name(cvs);
    	return n ? std::string(n) : std::string();
    }

    static std::string getUuid(const obs_canvas_t* cvs) {
    	const char* u = obs_canvas_get_uuid(cvs);
    	return u ? std::string(u) : std::string();
    }
};

/**
 * @brief Wrapper for obs_encoder_t
 */
class Encoder {
public:
    // Find by name
    static std::optional<Local<Encoder>> findByName(const std::string& name) {
        auto* enc = obs_get_encoder_by_name(name.c_str());
        if (!enc) return std::nullopt;
        return Local<Encoder>(enc);
    }

    // Enumerate encoders
    static void forEach(const std::function<bool(Local<Encoder>&, size_t)>& callback) {
        struct IterState {
            size_t index = 0;
            const std::function<bool(Local<Encoder>&, size_t)>* callback;
        };

        auto cb = [](void* param, obs_encoder_t* enc) -> bool {
            auto* state = static_cast<IterState*>(param);
            auto* ref = obs_encoder_get_ref(enc);
            if (!ref) return true;

            Local<Encoder> wrapped(ref);
            return (*state->callback)(wrapped, state->index++);
        };

        IterState state{0, &callback};
        obs_enum_encoders(cb, &state);
    }
};

/**
 * @brief Wrapper for obs_output_t
 */
class Output {
public:
    // Find by name
    static std::optional<Local<Output>> findByName(const std::string& name) {
        auto* out = obs_get_output_by_name(name.c_str());
        if (!out) return std::nullopt;
        return Local<Output>(out);
    }

    // Enumerate outputs
    static void forEach(const std::function<bool(Local<Output>&, size_t)>& callback) {
        struct IterState {
            size_t index = 0;
            const std::function<bool(Local<Output>&, size_t)>* callback{};
        };

        auto cb = [](void* param, obs_output_t* out) -> bool {
            auto* state = static_cast<IterState*>(param);
            auto* ref = obs_output_get_ref(out);
            if (!ref) return true;

            Local<Output> wrapped(ref);
            return (*state->callback)(wrapped, state->index++);
        };

        IterState state{0, &callback};
        obs_enum_outputs(cb, &state);
    }
};

/**
 * @brief Wrapper for obs_service_t
 */
class Service {
public:
    // Find by name
    static std::optional<Local<Service>> findByName(const std::string& name) {
        auto* svc = obs_get_service_by_name(name.c_str());
        if (!svc) return std::nullopt;
        return Local<Service>(svc);
    }
};

//=============================================================================
// Signal Handling
//=============================================================================

class SignalHandler {
    signal_handler_t* inner_;

    explicit SignalHandler(signal_handler_t* handler) : inner_(handler) {}
public:
    static SignalHandler get() {
        return SignalHandler(obs_get_signal_handler());
    }

    static SignalHandler of(const Local<Canvas>& canvas) {
    	return SignalHandler(obs_canvas_get_signal_handler(canvas.raw()));
    }

    static SignalHandler of(const Local<Source>& source) {
        return SignalHandler(obs_source_get_signal_handler(source.raw()));
    }
    
    signal_handler_t* raw() const { return inner_; }
};

//=============================================================================
// Debug/Test Helpers
//=============================================================================

/**
 * @brief Debug utilities for testing handle behavior
 * 
 * These functions help verify reference counting behavior by
 * attempting get_ref/release cycles to detect liveness.
 * 
 * WARNING: These are for debugging only - they temporarily
 * increment ref counts which could mask bugs if used incorrectly.
 */
namespace debug {

/**
 * @brief Check if a source is still alive by trying to get a reference
 * @return true if the source can still be referenced
 */
inline bool isAlive(obs_source_t* src) {
    if (!src) return false;
    if (auto* ref = obs_source_get_ref(src)) {
        obs_source_release(ref);
        return true;
    }
    return false;
}

/**
 * @brief Check if a canvas is still alive
 */
inline bool isAlive(obs_canvas_t* canvas) {
    if (!canvas) return false;
    if (auto* ref = obs_canvas_get_ref(canvas)) {
        obs_canvas_release(ref);
        return true;
    }
    return false;
}

/**
 * @brief Check if an encoder is still alive
 */
inline bool isAlive(obs_encoder_t* enc) {
    if (!enc) return false;
    if (auto* ref = obs_encoder_get_ref(enc)) {
        obs_encoder_release(ref);
        return true;
    }
    return false;
}

/**
 * @brief Check if an output is still alive
 */
inline bool isAlive(obs_output_t* out) {
    if (!out) return false;
    if (auto* ref = obs_output_get_ref(out)) {
        obs_output_release(ref);
        return true;
    }
    return false;
}

/**
 * @brief Check if a service is still alive
 */
inline bool isAlive(obs_service_t* svc) {
    if (!svc) return false;
    if (auto* ref = obs_service_get_ref(svc)) {
        obs_service_release(ref);
        return true;
    }
    return false;
}

/**
 * @brief RefCountProbe - Estimates ref count by observing get_ref behavior
 * 
 * OBS doesn't expose ref counts directly, but we can probe liveness.
 * This struct provides additional diagnostic info.
 */
struct RefCountProbe {
    bool isAlive;
    bool hasWeakRefs;  // true if weak ref can lock (object has weak observers)
    
    static RefCountProbe probe(obs_source_t* src) {
        RefCountProbe result{false, false};
        if (!src) return result;
        
        // Check if alive
        if (auto* ref = obs_source_get_ref(src)) {
            result.isAlive = true;
            obs_source_release(ref);
        }
        
        // Check weak ref behavior
        if (auto* weak = obs_source_get_weak_source(src)) {
            if (auto* locked = obs_weak_source_get_source(weak)) {
                result.hasWeakRefs = true;
                obs_source_release(locked);
            }
            obs_weak_source_release(weak);
        }
        
        return result;
    }
    
    static RefCountProbe probe(obs_canvas_t* canvas) {
        RefCountProbe result{false, false};
        if (!canvas) return result;
        
        if (auto* ref = obs_canvas_get_ref(canvas)) {
            result.isAlive = true;
            obs_canvas_release(ref);
        }
        
        if (auto* weak = obs_canvas_get_weak_canvas(canvas)) {
            if (auto* locked = obs_weak_canvas_get_canvas(weak)) {
                result.hasWeakRefs = true;
                obs_canvas_release(locked);
            }
            obs_weak_canvas_release(weak);
        }
        
        return result;
    }
};

/**
 * @brief Log handle state for debugging
 * @param label A label to identify this log entry
 * @param src The source to check
 */
inline void logSourceState(const char* label, obs_source_t* src) {
    if (!src) {
        blog(LOG_DEBUG, "[obs::debug] %s: source is null", label);
        return;
    }
    
    const char* name = obs_source_get_name(src);
    const char* uuid = obs_source_get_uuid(src);
    auto probeResult = RefCountProbe::probe(src);
    
    blog(LOG_DEBUG, "[obs::debug] %s: '%s' (uuid=%s) alive=%d hasWeakRefs=%d",
         label, 
         name ? name : "(null)",
         uuid ? uuid : "(null)",
         probeResult.isAlive,
         probeResult.hasWeakRefs);
}

inline void logCanvasState(const char* label, obs_canvas_t* canvas) {
    if (!canvas) {
        blog(LOG_DEBUG, "[obs::debug] %s: canvas is null", label);
        return;
    }
    
    const char* name = obs_canvas_get_name(canvas);
    const char* uuid = obs_canvas_get_uuid(canvas);
    auto probeResult = RefCountProbe::probe(canvas);
    
    blog(LOG_DEBUG, "[obs::debug] %s: '%s' (uuid=%s) alive=%d hasWeakRefs=%d",
         label,
         name ? name : "(null)",
         uuid ? uuid : "(null)",
         probeResult.isAlive,
         probeResult.hasWeakRefs);
}

} // namespace debug

} // namespace obs
