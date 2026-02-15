#pragma once

// ============================================================================
// Integrated Debugger — Signal Tracing & Breakpoints
//
// Provides:
//   • SignalTrace: Records value changes on watched ports.
//   • PortBreakpoint: Pauses graph evaluation when a condition is met.
//   • DebugMonitor: Singleton that manages traces and breakpoints.
// ============================================================================

#include "../core/control_port.hpp"
#include "../core/control_registry.hpp"

#include <QObject>
#include <QList>
#include <QHash>
#include <QDateTime>
#include <QElapsedTimer>
#include <functional>

namespace super {

// ---------------------------------------------------------------------------
// TraceEntry — One recorded value sample.
// ---------------------------------------------------------------------------
struct TraceEntry {
	qint64 timestamp_ms;	// ms since trace start
	double value;
	QString source;			// "hardware", "script", "graph", etc.
};

// ---------------------------------------------------------------------------
// SignalTrace — Records value history for a single port.
// ---------------------------------------------------------------------------
class SignalTrace : public QObject {
	Q_OBJECT

public:
	explicit SignalTrace(ControlPort *port, int max_entries = 1000,
						  QObject *parent = nullptr)
		: QObject(parent), m_port(port), m_max(max_entries)
	{
		m_timer.start();
		connect(port, &ControlPort::value_changed, this,
			[this](const QVariant &val) {
				TraceEntry e;
				e.timestamp_ms = m_timer.elapsed();
				e.value = val.toDouble();
				m_entries.append(e);
				if (m_entries.size() > m_max)
					m_entries.removeFirst();
				emit entry_added(e);
			});
	}

	const QString &port_id() const { return m_port->id(); }
	const QList<TraceEntry> &entries() const { return m_entries; }
	void clear() { m_entries.clear(); m_timer.restart(); }

	double min_value() const {
		double v = 1e9;
		for (const auto &e : m_entries) v = qMin(v, e.value);
		return m_entries.isEmpty() ? 0.0 : v;
	}

	double max_value() const {
		double v = -1e9;
		for (const auto &e : m_entries) v = qMax(v, e.value);
		return m_entries.isEmpty() ? 0.0 : v;
	}

signals:
	void entry_added(const TraceEntry &entry);

private:
	ControlPort *m_port;
	QElapsedTimer m_timer;
	QList<TraceEntry> m_entries;
	int m_max;
};

// ---------------------------------------------------------------------------
// BreakCondition — When to trigger a breakpoint.
// ---------------------------------------------------------------------------
enum class BreakCondition {
	OnChange,		// Any value change
	OnThreshold,	// Value crosses a threshold
	OnRange			// Value enters/exits a range
};

// ---------------------------------------------------------------------------
// PortBreakpoint — Conditional pause on a port.
// ---------------------------------------------------------------------------
struct PortBreakpoint {
	int id = 0;
	QString port_id;
	BreakCondition condition = BreakCondition::OnChange;
	double threshold = 0.5;
	double range_min = 0.0;
	double range_max = 1.0;
	bool enabled = true;
	std::function<void(double)> callback;
};

// ---------------------------------------------------------------------------
// DebugMonitor — Central debugging hub.
// ---------------------------------------------------------------------------
class DebugMonitor : public QObject {
	Q_OBJECT

public:
	static DebugMonitor &instance() {
		static DebugMonitor s;
		return s;
	}

	// -- Signal Tracing --
	SignalTrace *start_trace(const QString &port_id, int max_entries = 1000) {
		auto *port = ControlRegistry::instance().find(port_id);
		if (!port)
			return nullptr;

		auto *trace = new SignalTrace(port, max_entries, this);
		m_traces.insert(port_id, trace);
		return trace;
	}

	void stop_trace(const QString &port_id) {
		delete m_traces.take(port_id);
	}

	SignalTrace *trace(const QString &port_id) const {
		return m_traces.value(port_id, nullptr);
	}

	QStringList active_traces() const { return m_traces.keys(); }

	// -- Breakpoints --
	int add_breakpoint(const QString &port_id, BreakCondition cond,
					   std::function<void(double)> callback = {}) {
		PortBreakpoint bp;
		bp.id = m_next_bp_id++;
		bp.port_id = port_id;
		bp.condition = cond;
		bp.callback = std::move(callback);
		m_breakpoints.append(bp);

		// Connect to port
		auto *port = ControlRegistry::instance().find(port_id);
		if (port) {
			connect(port, &ControlPort::value_changed, this,
				[this, bpid = bp.id](const QVariant &val) {
					check_breakpoint(bpid, val.toDouble());
				});
		}

		return bp.id;
	}

	void remove_breakpoint(int id) {
		m_breakpoints.erase(
			std::remove_if(m_breakpoints.begin(), m_breakpoints.end(),
				[id](const PortBreakpoint &bp) { return bp.id == id; }),
			m_breakpoints.end());
	}

	void set_breakpoint_enabled(int id, bool enabled) {
		for (auto &bp : m_breakpoints) {
			if (bp.id == id) {
				bp.enabled = enabled;
				return;
			}
		}
	}

signals:
	void breakpoint_hit(int breakpoint_id, double value);

private:
	DebugMonitor() : QObject(nullptr) {}

	void check_breakpoint(int id, double value) {
		for (const auto &bp : m_breakpoints) {
			if (bp.id != id || !bp.enabled)
				continue;

			bool hit = false;
			switch (bp.condition) {
			case BreakCondition::OnChange:
				hit = true;
				break;
			case BreakCondition::OnThreshold:
				hit = (value >= bp.threshold);
				break;
			case BreakCondition::OnRange:
				hit = (value >= bp.range_min && value <= bp.range_max);
				break;
			}

			if (hit) {
				if (bp.callback)
					bp.callback(value);
				emit breakpoint_hit(bp.id, value);
			}
		}
	}

	QHash<QString, SignalTrace *> m_traces;
	QList<PortBreakpoint> m_breakpoints;
	int m_next_bp_id = 1;
};

} // namespace super
