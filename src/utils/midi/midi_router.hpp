#pragma once

#include "midi_backend.hpp"

#include <QObject>
#include <QString>
#include <QVector>
#include <QJsonObject>

#include <memory>

// Persisted mapping from a MIDI message to a widget control
struct MidiBinding {
	int device_index = -1;  // -1 = any device
	int channel = 0;        // MIDI channel 0-15
	int cc = 0;             // CC number 0-127 (or note number for NoteOn/NoteOff)
	enum Type { CC = 0, NoteOn = 1, NoteOff = 2 };
	Type type = CC;
	QString widget_id;      // PersistableWidget::widget_id()
	QString control_name;   // Registered control name within the widget

	// --- Mapping mode ---
	// Determines how raw MIDI values are translated for the control.
	enum MapMode {
		Range   = 0, // Linear map: input range → output range (sliders, dials, spinboxes)
		Toggle  = 1, // Threshold toggle: > threshold = on (checkboxes, checkable buttons)
		Select  = 2, // Normalized 0-1 mapped to combo item indices
		Trigger = 3, // Fire once when value crosses above threshold (non-checkable buttons)
	};
	MapMode map_mode = Range;

	// Range mode: raw MIDI (input_min..input_max) → (output_min..output_max)
	int input_min = 0;
	int input_max = 127;
	double output_min = 0.0;
	double output_max = 127.0;

	// Toggle/Trigger mode: raw MIDI threshold (values above = on)
	int threshold = 63;

	// Select mode: item count and optional per-item boundaries
	int select_count = 0;           // number of combo items (set at bind time)
	QVector<int> select_thresholds; // N-1 upper-boundary values for N items (empty = even split)

	bool invert = false;
	bool enabled = true;   // false = binding exists but is muted

	// Runtime state (not serialized) — for edge detection in Toggle/Trigger
	int last_raw = 0;

	// Map a raw MIDI value. Returns:
	//   Range:   value in [output_min, output_max]
	//   Toggle:  0.0 or 1.0
	//   Select:  item index (0, 1, 2, ...)
	//   Trigger: 0.0 or 1.0
	double map_value(int raw) const;

	QJsonObject to_json() const;
	static MidiBinding from_json(const QJsonObject &obj);
};

// Singleton MIDI router.
// Owns the MidiBackend, manages bindings, and dispatches MIDI values to widgets.
class MidiRouter : public QObject {
	Q_OBJECT

public:
	static MidiRouter *instance();
	static void cleanup();

	// Backend access
	MidiBackend *backend() const;

	// Device management
	QStringList available_devices() const;
	bool open_device(int index);
	void open_all_devices();
	void close_all();

	// Binding management
	void add_binding(const MidiBinding &b);  // appends (allows multiple per control)
	void update_binding_at(int index, const MidiBinding &b);
	void remove_binding_at(int index);
	void remove_binding(const QString &widget_id, const QString &control_name); // removes ALL for widget+control
	void remove_all_bindings(const QString &widget_id);
	QVector<MidiBinding> bindings_for(const QString &widget_id) const;
	QVector<int> binding_indices_for(const QString &widget_id, const QString &control_name) const;
	const QVector<MidiBinding> &all_bindings() const;

	// MIDI Learn
	void start_learn(const QString &widget_id, const QString &control_name);
	void cancel_learn();
	bool is_learning() const;

	// Persistence
	QJsonObject save() const;
	void load(const QJsonObject &obj);

signals:
	// Dispatched when a bound CC message arrives
	void midi_cc_received(const QString &widget_id, const QString &control_name, double value);
	// Dispatched when a bound Note On message arrives
	void midi_note_received(const QString &widget_id, const QString &control_name, int velocity);
	// Emitted when learn mode captures a binding
	void binding_learned(const MidiBinding &binding);
	// Emitted when learn mode is cancelled
	void learn_cancelled();

private:
	MidiRouter();
	~MidiRouter() override;

	void on_midi_message(int device, int status, int data1, int data2);

	std::unique_ptr<MidiBackend> m_backend;
	QVector<MidiBinding> m_bindings;

	// Learn state
	bool m_learning = false;
	QString m_learn_widget_id;
	QString m_learn_control_name;
};
