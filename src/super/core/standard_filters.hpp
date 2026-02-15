#pragma once

// ============================================================================
// Universal Control API — Standard Filters
// Built-in ControlFilter implementations for common signal processing.
// ============================================================================

#include "control_port.hpp"

#include <QtMath>
#include <algorithm>

namespace super {

// ---------------------------------------------------------------------------
// InvertFilter — Flips 0..1 to 1..0
// ---------------------------------------------------------------------------
class InvertFilter : public ControlFilter {
public:
	QVariant process(const QVariant &input,
					  const ControlPort & /*port*/) const override
	{
		return QVariant(1.0 - input.toDouble());
	}
	QString name() const override { return QStringLiteral("Invert"); }
};

// ---------------------------------------------------------------------------
// ScaleFilter — Multiplies by a constant factor.
// ---------------------------------------------------------------------------
class ScaleFilter : public ControlFilter {
public:
	explicit ScaleFilter(double factor) : m_factor(factor) {}

	QVariant process(const QVariant &input,
					  const ControlPort & /*port*/) const override
	{
		return QVariant(input.toDouble() * m_factor);
	}
	QString name() const override
	{
		return QStringLiteral("Scale(%1)").arg(m_factor);
	}

private:
	double m_factor;
};

// ---------------------------------------------------------------------------
// ClampFilter — Restricts value to [min, max].
// ---------------------------------------------------------------------------
class ClampFilter : public ControlFilter {
public:
	ClampFilter(double min_val, double max_val)
		: m_min(min_val), m_max(max_val) {}

	QVariant process(const QVariant &input,
					  const ControlPort & /*port*/) const override
	{
		return QVariant(qBound(m_min, input.toDouble(), m_max));
	}
	QString name() const override
	{
		return QStringLiteral("Clamp(%1,%2)").arg(m_min).arg(m_max);
	}

private:
	double m_min, m_max;
};

// ---------------------------------------------------------------------------
// ThresholdFilter — Outputs 0 or 1 based on a threshold.
// ---------------------------------------------------------------------------
class ThresholdFilter : public ControlFilter {
public:
	explicit ThresholdFilter(double threshold = 0.5)
		: m_threshold(threshold) {}

	QVariant process(const QVariant &input,
					  const ControlPort & /*port*/) const override
	{
		return QVariant(input.toDouble() >= m_threshold ? 1.0 : 0.0);
	}
	QString name() const override
	{
		return QStringLiteral("Threshold(%1)").arg(m_threshold);
	}

private:
	double m_threshold;
};

// ---------------------------------------------------------------------------
// SmoothFilter — Exponential moving average for noise reduction.
// alpha 0..1: 0 = no smoothing, 1 = maximum smoothing.
// ---------------------------------------------------------------------------
class SmoothFilter : public ControlFilter {
public:
	explicit SmoothFilter(double alpha = 0.8)
		: m_alpha(qBound(0.0, alpha, 1.0)) {}

	QVariant process(const QVariant &input,
					  const ControlPort & /*port*/) const override
	{
		double v = input.toDouble();
		m_prev = m_alpha * m_prev + (1.0 - m_alpha) * v;
		return QVariant(m_prev);
	}
	QString name() const override
	{
		return QStringLiteral("Smooth(%1)").arg(m_alpha);
	}

private:
	double m_alpha;
	mutable double m_prev = 0.0;
};

// ---------------------------------------------------------------------------
// DeadZoneFilter — Ignores values below a threshold (noise gate).
// ---------------------------------------------------------------------------
class DeadZoneFilter : public ControlFilter {
public:
	explicit DeadZoneFilter(double zone = 0.05)
		: m_zone(zone) {}

	QVariant process(const QVariant &input,
					  const ControlPort & /*port*/) const override
	{
		double v = input.toDouble();
		return QVariant(v < m_zone ? 0.0 : v);
	}
	QString name() const override
	{
		return QStringLiteral("DeadZone(%1)").arg(m_zone);
	}

private:
	double m_zone;
};

// ---------------------------------------------------------------------------
// MapRangeFilter — Remaps from [in_min, in_max] to [out_min, out_max].
// ---------------------------------------------------------------------------
class MapRangeFilter : public ControlFilter {
public:
	MapRangeFilter(double in_min, double in_max,
				   double out_min, double out_max)
		: m_in_min(in_min), m_in_max(in_max)
		, m_out_min(out_min), m_out_max(out_max) {}

	QVariant process(const QVariant &input,
					  const ControlPort & /*port*/) const override
	{
		double v = input.toDouble();
		double in_span = m_in_max - m_in_min;
		if (qFuzzyIsNull(in_span))
			return QVariant(m_out_min);
		double normalized = (v - m_in_min) / in_span;
		return QVariant(m_out_min + normalized * (m_out_max - m_out_min));
	}
	QString name() const override
	{
		return QStringLiteral("MapRange(%1-%2 -> %3-%4)")
			.arg(m_in_min).arg(m_in_max)
			.arg(m_out_min).arg(m_out_max);
	}

private:
	double m_in_min, m_in_max, m_out_min, m_out_max;
};

} // namespace super
