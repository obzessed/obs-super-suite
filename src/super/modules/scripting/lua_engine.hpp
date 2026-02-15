#pragma once

// ============================================================================
// Lua Scripting Engine — Bindings for the Control API.
//
// Embeds Lua 5.4 and exposes:
//   • ControlRegistry port read/write
//   • Variable management
//   • Modifier state queries
//   • Logging
//
// NOTE: Lua 5.4 is NOT yet linked. This header defines the interface;
//       the implementation stub compiles without Lua and will be activated
//       once the Lua library is added to CMake.
// ============================================================================

#include <QObject>
#include <QString>
#include <QStringList>

namespace super {

// ---------------------------------------------------------------------------
// LuaEngine — Script execution environment.
// ---------------------------------------------------------------------------
class LuaEngine : public QObject {
	Q_OBJECT

public:
	explicit LuaEngine(QObject *parent = nullptr);
	~LuaEngine() override;

	// -- Script Execution --
	// Run a Lua string. Returns true on success.
	bool run(const QString &script);

	// Load and run a Lua file. Returns true on success.
	bool run_file(const QString &path);

	// -- Error Handling --
	QString last_error() const;

	// -- Status --
	bool is_initialized() const;

signals:
	void script_error(const QString &error);
	void log_message(const QString &message);

private:
	void init();
	void register_api();
	void shutdown();

	struct Impl;
	Impl *m_impl = nullptr;
};

} // namespace super
