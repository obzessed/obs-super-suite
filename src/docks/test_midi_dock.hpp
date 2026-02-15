#pragma once

#include "../utils/persistable_widget.hpp"

#include <QSlider>
#include <QDial>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>

// Example dock demonstrating PersistableWidget features:
//   - Persistence: control values survive restarts
//   - MIDI assign: toolbar toggle -> click overlay -> learn from MIDI controller
//   - Showcases all supported control types
class TestMidiDock : public PersistableWidget {
	Q_OBJECT

public:
	explicit TestMidiDock(QWidget *parent = nullptr);
	~TestMidiDock() override = default;

	QJsonObject save_state() const override;
	void load_state(const QJsonObject &state) override;

private:
	void setup_ui();
	void update_labels();

	// Sliders
	QSlider *m_volume_slider;
	QSlider *m_pan_slider;
	QLabel *m_volume_label;
	QLabel *m_pan_label;

	// Dial
	QDial *m_send_dial;
	QLabel *m_send_label;

	// SpinBoxes
	QSpinBox *m_delay_spin;
	QDoubleSpinBox *m_gain_spin;

	// Combo
	QComboBox *m_mode_combo;

	// Toggles
	QCheckBox *m_solo_check;
	QPushButton *m_mute_btn;
	QPushButton *m_rec_btn;
};
