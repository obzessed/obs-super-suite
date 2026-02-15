#include "test_midi_dock.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>

TestMidiDock::TestMidiDock(QWidget *parent)
	: PersistableWidget("test_midi_dock", parent)
{
	setup_ui();
}

void TestMidiDock::setup_ui()
{
	auto *layout = new QVBoxLayout(content_area());
	layout->setContentsMargins(8, 8, 8, 8);
	layout->setSpacing(6);

	// --- Volume (QSlider) ---
	{
		auto *group = new QGroupBox("Volume", content_area());
		auto *row = new QHBoxLayout(group);

		m_volume_slider = new QSlider(Qt::Horizontal, group);
		m_volume_slider->setObjectName("volume");
		m_volume_slider->setRange(0, 100);
		m_volume_slider->setValue(80);

		m_volume_label = new QLabel("80", group);
		m_volume_label->setFixedWidth(32);
		m_volume_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

		row->addWidget(m_volume_slider, 1);
		row->addWidget(m_volume_label);

		layout->addWidget(group);

		connect(m_volume_slider, &QSlider::valueChanged,
			this, &TestMidiDock::update_labels);
	}

	// --- Pan (QSlider, bipolar) ---
	{
		auto *group = new QGroupBox("Pan", content_area());
		auto *row = new QHBoxLayout(group);

		m_pan_slider = new QSlider(Qt::Horizontal, group);
		m_pan_slider->setObjectName("pan");
		m_pan_slider->setRange(-100, 100);
		m_pan_slider->setValue(0);

		m_pan_label = new QLabel("C", group);
		m_pan_label->setFixedWidth(32);
		m_pan_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

		row->addWidget(m_pan_slider, 1);
		row->addWidget(m_pan_label);

		layout->addWidget(group);

		connect(m_pan_slider, &QSlider::valueChanged,
			this, &TestMidiDock::update_labels);
	}

	// --- Send (QDial) ---
	{
		auto *group = new QGroupBox("Send", content_area());
		auto *row = new QHBoxLayout(group);

		m_send_dial = new QDial(group);
		m_send_dial->setObjectName("send");
		m_send_dial->setRange(0, 100);
		m_send_dial->setValue(0);
		m_send_dial->setFixedSize(48, 48);
		m_send_dial->setNotchesVisible(true);

		m_send_label = new QLabel("0", group);
		m_send_label->setFixedWidth(32);
		m_send_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

		row->addWidget(m_send_dial);
		row->addWidget(m_send_label);
		row->addStretch();

		layout->addWidget(group);

		connect(m_send_dial, &QDial::valueChanged,
			this, &TestMidiDock::update_labels);
	}

	// --- Delay & Gain (QSpinBox, QDoubleSpinBox) ---
	{
		auto *group = new QGroupBox("Parameters", content_area());
		auto *form = new QFormLayout(group);
		form->setContentsMargins(8, 4, 8, 8);
		form->setSpacing(4);

		m_delay_spin = new QSpinBox(group);
		m_delay_spin->setObjectName("delay");
		m_delay_spin->setRange(0, 500);
		m_delay_spin->setValue(0);
		m_delay_spin->setSuffix(" ms");
		form->addRow("Delay:", m_delay_spin);

		m_gain_spin = new QDoubleSpinBox(group);
		m_gain_spin->setObjectName("gain");
		m_gain_spin->setRange(-24.0, 24.0);
		m_gain_spin->setValue(0.0);
		m_gain_spin->setSuffix(" dB");
		m_gain_spin->setSingleStep(0.1);
		m_gain_spin->setDecimals(1);
		form->addRow("Gain:", m_gain_spin);

		layout->addWidget(group);
	}

	// --- Mode (QComboBox) ---
	{
		auto *group = new QGroupBox("Mode", content_area());
		auto *row = new QHBoxLayout(group);

		m_mode_combo = new QComboBox(group);
		m_mode_combo->setObjectName("mode");
		m_mode_combo->addItems({"Normal", "Sidechain", "Ducking",
			"Gate", "Compressor", "Limiter"});
		row->addWidget(m_mode_combo, 1);

		layout->addWidget(group);
	}

	// --- Toggles (QCheckBox, QPushButton) ---
	{
		auto *group = new QGroupBox("Toggles", content_area());
		auto *row = new QHBoxLayout(group);

		m_solo_check = new QCheckBox("Solo", group);
		m_solo_check->setObjectName("solo");
		m_solo_check->setStyleSheet(
			"QCheckBox::indicator:checked { background-color: #f1c40f; border-radius: 2px; }");
		row->addWidget(m_solo_check);

		m_mute_btn = new QPushButton("Mute", group);
		m_mute_btn->setObjectName("mute");
		m_mute_btn->setCheckable(true);
		m_mute_btn->setStyleSheet(
			"QPushButton { padding: 4px 12px; }"
			"QPushButton:checked { background-color: #c0392b; color: white; }");
		row->addWidget(m_mute_btn);

		m_rec_btn = new QPushButton("Rec", group);
		m_rec_btn->setObjectName("rec");
		m_rec_btn->setCheckable(true);
		m_rec_btn->setStyleSheet(
			"QPushButton { padding: 4px 12px; }"
			"QPushButton:checked { background-color: #e74c3c; color: white; }");
		row->addWidget(m_rec_btn);

		layout->addWidget(group);
	}

	layout->addStretch();

	// --- Register all MIDI controls ---
	register_midi_control(m_volume_slider, "volume");
	register_midi_control(m_pan_slider, "pan");
	register_midi_control(m_send_dial, "send");
	register_midi_control(m_delay_spin, "delay");
	register_midi_control(m_gain_spin, "gain");
	register_midi_control(m_mode_combo, "mode");
	register_midi_control(m_solo_check, "solo");
	register_midi_control(m_mute_btn, "mute");
	register_midi_control(m_rec_btn, "rec");
}

void TestMidiDock::update_labels()
{
	m_volume_label->setText(QString::number(m_volume_slider->value()));

	int pan = m_pan_slider->value();
	if (pan == 0)
		m_pan_label->setText("C");
	else if (pan < 0)
		m_pan_label->setText(QString("L%1").arg(-pan));
	else
		m_pan_label->setText(QString("R%1").arg(pan));

	m_send_label->setText(QString::number(m_send_dial->value()));
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

QJsonObject TestMidiDock::save_state() const
{
	QJsonObject obj = PersistableWidget::save_state();
	obj["volume"] = m_volume_slider->value();
	obj["pan"] = m_pan_slider->value();
	obj["send"] = m_send_dial->value();
	obj["delay"] = m_delay_spin->value();
	obj["gain"] = m_gain_spin->value();
	obj["mode"] = m_mode_combo->currentIndex();
	obj["solo"] = m_solo_check->isChecked();
	obj["muted"] = m_mute_btn->isChecked();
	obj["rec"] = m_rec_btn->isChecked();
	return obj;
}

void TestMidiDock::load_state(const QJsonObject &state)
{
	PersistableWidget::load_state(state);

	if (state.contains("volume"))
		m_volume_slider->setValue(state["volume"].toInt(80));
	if (state.contains("pan"))
		m_pan_slider->setValue(state["pan"].toInt(0));
	if (state.contains("send"))
		m_send_dial->setValue(state["send"].toInt(0));
	if (state.contains("delay"))
		m_delay_spin->setValue(state["delay"].toInt(0));
	if (state.contains("gain"))
		m_gain_spin->setValue(state["gain"].toDouble(0.0));
	if (state.contains("mode"))
		m_mode_combo->setCurrentIndex(state["mode"].toInt(0));
	if (state.contains("solo"))
		m_solo_check->setChecked(state["solo"].toBool(false));
	if (state.contains("muted"))
		m_mute_btn->setChecked(state["muted"].toBool(false));
	if (state.contains("rec"))
		m_rec_btn->setChecked(state["rec"].toBool(false));

	update_labels();
}
