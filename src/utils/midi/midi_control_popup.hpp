#pragma once

#include <QFrame>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QString>

struct MidiBinding;

// Popup shown when clicking a control in MIDI assign mode.
// Displays the current binding (device, channel, CC), value mapping,
// allows MIDI Learn, manual editing, and unbinding.
class MidiControlPopup : public QFrame {
	Q_OBJECT

public:
	// output_range_min/max: default output range from the target control
	explicit MidiControlPopup(const QString &widget_id,
		const QString &control_name,
		int output_range_min, int output_range_max,
		QWidget *parent = nullptr);
	~MidiControlPopup() override;

	// Position the popup near a target widget
	void show_near(QWidget *target);

signals:
	// Emitted when the user closes the popup
	void closed();

protected:
	bool event(QEvent *event) override;

private:
	void setup_ui();
	void refresh_from_binding();
	void populate_devices();

	void on_learn_clicked();
	void on_apply_clicked();
	void on_unassign_clicked();
	void on_binding_learned(const MidiBinding &binding);
	void on_learn_cancelled();

	QString m_widget_id;
	QString m_control_name;
	int m_default_out_min;
	int m_default_out_max;

	// UI
	QLabel *m_title_label;
	QLabel *m_status_label;
	QComboBox *m_device_combo;
	QSpinBox *m_channel_spin;
	QSpinBox *m_cc_spin;

	// Value mapping
	QSpinBox *m_input_min_spin;
	QSpinBox *m_input_max_spin;
	QSpinBox *m_output_min_spin;
	QSpinBox *m_output_max_spin;
	QCheckBox *m_invert_check;
	QCheckBox *m_enabled_check;

	QPushButton *m_learn_btn;
	QPushButton *m_apply_btn;
	QPushButton *m_unassign_btn;
	QPushButton *m_close_btn;

	// MIDI Monitor (accordion)
	QPushButton *m_monitor_toggle;
	QWidget *m_monitor_container;
	QPlainTextEdit *m_monitor_log;
	QPushButton *m_monitor_clear_btn;
	int m_monitor_msg_count = 0;

	void on_raw_midi(int device, int status, int data1, int data2);
	void toggle_monitor(bool expanded);
};
