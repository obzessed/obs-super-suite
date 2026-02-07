#pragma once

#include "plugin-support.h"

#include <obs.h>

#include <typeinfo>
#include <optional>
#include <utility>
#include <type_traits>

/**
 * @file handle.hxx
 * @brief V8-style Handle pattern for OBS API reference counting
 * 
 * Provides automatic memory management for OBS objects:
 * - Local<T>	: Move-only owning handle (auto-releases on destruction)
 * - Ref<T>	: Copyable shared ownership (calls get_ref on copy)
 * - WeakRef<T> : Weak reference with lock() -> std::optional<Local<T>>
 */

namespace obs {

namespace detail {
template<typename T>
class OBSObject;
}

// Forward declarations
template<typename T> class Local;
template<typename T> class Ref;
template<typename T> class WeakRef;

//=============================================================================
// HandleTraits - Specialize for each OBS type
//=============================================================================

/**
 * @brief Traits template for OBS types.
 * 
 * Specializations must provide:
 *   - RawType       : The underlying obs_*_t pointer type
 *   - WeakType      : The obs_weak_*_t type (or void if no weak refs)
 *   - release(ptr)  : Release a reference
 *   - get_ref(ptr)  : Increment and return reference (may return null)
 *   - get_weak(ptr) : Get weak reference from strong
 *   - from_weak(w)  : Get strong reference from weak (may return null)
 *   - release_weak(w) : Release weak reference
 */
template<typename T>
struct HandleTraits {
    // Default: not specialized - will cause compile error if used
    static_assert(sizeof(T) == 0, "HandleTraits must be specialized for this type");
};

//=============================================================================
// Local<T> - Move-only owning handle
//=============================================================================

/**
 * @brief Move-only owning handle that releases on destruction.
 * 
 * Similar to std::unique_ptr but uses OBS reference counting.
 * 
 * @example
 *   auto source = Source::findByName("MySource");
 *   if (source) {
 *       // Use source.value()
 *   } // Automatically released here
 */
template<typename T>
class Local {
public:
    using RawType = HandleTraits<T>::RawType;

private:
    RawType* ptr_ = nullptr;

public:
    // Constructors
    Local() noexcept
    {
	    obs_log(LOG_INFO, "%s (%p)", __FUNCSIG__, this);
    }
    explicit Local(RawType* ptr, bool addref = false)
    {
    	ptr_ = addref ? HandleTraits<T>::get_ref(ptr) : ptr;
    	if (!ptr_) {
    		obs_log(LOG_ERROR, "null ref ptr:" "%s (%p)[%p][%s]", __FUNCSIG__, this, ptr_, addref ? "+" : "-");
    	} else {
    		obs_log(LOG_INFO, "%s (%p)[%p][%s]", __FUNCSIG__, this, ptr_, addref ? "+" : "-");
    	}
    }

    // Move semantics
    Local(Local&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    
    Local& operator=(Local&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    
    // No copy
    Local(const Local&) = delete;
    Local& operator=(const Local&) = delete;
    
    // Destructor - releases the reference
    ~Local() {
	obs_log(LOG_INFO, "%s (%p)[%p]", __FUNCSIG__, this, ptr_);
        reset();
    }
    
    // Reset/release
    void reset() noexcept {
        if (ptr_) {
            HandleTraits<T>::release(ptr_);
            ptr_ = nullptr;
        }
    }
    
    // Release ownership without calling release
    [[nodiscard]] RawType* release() noexcept {
	obs_log(LOG_INFO, "%s (%p)[%p]", __FUNCSIG__, this, ptr_);
        RawType* tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }
    
    // Access
    RawType* raw() const noexcept { return ptr_; }
    RawType* operator->() const noexcept { return ptr_; }
    RawType& operator*() const noexcept { return *ptr_; }
    
    // Boolean conversion
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    
    // Create a copy by incrementing ref count (returns new Local)
    [[nodiscard]] Local clone() const {
	obs_log(LOG_INFO, "%s (%p)[%p]", __FUNCSIG__, this, ptr_);
        if (ptr_) {
            return Local(HandleTraits<T>::get_ref(ptr_));
        }
        return Local();
    }
    
    // Get weak reference
    [[nodiscard]] WeakRef<T> weak() const;
    
    // Convert to Ref (shared ownership)
    [[nodiscard]] Ref<T> share() const;
};

//=============================================================================
// Ref<T> - Copyable shared ownership
//=============================================================================

/**
 * @brief Copyable shared ownership handle.
 * 
 * Like Local<T> but supports copying by calling get_ref().
 * Use when you need to store handles that may be copied.
 */
template<typename T>
class Ref {
public:
    using RawType = HandleTraits<T>::RawType;

private:
    RawType* ptr_ = nullptr;

public:
    // Constructors
    Ref() noexcept = default;
    explicit Ref(RawType* ptr) noexcept : ptr_(ptr) {}
    
    // Copy - increments ref count
    Ref(const Ref& other) : ptr_(nullptr) {
        if (other.ptr_) {
            ptr_ = HandleTraits<T>::get_ref(other.ptr_);
        }
    }
    
    Ref& operator=(const Ref& other) {
        if (this != &other) {
            reset();
            if (other.ptr_) {
                ptr_ = HandleTraits<T>::get_ref(other.ptr_);
            }
        }
        return *this;
    }
    
    // Move - transfers ownership
    Ref(Ref&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    
    Ref& operator=(Ref&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    
    // From Local - takes ownership
    Ref(Local<T>&& local) noexcept : ptr_(local.release()) {}
    
    // Destructor
    ~Ref() {
        reset();
    }
    
    // Reset
    void reset() noexcept {
        if (ptr_) {
            HandleTraits<T>::release(ptr_);
            ptr_ = nullptr;
        }
    }
    
    // Access
    RawType* raw() const noexcept { return ptr_; }
    RawType* operator->() const noexcept { return ptr_; }
    RawType& operator*() const noexcept { return *ptr_; }
    
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    
    // Get weak reference
    [[nodiscard]] WeakRef<T> weak() const;
};

//=============================================================================
// WeakRef<T> - Weak reference
//=============================================================================

/**
 * @brief Weak reference that doesn't extend object lifetime.
 * 
 * Use lock() to get an owning handle if the object is still alive.
 * 
 * @example
 *   WeakRef<Source> weak = source.weak();
 *   // ... later ...
 *   if (auto locked = weak.lock()) {
 *       // Source still alive
 *   }
 */
template<typename T>
class WeakRef {
public:
    using WeakType = HandleTraits<T>::WeakType;

private:
    WeakType* ptr_ = nullptr;

public:
    // Constructors
    WeakRef() noexcept = default;
    explicit WeakRef(WeakType* ptr) noexcept : ptr_(ptr) {}
    
    // Copy - increments weak ref count
    WeakRef(const WeakRef& other) : ptr_(nullptr) {
        if (other.ptr_) {
            HandleTraits<T>::addref_weak(other.ptr_);
            ptr_ = other.ptr_;
        }
    }
    
    WeakRef& operator=(const WeakRef& other) {
        if (this != &other) {
            reset();
            if (other.ptr_) {
                HandleTraits<T>::addref_weak(other.ptr_);
                ptr_ = other.ptr_;
            }
        }
        return *this;
    }
    
    // Move
    WeakRef(WeakRef&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    
    WeakRef& operator=(WeakRef&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    
    // Destructor
    ~WeakRef() {
        reset();
    }
    
    // Reset
    void reset() noexcept {
        if (ptr_) {
            HandleTraits<T>::release_weak(ptr_);
            ptr_ = nullptr;
        }
    }
    
    // Try to get owning handle
    [[nodiscard]] std::optional<Local<T>> lock() const {
        if (ptr_) {
	    if (auto *strong = HandleTraits<T>::from_weak(ptr_)) {
                return Local<T>(strong);
            }
        }
        return std::nullopt;
    }
    
    // Check if expired (without acquiring reference)
    [[nodiscard]] bool expired() const noexcept {
        if (!ptr_) return true;
        // Try to lock and immediately release to check
	if (auto *strong = HandleTraits<T>::from_weak(ptr_)) {
            HandleTraits<T>::release(strong);
            return false;
        }
        return true;
    }
    
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    
    WeakType* raw() const noexcept { return ptr_; }
};

//=============================================================================
// Deferred implementations (need complete types)
//=============================================================================

template<typename T>
WeakRef<T> Local<T>::weak() const {
    if (ptr_) {
        return WeakRef<T>(HandleTraits<T>::get_weak(ptr_));
    }
    return WeakRef<T>();
}

template<typename T>
Ref<T> Local<T>::share() const {
    if (ptr_) {
        return Ref<T>(HandleTraits<T>::get_ref(ptr_));
    }
    return Ref<T>();
}

template<typename T>
WeakRef<T> Ref<T>::weak() const {
    if (ptr_) {
        return WeakRef<T>(HandleTraits<T>::get_weak(ptr_));
    }
    return WeakRef<T>();
}

} // namespace obs
