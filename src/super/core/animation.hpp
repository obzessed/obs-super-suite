#pragma once

// ============================================================================
// Animation System — Tweening and Custom Easing
//
// Provides:
//   • Tween: A standalone animation that drives a ControlPort value.
//   • TweenManager: Owns active tweens, ticks them on a timer.
//   • Custom easing via cubic bezier curves.
// ============================================================================

#include <QObject>
#include <QVariant>
#include <QEasingCurve>
#include <QElapsedTimer>
#include <QTimer>
#include <QList>
#include <QPoint>
#include <functional>
#include <memory>

namespace super {

class ControlPort;

// ---------------------------------------------------------------------------
// Tween — A single animation targeting a double value range.
// ---------------------------------------------------------------------------
class Tween {
public:
	Tween() = default;

	Tween(double from, double to, int duration_ms,
		  QEasingCurve curve = QEasingCurve::Linear)
		: m_from(from), m_to(to)
		, m_duration(duration_ms)
		, m_curve(curve)
	{
		m_timer.start();
	}

	// Current progress 0..1
	double progress() const {
		if (m_duration <= 0)
			return 1.0;
		double t = static_cast<double>(m_timer.elapsed()) / m_duration;
		return qBound(0.0, t, 1.0);
	}

	// Current value (eased)
	double value() const {
		double t = m_curve.valueForProgress(progress());
		return m_from + t * (m_to - m_from);
	}

	bool is_finished() const {
		return m_timer.elapsed() >= m_duration;
	}

	double target() const { return m_to; }

	// Callback invoked each tick
	std::function<void(double)> on_update;
	// Callback invoked when finished
	std::function<void()> on_complete;

private:
	double m_from = 0.0;
	double m_to = 1.0;
	int m_duration = 1000;
	QEasingCurve m_curve;
	QElapsedTimer m_timer;
};

// ---------------------------------------------------------------------------
// TweenManager — Ticks all active tweens at ~60fps.
// ---------------------------------------------------------------------------
class TweenManager : public QObject {
	Q_OBJECT

public:
	static TweenManager &instance() {
		static TweenManager s;
		return s;
	}

	// Create a tween that calls `callback` with the interpolated value.
	// Returns a handle that can be used to cancel.
	int animate(double from, double to, int duration_ms,
				std::function<void(double)> callback,
				QEasingCurve::Type curve = QEasingCurve::Linear,
				std::function<void()> on_complete = {})
	{
		auto *tween = new Tween(from, to, duration_ms, QEasingCurve(curve));
		tween->on_update = std::move(callback);
		tween->on_complete = std::move(on_complete);

		int handle = m_next_handle++;
		m_tweens.insert(handle, tween);

		if (!m_tick_timer.isActive())
			m_tick_timer.start(16);  // ~60fps

		return handle;
	}

	// Animate a ControlPort to a target value.
	int animate_port(ControlPort *port, double target, int duration_ms,
					  QEasingCurve::Type curve = QEasingCurve::Linear);

	void cancel(int handle) {
		if (auto *tw = m_tweens.take(handle)) {
			delete tw;
		}
		if (m_tweens.isEmpty())
			m_tick_timer.stop();
	}

	void cancel_all() {
		qDeleteAll(m_tweens);
		m_tweens.clear();
		m_tick_timer.stop();
	}

	int active_count() const { return m_tweens.size(); }

signals:
	void tween_completed(int handle);

private:
	TweenManager() : QObject(nullptr) {
		m_tick_timer.setTimerType(Qt::PreciseTimer);
		connect(&m_tick_timer, &QTimer::timeout, this, &TweenManager::tick);
	}

	void tick() {
		QList<int> finished;
		for (auto it = m_tweens.begin(); it != m_tweens.end(); ++it) {
			auto *tw = it.value();
			if (tw->on_update)
				tw->on_update(tw->value());
			if (tw->is_finished())
				finished.append(it.key());
		}
		for (int h : finished) {
			auto *tw = m_tweens.take(h);
			if (tw) {
				if (tw->on_update)
					tw->on_update(tw->target());  // Snap to final value
				if (tw->on_complete)
					tw->on_complete();
				delete tw;
			}
			emit tween_completed(h);
		}
		if (m_tweens.isEmpty())
			m_tick_timer.stop();
	}

	QTimer m_tick_timer;
	QHash<int, Tween*> m_tweens;
	int m_next_handle = 1;
};

// ---------------------------------------------------------------------------
// Custom Bezier Easing — Create curves from control points.
// ---------------------------------------------------------------------------
inline QEasingCurve bezier_ease(qreal x1, qreal y1, qreal x2, qreal y2)
{
	QEasingCurve curve(QEasingCurve::BezierSpline);
	curve.addCubicBezierSegment(QPointF(x1, y1), QPointF(x2, y2),
								QPointF(1.0, 1.0));
	return curve;
}

} // namespace super
