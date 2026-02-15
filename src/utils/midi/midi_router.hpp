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

	// Value mapping: raw MIDI (input_min..input_max) → (output_min..output_max)
	int input_min = 0;
	int input_max = 127;
	int output_min = 0;
	int output_max = 127;
	bool invert = false;
	bool enabled = true;   // false = binding exists but is muted

	// Map a raw MIDI value through the input/output ranges
	int map_value(int raw) const;

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
	void add_binding(const MidiBinding &b);
	void remove_binding(const QString &widget_id, const QString &control_name);
	void remove_all_bindings(const QString &widget_id);
	QVector<MidiBinding> bindings_for(const QString &widget_id) const;
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
	void midi_cc_received(const QString &widget_id, const QString &control_name, int value);
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
