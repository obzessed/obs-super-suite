// ============================================================================
// Lua Scripting Engine — Stub Implementation
//
// This compiles without Lua linked. Once lua54 is added to CMake,
// uncomment the #include and replace the stubs with real calls.
// ============================================================================

#include "lua_engine.hpp"

// #include <lua.hpp>   // Uncomment when Lua is linked

namespace super {

// ---------------------------------------------------------------------------
// PIMPL (holds lua_State* when Lua is linked)
// ---------------------------------------------------------------------------
struct LuaEngine::Impl {
	// lua_State *L = nullptr;   // Uncomment when Lua is linked
	QString last_error;
	bool initialized = false;
};

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

LuaEngine::LuaEngine(QObject *parent)
	: QObject(parent), m_impl(new Impl)
{
	init();
}

LuaEngine::~LuaEngine()
{
	shutdown();
	delete m_impl;
}

// ---------------------------------------------------------------------------
// Initialization (stubbed)
// ---------------------------------------------------------------------------

void LuaEngine::init()
{
	// When Lua is linked:
	// m_impl->L = luaL_newstate();
	// luaL_openlibs(m_impl->L);
	// register_api();
	// m_impl->initialized = true;

	m_impl->initialized = false;  // Stub: not yet functional
}

void LuaEngine::register_api()
{
	// This will expose C functions to Lua:
	//   super.get(port_id)          -> double
	//   super.set(port_id, value)   -> void
	//   super.var(var_id)           -> double
	//   super.set_var(var_id, val)  -> void
	//   super.modifier(mod_id)     -> bool
	//   super.log(message)         -> void
}

void LuaEngine::shutdown()
{
	// if (m_impl->L) {
	//     lua_close(m_impl->L);
	//     m_impl->L = nullptr;
	// }
	m_impl->initialized = false;
}

// ---------------------------------------------------------------------------
// Script Execution (stubbed)
// ---------------------------------------------------------------------------

bool LuaEngine::run(const QString &script)
{
	Q_UNUSED(script);

	if (!m_impl->initialized) {
		m_impl->last_error = "Lua engine not initialized (library not linked)";
		emit script_error(m_impl->last_error);
		return false;
	}

	// int status = luaL_dostring(m_impl->L, script.toUtf8().constData());
	// if (status != LUA_OK) {
	//     m_impl->last_error = lua_tostring(m_impl->L, -1);
	//     lua_pop(m_impl->L, 1);
	//     emit script_error(m_impl->last_error);
	//     return false;
	// }
	return true;
}

bool LuaEngine::run_file(const QString &path)
{
	Q_UNUSED(path);

	if (!m_impl->initialized) {
		m_impl->last_error = "Lua engine not initialized (library not linked)";
		emit script_error(m_impl->last_error);
		return false;
	}

	// int status = luaL_dofile(m_impl->L, path.toUtf8().constData());
	// if (status != LUA_OK) {
	//     m_impl->last_error = lua_tostring(m_impl->L, -1);
	//     lua_pop(m_impl->L, 1);
	//     emit script_error(m_impl->last_error);
	//     return false;
	// }
	return true;
}

// ---------------------------------------------------------------------------
// Error Reporting
// ---------------------------------------------------------------------------

QString LuaEngine::last_error() const
{
	return m_impl->last_error;
}

bool LuaEngine::is_initialized() const
{
	return m_impl->initialized;
}

} // namespace super
