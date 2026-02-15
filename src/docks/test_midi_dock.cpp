#include "test_midi_dock.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

TestMidiDock::TestMidiDock(QWidget *parent)
	: PersistableWidget("test_midi_dock", parent)
{
	setup_ui();
}

void TestMidiDock::setup_ui()
{
	auto *layout = new QVBoxLayout(content_area());
	layout->setContentsMargins(8, 8, 8, 8);
	layout->setSpacing(8);

	// --- Volume ---
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

	// --- Pan ---
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

	// --- Mute ---
	{
		m_mute_btn = new QPushButton("Mute", content_area());
		m_mute_btn->setObjectName("mute");
		m_mute_btn->setCheckable(true);
		m_mute_btn->setStyleSheet(
			"QPushButton { padding: 6px; }"
			"QPushButton:checked { background-color: #c0392b; color: white; }");

		layout->addWidget(m_mute_btn);
	}

	layout->addStretch();

	// --- Register MIDI controls ---
	register_midi_control(m_volume_slider, "volume");
	register_midi_control(m_pan_slider, "pan");
	register_midi_control(m_mute_btn, "mute");
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
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

QJsonObject TestMidiDock::save_state() const
{
	QJsonObject obj = PersistableWidget::save_state();
	obj["volume"] = m_volume_slider->value();
	obj["pan"] = m_pan_slider->value();
	obj["muted"] = m_mute_btn->isChecked();
	return obj;
}

void TestMidiDock::load_state(const QJsonObject &state)
{
	PersistableWidget::load_state(state);

	if (state.contains("volume"))
		m_volume_slider->setValue(state["volume"].toInt(80));
	if (state.contains("pan"))
		m_pan_slider->setValue(state["pan"].toInt(0));
	if (state.contains("muted"))
		m_mute_btn->setChecked(state["muted"].toBool(false));

	update_labels();
}
