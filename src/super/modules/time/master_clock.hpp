#pragma once

// ============================================================================
// Master Clock & Scheduler
//
// Provides:
//   • MasterClock: BPM-driven clock with beat/bar signals.
//   • Scheduler: Time-based event triggering (cue points, calendar events).
//
// Future extensions: LTC/MTC timecode, Ableton Link.
// ============================================================================

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QList>
#include <QDateTime>
#include <functional>

namespace super {

// ---------------------------------------------------------------------------
// MasterClock — BPM-based timing source.
// ---------------------------------------------------------------------------
class MasterClock : public QObject {
	Q_OBJECT

public:
	static MasterClock &instance() {
		static MasterClock s;
		return s;
	}

	// -- BPM --
	double bpm() const { return m_bpm; }
	void set_bpm(double bpm) {
		m_bpm = qBound(20.0, bpm, 300.0);
		if (m_running)
			update_interval();
	}

	// -- Transport --
	void start() {
		m_running = true;
		m_beat_count = 0;
		m_elapsed.start();
		update_interval();
		m_tick_timer.start();
		emit transport_started();
	}

	void stop() {
		m_running = false;
		m_tick_timer.stop();
		emit transport_stopped();
	}

	bool is_running() const { return m_running; }

	// -- Position --
	int beat() const { return m_beat_count % m_beats_per_bar; }
	int bar() const { return m_beat_count / m_beats_per_bar; }
	int total_beats() const { return m_beat_count; }
	int beats_per_bar() const { return m_beats_per_bar; }
	void set_beats_per_bar(int n) { m_beats_per_bar = qBound(1, n, 16); }

	// -- Elapsed time (ms since start) --
	qint64 elapsed_ms() const {
		return m_running ? m_elapsed.elapsed() : 0;
	}

signals:
	void tick();					// Every beat subdivision
	void beat_signal(int beat);		// Every beat
	void bar_signal(int bar);		// Every bar
	void transport_started();
	void transport_stopped();

private:
	MasterClock() : QObject(nullptr) {
		m_tick_timer.setTimerType(Qt::PreciseTimer);
		connect(&m_tick_timer, &QTimer::timeout, this, &MasterClock::on_tick);
	}

	void update_interval() {
		// Interval = 60000 / BPM (ms per beat)
		int ms = static_cast<int>(60000.0 / m_bpm);
		m_tick_timer.setInterval(qMax(1, ms));
	}

	void on_tick() {
		emit tick();

		int prev_beat = m_beat_count % m_beats_per_bar;
		m_beat_count++;
		int cur_beat = m_beat_count % m_beats_per_bar;

		emit beat_signal(cur_beat);

		if (cur_beat < prev_beat)
			emit bar_signal(bar());
	}

	double m_bpm = 120.0;
	int m_beats_per_bar = 4;
	int m_beat_count = 0;
	bool m_running = false;
	QTimer m_tick_timer;
	QElapsedTimer m_elapsed;
};

// ---------------------------------------------------------------------------
// ScheduledEvent — A time-triggered action.
// ---------------------------------------------------------------------------
struct ScheduledEvent {
	int id = 0;
	QString name;
	QDateTime trigger_time;				// Absolute time
	int repeat_interval_ms = 0;		// 0 = one-shot
	std::function<void()> action;
	bool active = true;
};

// ---------------------------------------------------------------------------
// Scheduler — Calendar/time-based event manager.
// ---------------------------------------------------------------------------
class Scheduler : public QObject {
	Q_OBJECT

public:
	static Scheduler &instance() {
		static Scheduler s;
		return s;
	}

	// Schedule a one-shot event at a specific time.
	int schedule_at(const QString &name, const QDateTime &when,
					std::function<void()> action) {
		ScheduledEvent ev;
		ev.id = m_next_id++;
		ev.name = name;
		ev.trigger_time = when;
		ev.action = std::move(action);
		m_events.append(ev);
		ensure_timer();
		return ev.id;
	}

	// Schedule a repeating event.
	int schedule_repeating(const QString &name, int interval_ms,
						   std::function<void()> action) {
		ScheduledEvent ev;
		ev.id = m_next_id++;
		ev.name = name;
		ev.trigger_time = QDateTime::currentDateTime();
		ev.repeat_interval_ms = interval_ms;
		ev.action = std::move(action);
		m_events.append(ev);
		ensure_timer();
		return ev.id;
	}

	void cancel(int id) {
		m_events.erase(
			std::remove_if(m_events.begin(), m_events.end(),
				[id](const ScheduledEvent &e) { return e.id == id; }),
			m_events.end());
	}

	void cancel_all() { m_events.clear(); }

	QList<ScheduledEvent> upcoming_events() const { return m_events; }

signals:
	void event_triggered(const QString &name);

private:
	Scheduler() : QObject(nullptr) {
		m_check_timer.setInterval(1000);  // Check every second
		connect(&m_check_timer, &QTimer::timeout, this, &Scheduler::check);
	}

	void ensure_timer() {
		if (!m_check_timer.isActive() && !m_events.isEmpty())
			m_check_timer.start();
	}

	void check() {
		QDateTime now = QDateTime::currentDateTime();
		QList<int> to_remove;

		for (auto &ev : m_events) {
			if (!ev.active)
				continue;
			if (now >= ev.trigger_time) {
				if (ev.action)
					ev.action();
				emit event_triggered(ev.name);

				if (ev.repeat_interval_ms > 0) {
					ev.trigger_time = now.addMSecs(ev.repeat_interval_ms);
				} else {
					to_remove.append(ev.id);
				}
			}
		}

		for (int id : to_remove)
			cancel(id);

		if (m_events.isEmpty())
			m_check_timer.stop();
	}

	QTimer m_check_timer;
	QList<ScheduledEvent> m_events;
	int m_next_id = 1;
};

} // namespace super
