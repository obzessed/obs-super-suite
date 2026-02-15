// ============================================================================
// Universal Control API — ControlPort Implementation
// ============================================================================

#include "control_port.hpp"

#include <QtMath>

namespace super {

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------
ControlPort::ControlPort(const ControlDescriptor &desc, QObject *parent)
	: QObject(parent)
	, m_desc(desc)
	, m_value(desc.default_value)
{
}

ControlPort::~ControlPort()
{
	stop_animation();
}

// ---------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------
const QString &ControlPort::id() const { return m_desc.id; }
const QString &ControlPort::display_name() const { return m_desc.display_name; }
const QString &ControlPort::group() const { return m_desc.group; }
ControlType ControlPort::type() const { return m_desc.type; }
FeedbackPolicy ControlPort::feedback_policy() const { return m_desc.feedback; }

// ---------------------------------------------------------------------------
// Value Access
// ---------------------------------------------------------------------------
QVariant ControlPort::value() const { return m_value; }

double ControlPort::as_double() const { return m_value.toDouble(); }
bool ControlPort::as_bool() const { return m_value.toBool(); }
int ControlPort::as_int() const { return m_value.toInt(); }
QString ControlPort::as_string() const { return m_value.toString(); }

double ControlPort::normalized_value() const
{
	if (m_desc.type == ControlType::Range)
		return m_value.toDouble();

	// For Float: normalize into [0,1] using range_min/max
	if (m_desc.type == ControlType::Float) {
		double span = m_desc.range_max - m_desc.range_min;
		if (qFuzzyIsNull(span))
			return 0.0;
		return (m_value.toDouble() - m_desc.range_min) / span;
	}

	return m_value.toDouble();
}

// ---------------------------------------------------------------------------
// Value Mutation
// ---------------------------------------------------------------------------
void ControlPort::set_value(const QVariant &val, bool from_hardware)
{
	if (from_hardware)
		emit hardware_input(val);

	apply_filters_and_commit(val, from_hardware);
}

void ControlPort::set_normalized_value(double v)
{
	if (m_desc.type == ControlType::Float) {
		double mapped = m_desc.range_min +
						v * (m_desc.range_max - m_desc.range_min);
		set_value(QVariant(mapped));
	} else {
		set_value(QVariant(qBound(0.0, v, 1.0)));
	}
}

// ---------------------------------------------------------------------------
// Constraints
// ---------------------------------------------------------------------------
double ControlPort::range_min() const { return m_desc.range_min; }
double ControlPort::range_max() const { return m_desc.range_max; }
QVariant ControlPort::default_value() const { return m_desc.default_value; }

void ControlPort::reset_to_default()
{
	set_value(QVariant(m_desc.default_value));
}

// ---------------------------------------------------------------------------
// Filter Pipeline
// ---------------------------------------------------------------------------
void ControlPort::add_filter(std::shared_ptr<ControlFilter> filter)
{
	m_filters.append(std::move(filter));
}

void ControlPort::remove_filter(const std::shared_ptr<ControlFilter> &filter)
{
	m_filters.removeOne(filter);
}

void ControlPort::clear_filters()
{
	m_filters.clear();
}

const QList<std::shared_ptr<ControlFilter>> &ControlPort::filters() const
{
	return m_filters;
}

// ---------------------------------------------------------------------------
// Animation / Easing
// ---------------------------------------------------------------------------
void ControlPort::setup_animation()
{
	if (m_animation)
		return;

	m_animation = new QPropertyAnimation(this, "normalizedValue", this);
	connect(m_animation, &QPropertyAnimation::finished,
			this, &ControlPort::animation_finished);
}

void ControlPort::animate_to(const QVariant &target, int duration_ms,
							  QEasingCurve::Type curve)
{
	setup_animation();
	m_animation->stop();
	m_animation->setDuration(duration_ms);
	m_animation->setEasingCurve(curve);
	m_animation->setStartValue(normalized_value());

	double target_normalized = target.toDouble();
	if (m_desc.type == ControlType::Float) {
		double span = m_desc.range_max - m_desc.range_min;
		if (!qFuzzyIsNull(span))
			target_normalized =
				(target.toDouble() - m_desc.range_min) / span;
	}
	m_animation->setEndValue(target_normalized);
	m_animation->start();
}

void ControlPort::stop_animation()
{
	if (m_animation) {
		m_animation->stop();
	}
}

bool ControlPort::is_animating() const
{
	return m_animation &&
		   m_animation->state() == QAbstractAnimation::Running;
}

// ---------------------------------------------------------------------------
// Soft Takeover
// ---------------------------------------------------------------------------
void ControlPort::set_soft_takeover(bool enabled)
{
	m_soft_takeover = enabled;
	m_takeover_engaged = false;
}

bool ControlPort::soft_takeover() const { return m_soft_takeover; }

// ---------------------------------------------------------------------------
// Descriptor
// ---------------------------------------------------------------------------
const ControlDescriptor &ControlPort::descriptor() const { return m_desc; }

// ---------------------------------------------------------------------------
// Internal: Apply filter pipeline, then commit value.
// ---------------------------------------------------------------------------
void ControlPort::apply_filters_and_commit(const QVariant &raw,
											bool from_hardware)
{
	// Soft-takeover check: ignore hardware input until physical position
	// "catches up" to the current internal value.
	if (from_hardware && m_soft_takeover && !m_takeover_engaged) {
		double hw = raw.toDouble();
		double cur = m_value.toDouble();
		constexpr double kThreshold = 0.02;  // ~2% dead-zone
		if (qAbs(hw - cur) > kThreshold)
			return;  // Ignore until hardware matches
		m_takeover_engaged = true;
	}

	// Run through filter pipeline
	QVariant filtered = raw;
	for (const auto &f : m_filters) {
		filtered = f->process(filtered, *this);
	}

	// Clamp for Range type
	if (m_desc.type == ControlType::Range) {
		double v = qBound(0.0, filtered.toDouble(), 1.0);
		filtered = QVariant(v);
	}

	// Commit
	if (filtered != m_value) {
		m_value = filtered;
		emit value_changed(m_value);
	}
}

} // namespace super
