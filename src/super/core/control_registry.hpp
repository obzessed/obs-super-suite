#pragma once

// ============================================================================
// Universal Control API — ControlRegistry
// The central nervous system: a singleton database of all ports and variables.
// ============================================================================

#include "control_port.hpp"
#include "control_types.hpp"

#include <QObject>
#include <QHash>
#include <QList>
#include <QJsonObject>

namespace super {

class ControlVariable;

// ---------------------------------------------------------------------------
// ControlRegistry — Singleton that owns and manages all ControlPorts.
//
// Features:
//   • Create / destroy ports by descriptor.
//   • Hierarchical ID lookup  ("audio.mic.vol").
//   • Group enumeration       ("audio.mic.*").
//   • Global snapshot / restore.
//   • Modifier state tracking (Shift / Alt layers).
// ---------------------------------------------------------------------------
class ControlRegistry : public QObject {
	Q_OBJECT

public:
	// Singleton access
	static ControlRegistry &instance();

	// -- Port Lifecycle ----------------------------------------------------
	ControlPort *create_port(const ControlDescriptor &desc);
	void destroy_port(const QString &id);
	bool has_port(const QString &id) const;

	// -- Lookup ------------------------------------------------------------
	ControlPort *find(const QString &id) const;
	QList<ControlPort *> find_by_group(const QString &group) const;
	QList<ControlPort *> all_ports() const;
	QStringList all_ids() const;

	// -- Variable Management -----------------------------------------------
	ControlVariable *create_variable(const QString &id, ControlType type,
									  PersistencePolicy policy = PersistencePolicy::Session);
	ControlVariable *find_variable(const QString &id) const;
	QList<ControlVariable *> all_variables() const;

	// -- Snapshots ---------------------------------------------------------
	QJsonObject capture_snapshot() const;
	void restore_snapshot(const QJsonObject &snapshot);

	// -- Modifiers (Global Layers) -----------------------------------------
	void set_modifier(const QString &mod_id, bool active);
	bool modifier(const QString &mod_id) const;
	QStringList active_modifiers() const;

	// -- Persistence -------------------------------------------------------
	// Save/load all Persist-policy variables to/from JSON.
	QJsonObject save_variables() const;
	void load_variables(const QJsonObject &data);

signals:
	void port_added(const QString &id);
	void port_removed(const QString &id);
	void modifier_changed(const QString &mod_id, bool active);
	void snapshot_restored();

private:
	ControlRegistry();
	~ControlRegistry() override;
	Q_DISABLE_COPY_MOVE(ControlRegistry)

	QHash<QString, ControlPort *> m_ports;
	QHash<QString, ControlVariable *> m_variables;
	QHash<QString, bool> m_modifiers;
};

} // namespace super
