// ============================================================================
// Universal Control API — ControlRegistry Implementation
// ============================================================================

#include "control_registry.hpp"
#include "control_variable.hpp"

#include <QJsonArray>

namespace super {

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
ControlRegistry &ControlRegistry::instance()
{
	static ControlRegistry s_instance;
	return s_instance;
}

ControlRegistry::ControlRegistry() : QObject(nullptr) {}
ControlRegistry::~ControlRegistry()
{
	qDeleteAll(m_ports);
	m_ports.clear();
	m_variables.clear(); // Variables are also in m_ports, already deleted
}

// ---------------------------------------------------------------------------
// Port Lifecycle
// ---------------------------------------------------------------------------
ControlPort *ControlRegistry::create_port(const ControlDescriptor &desc)
{
	if (m_ports.contains(desc.id))
		return m_ports.value(desc.id);

	auto *port = new ControlPort(desc, this);
	m_ports.insert(desc.id, port);
	emit port_added(desc.id);
	return port;
}

void ControlRegistry::destroy_port(const QString &id)
{
	auto *port = m_ports.take(id);
	if (!port)
		return;

	// Also remove from variables if it was one
	m_variables.remove(id);

	emit port_removed(id);
	port->deleteLater();
}

bool ControlRegistry::has_port(const QString &id) const
{
	return m_ports.contains(id);
}

// ---------------------------------------------------------------------------
// Lookup
// ---------------------------------------------------------------------------
ControlPort *ControlRegistry::find(const QString &id) const
{
	return m_ports.value(id, nullptr);
}

QList<ControlPort *> ControlRegistry::find_by_group(const QString &group) const
{
	QList<ControlPort *> result;
	const QString prefix = group + QStringLiteral(".");
	for (auto it = m_ports.cbegin(); it != m_ports.cend(); ++it) {
		if (it.key().startsWith(prefix) || it.key() == group)
			result.append(it.value());
	}
	return result;
}

QList<ControlPort *> ControlRegistry::all_ports() const
{
	return m_ports.values();
}

QStringList ControlRegistry::all_ids() const
{
	return m_ports.keys();
}

// ---------------------------------------------------------------------------
// Variable Management
// ---------------------------------------------------------------------------
ControlVariable *ControlRegistry::create_variable(const QString &id,
												   ControlType type,
												   PersistencePolicy policy)
{
	if (m_variables.contains(id))
		return m_variables.value(id);

	ControlDescriptor desc;
	desc.id = id;
	desc.display_name = id;
	desc.type = type;

	auto *var = new ControlVariable(desc, policy, this);
	m_ports.insert(id, var);
	m_variables.insert(id, var);
	emit port_added(id);
	return var;
}

ControlVariable *ControlRegistry::find_variable(const QString &id) const
{
	return m_variables.value(id, nullptr);
}

QList<ControlVariable *> ControlRegistry::all_variables() const
{
	return m_variables.values();
}

// ---------------------------------------------------------------------------
// Snapshots
// ---------------------------------------------------------------------------
QJsonObject ControlRegistry::capture_snapshot() const
{
	QJsonObject snap;
	for (auto it = m_ports.cbegin(); it != m_ports.cend(); ++it) {
		const auto &port = it.value();
		// Store as double for simplicity; extend for other types later
		snap.insert(it.key(), port->as_double());
	}
	return snap;
}

void ControlRegistry::restore_snapshot(const QJsonObject &snapshot)
{
	for (auto it = snapshot.constBegin(); it != snapshot.constEnd(); ++it) {
		if (auto *port = find(it.key())) {
			port->set_value(it.value().toVariant());
		}
	}
	emit snapshot_restored();
}

// ---------------------------------------------------------------------------
// Modifiers
// ---------------------------------------------------------------------------
void ControlRegistry::set_modifier(const QString &mod_id, bool active)
{
	if (m_modifiers.value(mod_id) == active)
		return;
	m_modifiers.insert(mod_id, active);
	emit modifier_changed(mod_id, active);
}

bool ControlRegistry::modifier(const QString &mod_id) const
{
	return m_modifiers.value(mod_id, false);
}

QStringList ControlRegistry::active_modifiers() const
{
	QStringList result;
	for (auto it = m_modifiers.cbegin(); it != m_modifiers.cend(); ++it) {
		if (it.value())
			result.append(it.key());
	}
	return result;
}

// ---------------------------------------------------------------------------
// Variable Persistence
// ---------------------------------------------------------------------------
QJsonObject ControlRegistry::save_variables() const
{
	QJsonObject data;
	for (auto it = m_variables.cbegin(); it != m_variables.cend(); ++it) {
		auto *var = it.value();
		if (var->persistence_policy() == PersistencePolicy::Persist) {
			data.insert(it.key(), var->as_double());
		}
	}
	return data;
}

void ControlRegistry::load_variables(const QJsonObject &data)
{
	for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
		if (auto *var = find_variable(it.key())) {
			var->set_value(it.value().toVariant());
		}
	}
}

} // namespace super
