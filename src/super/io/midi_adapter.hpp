#pragma once
#include "../hal/hardware_profile.hpp"
#include "../core/control_types.hpp"
#include <QObject>
#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QHash>
#include <QElapsedTimer>
#include <QEasingCurve>
#include <cmath>

class MidiBackend;

namespace super {

class ControlPort;

// ---------------------------------------------------------------------------
// FilterStage — One step in a filter chain.
// Pre-filters operate on raw MIDI (0-127 int, cast to double).
// Post-filters operate on output-range values.
// ---------------------------------------------------------------------------
struct FilterStage {
	enum Type {
		Delay     = 0, // param1 = delay ms
		Debounce  = 1, // param1 = debounce ms
		RateLimit = 2, // param1 = max change/sec
		Deadzone  = 3, // param1 = threshold
		Clamp     = 4, // param1 = min, param2 = max
		Scale     = 5, // param1 = factor, param2 = offset
	};

	int type = Deadzone;
	bool enabled = true;
	double param1 = 0.0;
	double param2 = 0.0;

	mutable double rt_last = 0.0;
	mutable double rt_target = 0.0;  // For convergence (RateLimit, Delay)
	mutable bool rt_init = false;
	mutable QElapsedTimer rt_timer;

	double process(double val) const;
	bool needs_convergence() const;
	QString type_name() const;

	QJsonObject to_json() const;
	static FilterStage from_json(const QJsonObject &o);
};

// ---------------------------------------------------------------------------
// InterpStage — One step in the interpolation processing chain.
// Operates on normalized 0–1 values (between normalize and denormalize).
// ---------------------------------------------------------------------------
struct InterpStage {
	enum Type {
		Linear     = 0, // Pass-through (default)
		Quantize   = 1, // Snap to grid (param1 = step in 0-1 space)
		Smooth     = 2, // EMA smoothing (param1 = factor 0.01-1.0)
		SCurve     = 3, // Hermite smoothstep
		Easing     = 4, // QEasingCurve (param1 = curve type index)
		AnimateTo  = 5, // Slew to target (param1 = duration ms, param2 = easing)
		AnimateFrom= 6, // Slew from previous (param1 = duration ms, param2 = easing)
	};

	int type = Linear;
	bool enabled = true;
	double param1 = 0.0;
	double param2 = 0.0;

	// Runtime (mutable, not serialized)
	mutable double rt_accum = 0.0;
	mutable double rt_from = 0.0;
	mutable double rt_target = 0.0;
	mutable double rt_current = 0.0;
	mutable bool rt_init = false;
	mutable QElapsedTimer rt_timer;

	double process(double val) const;
	QString type_name() const;

	QJsonObject to_json() const;
	static InterpStage from_json(const QJsonObject &o);
};

// ---------------------------------------------------------------------------
// ValueMapPoint — One point on a multi-point transfer curve.
// ---------------------------------------------------------------------------
struct ValueMapPoint {
	int input = 0;
	double output = 0.0;
	QJsonObject to_json() const {
		QJsonObject o; o["i"] = input; o["o"] = output; return o;
	}
	static ValueMapPoint from_json(const QJsonObject &o) {
		return { o["i"].toInt(0), o["o"].toDouble(0.0) };
	}
};

// ---------------------------------------------------------------------------
// PipelinePreview — Full stage-by-stage value trace for UI display.
// ---------------------------------------------------------------------------
struct PipelinePreview {
	int raw_in;                        // Raw MIDI byte
	QVector<double> after_pre_filter;  // Value after each pre-filter
	double pre_filtered;               // Final pre-filtered value
	double normalized;                 // After normalization + invert
	QVector<double> after_interp;      // Value after each interp stage
	double mapped;                     // After denormalization
	QVector<double> after_post_filter; // Value after each post-filter
	double final_value;                // End result
	QString action_description;        // "SetValue 0.65" etc
	// Stage enabled flags (for UI dimming)
	QVector<bool> pre_filter_enabled;
	QVector<bool> interp_enabled;
	QVector<bool> post_filter_enabled;
	// Range info (for UI labels)
	int input_min = 0, input_max = 127;
	double output_min = 0.0, output_max = 1.0;
};

// ---------------------------------------------------------------------------
// ActionMode — What happens at the end of the pipeline.
// ---------------------------------------------------------------------------
enum class ActionMode {
	SetValue    = 0,  // Direct set_value on ControlPort
	AnimateTo   = 1,  // port->animate_to(value, param1_ms, param2_easing)
	AnimateFrom = 2,  // Animate from current to target
	Trigger     = 3,  // Fire value=1.0 then reset to 0.0
};

// ---------------------------------------------------------------------------
// MidiOutputBinding — Maps ControlPort value → MIDI CC output (feedback).
// ---------------------------------------------------------------------------
struct MidiOutputBinding {
	QString port_id;
	int device_index = -1;
	int channel = 0;
	int cc = 0;
	double input_min = 0.0;   // Port value → output_min MIDI
	double input_max = 1.0;   // Port value → output_max MIDI
	int output_min = 0;
	int output_max = 127;
	bool enabled = true;
	bool on_change = true;

	mutable double last_sent = -1.0;

	int map_to_midi(double port_value) const;

	QJsonObject to_json() const;
	static MidiOutputBinding from_json(const QJsonObject &o);
};

// ---------------------------------------------------------------------------
// MidiPortBinding — Maps a MIDI input to a ControlPort.
//
// Pipeline order:
//   MIDI In (raw) → pre_filters → normalize → interp_stages → denorm
//                 → post_filters → action
// ---------------------------------------------------------------------------
struct MidiPortBinding {
	int device_index = -1;
	int channel = 0;
	int data1 = 0;

	enum MsgType { CC = 0, NoteOn = 1, NoteOff = 2 };
	MsgType msg_type = CC;

	QString port_id;

	enum MapMode {
		Range   = 0,
		Toggle  = 1,
		Trigger = 2,
		Select  = 3,
	};
	MapMode map_mode = Range;

	// Simple mapping (used when curve_points is empty)
	int input_min = 0;
	int input_max = 127;
	double output_min = 0.0;
	double output_max = 1.0;

	// Multi-point transfer curve (optional)
	QVector<ValueMapPoint> curve_points;

	// Processing chains (pipeline order)
	QVector<FilterStage> pre_filters;    // Raw domain (0-127)
	QVector<InterpStage> interp_stages;  // Normalized domain (0-1)
	QVector<FilterStage> post_filters;   // Output domain

	// End-of-pipeline action
	ActionMode action_mode = ActionMode::SetValue;
	double action_param1 = 500.0;  // AnimateTo duration ms
	double action_param2 = 0.0;    // AnimateTo easing curve index

	// Toggle/Trigger
	int threshold = 63;
	int toggle_mode = 0;   // 0=Toggle, 1=Check (set on), 2=Uncheck (set off)

	// Select
	int select_count = 0;
	QVector<int> select_thresholds;

	bool invert = false;
	bool enabled = true;

	// Continuous fire
	bool continuous_fire = false;
	int continuous_fire_interval_ms = 100;

	// Encoder
	bool is_encoder = false;
	EncoderMode encoder_mode = EncoderMode::Absolute;
	double encoder_sensitivity = 1.0;

	// Runtime (not serialized)
	int last_raw = 0;
	bool currently_above = false;

	double map_value(int raw) const;
	bool needs_convergence() const;
	PipelinePreview preview_pipeline(int raw) const;

	QJsonObject to_json() const;
	static MidiPortBinding from_json(const QJsonObject &obj);
};

// ---------------------------------------------------------------------------
// MidiAdapter
// ---------------------------------------------------------------------------
class MidiAdapter : public QObject {
	Q_OBJECT

public:
	explicit MidiAdapter(QObject *parent = nullptr);
	~MidiAdapter() override;

	void attach(MidiBackend *backend);
	void detach();
	bool is_attached() const;
	MidiBackend *backend() const;

	// Input bindings
	void add_binding(const MidiPortBinding &b);
	void remove_binding(const QString &port_id);
	void remove_all_bindings();
	QVector<MidiPortBinding> bindings_for(const QString &port_id) const;
	const QVector<MidiPortBinding> &all_bindings() const;

	// Output bindings (feedback)
	void add_output(const MidiOutputBinding &o);
	void remove_output(const QString &port_id);
	void remove_all_outputs();
	QVector<MidiOutputBinding> outputs_for(const QString &port_id) const;
	const QVector<MidiOutputBinding> &all_outputs() const;

	// MIDI Learn
	void start_learn(const QString &port_id);
	void cancel_learn();
	bool is_learning() const;

	// Hardware Profiles
	void load_profile(const HardwareProfile &profile);
	const HardwareProfile &active_profile() const;

	// Persistence
	QJsonObject save() const;
	void load(const QJsonObject &obj);

signals:
	void binding_learned(const MidiPortBinding &binding);
	void learn_cancelled();
	void midi_dispatched(const QString &port_id, double value);

private:
	void on_midi_message(int device, int status, int data1, int data2);
	void on_convergence_tick();
	void start_continuous_fire(int binding_index);
	void stop_continuous_fire(int binding_index);
	void send_feedback(const QString &port_id, double value);

	MidiBackend *m_backend = nullptr;
	QVector<MidiPortBinding> m_bindings;
	QVector<MidiOutputBinding> m_outputs;
	HardwareProfile m_profile;

	bool m_learning = false;
	QString m_learn_port_id;
	QHash<int, QTimer *> m_continuous_timers;
	QTimer *m_convergence_timer = nullptr;
};

} // namespace super
