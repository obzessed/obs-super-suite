#pragma once

// ============================================================================
// Undo/Redo System — Transaction-based history for ControlPorts.
//
// Uses Qt's QUndoStack with a custom QUndoCommand that captures
// ControlPort value changes with coalescing support.
// ============================================================================

#include "control_port.hpp"
#include "control_registry.hpp"

#include <QObject>
#include <QUndoStack>
#include <QUndoCommand>
#include <QVariant>
#include <QHash>
#include <QJsonObject>

namespace super {

// ---------------------------------------------------------------------------
// PortChangeCommand — A single port value change, undoable.
// Supports mergeWith() for coalescing rapid changes (e.g. fader drags).
// ---------------------------------------------------------------------------
class PortChangeCommand : public QUndoCommand {
public:
	PortChangeCommand(const QString &port_id,
					   const QVariant &old_val,
					   const QVariant &new_val,
					   QUndoCommand *parent = nullptr)
		: QUndoCommand(parent)
		, m_port_id(port_id)
		, m_old_val(old_val)
		, m_new_val(new_val)
		, m_coalesce_id(qHash(port_id))
	{
		setText("Change " + port_id);
	}

	void undo() override {
		if (auto *port = ControlRegistry::instance().find(m_port_id))
			port->set_value(m_old_val);
	}

	void redo() override {
		if (auto *port = ControlRegistry::instance().find(m_port_id))
			port->set_value(m_new_val);
	}

	int id() const override { return static_cast<int>(m_coalesce_id); }

	bool mergeWith(const QUndoCommand *other) override {
		auto *cmd = dynamic_cast<const PortChangeCommand *>(other);
		if (!cmd || cmd->m_port_id != m_port_id)
			return false;
		m_new_val = cmd->m_new_val;
		return true;
	}

private:
	QString m_port_id;
	QVariant m_old_val;
	QVariant m_new_val;
	uint m_coalesce_id;
};

// ---------------------------------------------------------------------------
// SnapshotCommand — Captures/restores the entire registry state.
// ---------------------------------------------------------------------------
class SnapshotCommand : public QUndoCommand {
public:
	SnapshotCommand(const QJsonObject &before, const QJsonObject &after,
					 QUndoCommand *parent = nullptr)
		: QUndoCommand("Snapshot", parent)
		, m_before(before), m_after(after)
	{
	}

	void undo() override {
		ControlRegistry::instance().restore_snapshot(m_before);
	}

	void redo() override {
		ControlRegistry::instance().restore_snapshot(m_after);
	}

private:
	QJsonObject m_before, m_after;
};

// ---------------------------------------------------------------------------
// UndoManager — Singleton wrapper around QUndoStack.
// ---------------------------------------------------------------------------
class UndoManager : public QObject {
	Q_OBJECT

public:
	static UndoManager &instance() {
		static UndoManager s;
		return s;
	}

	QUndoStack *stack() { return &m_stack; }

	// Record a port value change
	void record(const QString &port_id,
				const QVariant &old_val, const QVariant &new_val) {
		m_stack.push(new PortChangeCommand(port_id, old_val, new_val));
	}

	// Record a full snapshot change
	void record_snapshot(const QJsonObject &before,
						  const QJsonObject &after) {
		m_stack.push(new SnapshotCommand(before, after));
	}

	void undo() { m_stack.undo(); }
	void redo() { m_stack.redo(); }
	bool can_undo() const { return m_stack.canUndo(); }
	bool can_redo() const { return m_stack.canRedo(); }
	void clear() { m_stack.clear(); }

	int undo_limit() const { return m_stack.undoLimit(); }
	void set_undo_limit(int limit) { m_stack.setUndoLimit(limit); }

private:
	UndoManager() : QObject(nullptr) {
		m_stack.setUndoLimit(200);
	}

	QUndoStack m_stack;
};

} // namespace super
