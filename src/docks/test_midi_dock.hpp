#pragma once

#include "../utils/persistable_widget.hpp"

#include <QSlider>
#include <QPushButton>
#include <QLabel>

// Example dock demonstrating PersistableWidget features:
//   - Persistence: slider values and mute state survive restarts
//   - MIDI assign: toolbar toggle → click overlay → learn from MIDI controller
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

	QSlider *m_volume_slider;
	QSlider *m_pan_slider;
	QPushButton *m_mute_btn;
	QLabel *m_volume_label;
	QLabel *m_pan_label;
};
