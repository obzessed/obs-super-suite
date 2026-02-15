#pragma once

// ============================================================================
// Universal Control API — ControlVariable
// A specialized ControlPort that represents user-defined persistent data.
// ============================================================================

#include "control_port.hpp"
#include "control_types.hpp"

namespace super {

// ---------------------------------------------------------------------------
// ControlVariable — A port with persistence policy.
//
// Variables can be:
//   • Session-scoped (lost on restart)
//   • Persistent (saved to disk as JSON)
//
// Used for counters, state machines, user preferences, etc.
// ---------------------------------------------------------------------------
class ControlVariable : public ControlPort {
	Q_OBJECT

public:
	explicit ControlVariable(const ControlDescriptor &desc,
							  PersistencePolicy policy,
							  QObject *parent = nullptr)
		: ControlPort(desc, parent)
		, m_policy(policy)
	{
	}

	PersistencePolicy persistence_policy() const { return m_policy; }
	void set_persistence_policy(PersistencePolicy p) { m_policy = p; }

private:
	PersistencePolicy m_policy;
};

} // namespace super
