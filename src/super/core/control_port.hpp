#pragma once

// ============================================================================
// Universal Control API — ControlPort
// The atomic unit of the control system. Every controllable parameter
// in the entire plugin is represented by one ControlPort instance.
// ============================================================================

#include "control_types.hpp"

#include <QObject>
#include <QVariant>
#include <QEasingCurve>
#include <QPropertyAnimation>
#include <QList>
#include <memory>

namespace super {

class ControlFilter;

// ---------------------------------------------------------------------------
// ControlPort — A single named, typed, observable value.
//
// Features:
//   • Typed value with change notification (Qt signal).
//   • Filter pipeline (chain of ControlFilter processors).
//   • Animation / easing (animate_to with QEasingCurve).
//   • Modifier layer bindings (Shift/Alt alternate targets).
//   • Soft-takeover support for hardware controllers.
// ---------------------------------------------------------------------------
class ControlPort : public QObject {
	Q_OBJECT
	Q_PROPERTY(double normalizedValue READ normalized_value
			   WRITE set_normalized_value NOTIFY value_changed)

public:
	explicit ControlPort(const ControlDescriptor &desc, QObject *parent = nullptr);
	~ControlPort() override;

	// -- Identity ----------------------------------------------------------
	const QString &id() const;
	const QString &display_name() const;
	const QString &group() const;
	ControlType type() const;
	FeedbackPolicy feedback_policy() const;

	// -- Value Access ------------------------------------------------------
	QVariant value() const;
	double as_double() const;
	bool as_bool() const;
	int as_int() const;
	QString as_string() const;

	// Normalized 0..1 representation (for Range type; identity for others)
	double normalized_value() const;

	// -- Value Mutation ----------------------------------------------------
	// Set the current value. `from_hardware` indicates the source is an
	// external controller (enables soft-takeover logic).
	void set_value(const QVariant &val, bool from_hardware = false);
	void set_normalized_value(double v);

	// -- Constraints -------------------------------------------------------
	double range_min() const;
	double range_max() const;
	QVariant default_value() const;
	void reset_to_default();

	// -- Filter Pipeline ---------------------------------------------------
	void add_filter(std::shared_ptr<ControlFilter> filter);
	void remove_filter(const std::shared_ptr<ControlFilter> &filter);
	void clear_filters();
	const QList<std::shared_ptr<ControlFilter>> &filters() const;

	// -- Animation / Easing ------------------------------------------------
	void animate_to(const QVariant &target, int duration_ms,
					QEasingCurve::Type curve = QEasingCurve::Linear);
	void stop_animation();
	bool is_animating() const;

	// -- Soft Takeover -----------------------------------------------------
	void set_soft_takeover(bool enabled);
	bool soft_takeover() const;

	// -- Metadata ----------------------------------------------------------
	const ControlDescriptor &descriptor() const;

signals:
	// Emitted after filters have been applied and the value is committed.
	void value_changed(const QVariant &new_value);
	// Emitted when a hardware source sets this port (before filters).
	void hardware_input(const QVariant &raw_value);
	// Emitted after animation completes.
	void animation_finished();

private:
	void apply_filters_and_commit(const QVariant &raw, bool from_hardware);
	void setup_animation();

	ControlDescriptor m_desc;
	QVariant m_value;
	bool m_soft_takeover = false;
	bool m_takeover_engaged = false;  // True once hardware "caught up"

	QList<std::shared_ptr<ControlFilter>> m_filters;

	// Animation (lazy-init)
	QPropertyAnimation *m_animation = nullptr;
};

// ---------------------------------------------------------------------------
// ControlFilter — Abstract base for signal processors in the pipeline.
// ---------------------------------------------------------------------------
class ControlFilter {
public:
	virtual ~ControlFilter() = default;

	// Transform a value. Return the modified value.
	virtual QVariant process(const QVariant &input,
							 const ControlPort &port) const = 0;

	// Human-readable name for debug / UI.
	virtual QString name() const = 0;
};

} // namespace super
