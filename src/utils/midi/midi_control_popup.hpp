#pragma once

#include <QFrame>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QString>
#include <QStringList>
#include <QVector>

struct MidiBinding;

// Popup shown when clicking a control in MIDI assign mode.
// Shows mode-specific mapping UI (Range/Toggle/Select/Trigger).
// Supports multiple bindings per control via a binding selector.
// Includes live MIDI preview.
class MidiControlPopup : public QFrame {
	Q_OBJECT

public:
	explicit MidiControlPopup(const QString &widget_id,
		const QString &control_name,
		int map_mode,  // MidiBinding::MapMode
		double output_range_min, double output_range_max,
		const QStringList &combo_items = {},
		QWidget *parent = nullptr);
	~MidiControlPopup() override;

	void show_near(QWidget *target);

signals:
	void closed();

protected:
	bool event(QEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	void setup_ui();
	void populate_devices();
	void rebuild_binding_selector();
	void select_binding(int local_index);
	void load_binding_to_ui(const MidiBinding &b);
	void reset_ui_to_defaults();
	MidiBinding build_binding_from_ui() const;

	void on_add_clicked();
	void on_remove_clicked();
	void on_learn_clicked();
	void on_apply_clicked();
	void on_binding_learned(const MidiBinding &binding);
	void on_learn_cancelled();

	void update_select_labels();
	void update_preview(int raw_value);
	QVector<int> compute_default_thresholds(int count) const;

	QString m_widget_id;
	QString m_control_name;
	int m_map_mode;
	double m_default_out_min;
	double m_default_out_max;
	QStringList m_combo_items;

	// Current binding indices (global MidiRouter indices)
	QVector<int> m_binding_indices;
	int m_selected_local = -1;

	// Binding selector
	QComboBox *m_binding_combo;
	QPushButton *m_add_btn;
	QPushButton *m_remove_btn;

	// Common UI
	QLabel *m_title_label;
	QLabel *m_status_label;
	QLabel *m_preview_label;
	QComboBox *m_device_combo;
	QSpinBox *m_channel_spin;
	QSpinBox *m_cc_spin;
	QCheckBox *m_invert_check;
	QCheckBox *m_enabled_check;

	// Range mode UI
	QGroupBox *m_range_group = nullptr;
	QSpinBox *m_input_min_spin = nullptr;
	QSpinBox *m_input_max_spin = nullptr;
	QDoubleSpinBox *m_output_min_spin = nullptr;
	QDoubleSpinBox *m_output_max_spin = nullptr;

	// Toggle/Trigger mode UI
	QGroupBox *m_threshold_group = nullptr;
	QSpinBox *m_threshold_spin = nullptr;

	// Select mode UI
	QGroupBox *m_select_group = nullptr;
	QVector<QSpinBox *> m_select_boundary_spins;
	QVector<QLabel *> m_select_range_labels;

	QPushButton *m_learn_btn;
	QPushButton *m_apply_btn;
	QPushButton *m_close_btn;

	// MIDI Monitor
	QPushButton *m_monitor_toggle;
	QWidget *m_monitor_container;
	QPlainTextEdit *m_monitor_log;
	QPushButton *m_monitor_clear_btn;
	int m_monitor_msg_count = 0;

	void on_raw_midi(int device, int status, int data1, int data2);
	void toggle_monitor(bool expanded);

	// Drag state
	bool m_dragging = false;
	QPoint m_drag_offset;
};
