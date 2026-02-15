#pragma once

// ============================================================================
// Concrete ControlFilter implementations for the filter pipeline.
//
// These filters process values before they are committed to a ControlPort.
// Chain them via ControlPort::add_filter() to build a custom signal path.
// ============================================================================

#include "control_port.hpp"

#include <QElapsedTimer>
#include <cmath>

namespace super {

// ---------------------------------------------------------------------------
// SmoothingFilter — Low-pass / exponential smoothing.
// Reduces jitter from noisy hardware controllers.
// ---------------------------------------------------------------------------
class SmoothingFilter : public ControlFilter {
public:
	explicit SmoothingFilter(double factor = 0.3)
		: m_factor(qBound(0.01, factor, 1.0))
		, m_last(0.0)
		, m_initialized(false) {}

	QVariant process(const QVariant &input,
					 const ControlPort &/*port*/) const override
	{
		double val = input.toDouble();
		if (!m_initialized) {
			m_last = val;
			m_initialized = true;
			return input;
		}
		m_last = m_last + m_factor * (val - m_last);
		return QVariant(m_last);
	}

	QString name() const override { return "Smoothing"; }

	void set_factor(double f) { m_factor = qBound(0.01, f, 1.0); }
	double factor() const { return m_factor; }

private:
	double m_factor;
	mutable double m_last;
	mutable bool m_initialized;
};

// ---------------------------------------------------------------------------
// DeadzoneFilter — Ignores small input changes around the current value.
// Useful for potentiometers or faders with mechanical noise.
// ---------------------------------------------------------------------------
class DeadzoneFilter : public ControlFilter {
public:
	explicit DeadzoneFilter(double zone = 0.02)
		: m_zone(zone)
		, m_last(0.0)
		, m_initialized(false) {}

	QVariant process(const QVariant &input,
					 const ControlPort &/*port*/) const override
	{
		double val = input.toDouble();
		if (!m_initialized) {
			m_last = val;
			m_initialized = true;
			return input;
		}
		if (std::abs(val - m_last) < m_zone)
			return QVariant(m_last); // Swallow jitter
		m_last = val;
		return input;
	}

	QString name() const override { return "Deadzone"; }

	void set_zone(double z) { m_zone = z; }
	double zone() const { return m_zone; }

private:
	double m_zone;
	mutable double m_last;
	mutable bool m_initialized;
};

// ---------------------------------------------------------------------------
// QuantizeFilter — Snaps values to a grid (e.g. whole integers, dB steps).
// ---------------------------------------------------------------------------
class QuantizeFilter : public ControlFilter {
public:
	explicit QuantizeFilter(double step = 1.0)
		: m_step(step > 0.0 ? step : 1.0) {}

	QVariant process(const QVariant &input,
					 const ControlPort &/*port*/) const override
	{
		double val = input.toDouble();
		double snapped = std::round(val / m_step) * m_step;
		return QVariant(snapped);
	}

	QString name() const override { return "Quantize"; }

	void set_step(double s) { m_step = s > 0.0 ? s : 1.0; }
	double step() const { return m_step; }

private:
	double m_step;
};

// ---------------------------------------------------------------------------
// ClampFilter — Hard clamp to min/max range.
// ---------------------------------------------------------------------------
class ClampFilter : public ControlFilter {
public:
	ClampFilter(double min_val, double max_val)
		: m_min(min_val), m_max(max_val) {}

	QVariant process(const QVariant &input,
					 const ControlPort &/*port*/) const override
	{
		double val = input.toDouble();
		return QVariant(qBound(m_min, val, m_max));
	}

	QString name() const override { return "Clamp"; }

	void set_range(double min_val, double max_val) {
		m_min = min_val;
		m_max = max_val;
	}

private:
	double m_min, m_max;
};

// ---------------------------------------------------------------------------
// ScaleFilter — Linear scale + offset (val = val * scale + offset).
// ---------------------------------------------------------------------------
class ScaleFilter : public ControlFilter {
public:
	ScaleFilter(double scale = 1.0, double offset = 0.0)
		: m_scale(scale), m_offset(offset) {}

	QVariant process(const QVariant &input,
					 const ControlPort &/*port*/) const override
	{
		double val = input.toDouble();
		return QVariant(val * m_scale + m_offset);
	}

	QString name() const override { return "Scale"; }

private:
	double m_scale, m_offset;
};

// ---------------------------------------------------------------------------
// RateLimitFilter — Limits how fast a value can change (per-second rate).
// ---------------------------------------------------------------------------
class RateLimitFilter : public ControlFilter {
public:
	explicit RateLimitFilter(double max_rate_per_sec = 100.0)
		: m_max_rate(max_rate_per_sec)
		, m_last(0.0)
		, m_initialized(false) {}

	QVariant process(const QVariant &input,
					 const ControlPort &/*port*/) const override
	{
		double val = input.toDouble();
		if (!m_initialized) {
			m_last = val;
			m_initialized = true;
			m_timer.start();
			return input;
		}

		double elapsed_sec = m_timer.elapsed() / 1000.0;
		if (elapsed_sec < 0.001)
			elapsed_sec = 0.001;
		m_timer.restart();

		double delta = val - m_last;
		double max_delta = m_max_rate * elapsed_sec;

		if (std::abs(delta) > max_delta) {
			delta = (delta > 0 ? max_delta : -max_delta);
		}

		m_last += delta;
		return QVariant(m_last);
	}

	QString name() const override { return "RateLimit"; }

private:
	double m_max_rate;
	mutable double m_last;
	mutable bool m_initialized;
	mutable QElapsedTimer m_timer;
};

} // namespace super
