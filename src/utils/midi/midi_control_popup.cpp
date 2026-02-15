#include "midi_control_popup.hpp"
#include "midi_router.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QKeyEvent>
#include <QApplication>
#include <QScreen>

static const char *POPUP_STYLE = R"(
MidiControlPopup {
	background-color: #2b2b2b;
	border: 1px solid #555;
	border-radius: 8px;
	padding: 4px;
}
QLabel {
	color: #ddd;
}
QLabel#title {
	font-weight: bold;
	font-size: 13px;
	color: #fff;
}
QLabel#status {
	font-size: 11px;
	color: #8cf;
	padding: 4px;
	background-color: rgba(50, 50, 80, 150);
	border-radius: 4px;
}
QGroupBox {
	color: #aaa;
	font-size: 11px;
	border: 1px solid #444;
	border-radius: 4px;
	margin-top: 8px;
	padding-top: 12px;
}
QGroupBox::title {
	subcontrol-origin: margin;
	left: 8px;
	padding: 0 4px;
}
QComboBox, QSpinBox {
	background-color: #3a3a3a;
	color: #eee;
	border: 1px solid #555;
	border-radius: 3px;
	padding: 2px 4px;
	min-height: 22px;
}
QComboBox:focus, QSpinBox:focus {
	border-color: #6af;
}
QComboBox::drop-down {
	border: none;
}
QCheckBox {
	color: #ddd;
	spacing: 6px;
}
QCheckBox::indicator {
	width: 14px;
	height: 14px;
}
QPushButton {
	padding: 5px 12px;
	border-radius: 4px;
	border: 1px solid #555;
	background-color: #3a3a3a;
	color: #ddd;
	font-size: 12px;
}
QPushButton:hover {
	background-color: #4a4a4a;
	border-color: #777;
}
QPushButton#learn {
	background-color: #1a5276;
	border-color: #2980b9;
	color: #fff;
	font-weight: bold;
}
QPushButton#learn:hover {
	background-color: #2471a3;
}
QPushButton#learn:checked {
	background-color: #c0392b;
	border-color: #e74c3c;
}
QPushButton#unassign {
	color: #e57373;
}
QPushButton#unassign:hover {
	background-color: #4a2a2a;
}
QPushButton#monitor_toggle {
	text-align: left;
	padding: 4px 8px;
	font-size: 11px;
	color: #aaa;
	background-color: #333;
	border: 1px solid #444;
	border-radius: 3px;
}
QPushButton#monitor_toggle:hover {
	color: #ddd;
	background-color: #3a3a3a;
}
QPlainTextEdit#monitor_log {
	background-color: #1a1a1a;
	color: #0f0;
	border: 1px solid #333;
	border-radius: 3px;
	font-family: 'Consolas', 'Courier New', monospace;
	font-size: 10px;
	padding: 4px;
}
)";

MidiControlPopup::MidiControlPopup(const QString &widget_id,
	const QString &control_name,
	int output_range_min, int output_range_max,
	QWidget *parent)
	: QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
	, m_widget_id(widget_id)
	, m_control_name(control_name)
	, m_default_out_min(output_range_min)
	, m_default_out_max(output_range_max)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setStyleSheet(POPUP_STYLE);

	setup_ui();
	populate_devices();
	refresh_from_binding();

	// Listen for learn results
	auto *router = MidiRouter::instance();
	connect(router, &MidiRouter::binding_learned,
		this, &MidiControlPopup::on_binding_learned);
	connect(router, &MidiRouter::learn_cancelled,
		this, &MidiControlPopup::on_learn_cancelled);

	// Listen for raw MIDI for the monitor
	if (router->backend()) {
		connect(router->backend(), &MidiBackend::midi_message,
			this, &MidiControlPopup::on_raw_midi);
	}
}

MidiControlPopup::~MidiControlPopup()
{
	// Cancel any in-progress learn when popup closes
	if (MidiRouter::instance()->is_learning())
		MidiRouter::instance()->cancel_learn();

	emit closed();
}

void MidiControlPopup::setup_ui()
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(12, 10, 12, 10);
	root->setSpacing(6);

	// --- Title row with enable toggle ---
	{
		auto *title_row = new QHBoxLayout();
		m_title_label = new QLabel(this);
		m_title_label->setObjectName("title");
		m_title_label->setText(
			QString::fromUtf8("\xF0\x9F\x8E\xB9  %1").arg(m_control_name));
		title_row->addWidget(m_title_label);

		title_row->addStretch();

		m_enabled_check = new QCheckBox("Enabled", this);
		m_enabled_check->setChecked(true);
		m_enabled_check->setToolTip("Enable/disable MIDI control for this binding");
		title_row->addWidget(m_enabled_check);

		root->addLayout(title_row);
	}

	// --- Status ---
	m_status_label = new QLabel("Not assigned", this);
	m_status_label->setObjectName("status");
	m_status_label->setAlignment(Qt::AlignCenter);
	root->addWidget(m_status_label);

	// --- MIDI Source ---
	{
		auto *group = new QGroupBox("MIDI Source", this);
		auto *form = new QFormLayout(group);
		form->setContentsMargins(8, 4, 8, 8);
		form->setSpacing(4);
		form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

		m_device_combo = new QComboBox(group);
		m_device_combo->setMinimumWidth(160);
		form->addRow("Device:", m_device_combo);

		m_channel_spin = new QSpinBox(group);
		m_channel_spin->setRange(0, 15);
		m_channel_spin->setPrefix("Ch ");
		form->addRow("Channel:", m_channel_spin);

		m_cc_spin = new QSpinBox(group);
		m_cc_spin->setRange(0, 127);
		m_cc_spin->setPrefix("CC ");
		form->addRow("CC:", m_cc_spin);

		root->addWidget(group);
	}

	// --- Value Mapping ---
	{
		auto *group = new QGroupBox("Value Mapping", this);
		auto *form = new QFormLayout(group);
		form->setContentsMargins(8, 4, 8, 8);
		form->setSpacing(4);
		form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

		// Input range (MIDI side)
		auto *input_row = new QHBoxLayout();
		m_input_min_spin = new QSpinBox(group);
		m_input_min_spin->setRange(0, 127);
		m_input_min_spin->setValue(0);
		auto *input_dash = new QLabel("\xe2\x86\x92", group); // →
		input_dash->setAlignment(Qt::AlignCenter);
		input_dash->setFixedWidth(20);
		m_input_max_spin = new QSpinBox(group);
		m_input_max_spin->setRange(0, 127);
		m_input_max_spin->setValue(127);
		input_row->addWidget(m_input_min_spin);
		input_row->addWidget(input_dash);
		input_row->addWidget(m_input_max_spin);
		form->addRow("MIDI In:", input_row);

		// Output range (control side)
		auto *output_row = new QHBoxLayout();
		m_output_min_spin = new QSpinBox(group);
		m_output_min_spin->setRange(-100000, 100000);
		m_output_min_spin->setValue(m_default_out_min);
		auto *output_dash = new QLabel("\xe2\x86\x92", group); // →
		output_dash->setAlignment(Qt::AlignCenter);
		output_dash->setFixedWidth(20);
		m_output_max_spin = new QSpinBox(group);
		m_output_max_spin->setRange(-100000, 100000);
		m_output_max_spin->setValue(m_default_out_max);
		output_row->addWidget(m_output_min_spin);
		output_row->addWidget(output_dash);
		output_row->addWidget(m_output_max_spin);
		form->addRow("Output:", output_row);

		m_invert_check = new QCheckBox("Invert", group);
		form->addRow("", m_invert_check);

		root->addWidget(group);
	}

	// --- Buttons ---
	auto *btn_row = new QHBoxLayout();
	btn_row->setSpacing(6);

	m_learn_btn = new QPushButton(
		QString::fromUtf8("\xF0\x9F\x8E\xB9 Learn"), this);
	m_learn_btn->setObjectName("learn");
	m_learn_btn->setCheckable(true);
	connect(m_learn_btn, &QPushButton::clicked,
		this, &MidiControlPopup::on_learn_clicked);

	m_apply_btn = new QPushButton("Apply", this);
	connect(m_apply_btn, &QPushButton::clicked,
		this, &MidiControlPopup::on_apply_clicked);

	m_unassign_btn = new QPushButton("Unassign", this);
	m_unassign_btn->setObjectName("unassign");
	connect(m_unassign_btn, &QPushButton::clicked,
		this, &MidiControlPopup::on_unassign_clicked);

	m_close_btn = new QPushButton("Close", this);
	connect(m_close_btn, &QPushButton::clicked,
		this, &MidiControlPopup::close);

	btn_row->addWidget(m_learn_btn);
	btn_row->addWidget(m_apply_btn);
	btn_row->addWidget(m_unassign_btn);
	btn_row->addStretch();
	btn_row->addWidget(m_close_btn);

	root->addLayout(btn_row);

	// --- MIDI Monitor (accordion) ---
	{
		m_monitor_toggle = new QPushButton(
			QString::fromUtf8("\xe2\x96\xb6 MIDI Monitor"), this);
		m_monitor_toggle->setObjectName("monitor_toggle");
		m_monitor_toggle->setCheckable(true);
		root->addWidget(m_monitor_toggle);

		m_monitor_container = new QWidget(this);
		auto *monitor_layout = new QVBoxLayout(m_monitor_container);
		monitor_layout->setContentsMargins(0, 4, 0, 0);
		monitor_layout->setSpacing(4);

		m_monitor_log = new QPlainTextEdit(m_monitor_container);
		m_monitor_log->setObjectName("monitor_log");
		m_monitor_log->setReadOnly(true);
		m_monitor_log->setMaximumBlockCount(200);
		m_monitor_log->setFixedHeight(120);
		m_monitor_log->setPlaceholderText("Waiting for MIDI input...");
		monitor_layout->addWidget(m_monitor_log);

		m_monitor_clear_btn = new QPushButton("Clear", m_monitor_container);
		m_monitor_clear_btn->setFixedWidth(60);
		connect(m_monitor_clear_btn, &QPushButton::clicked,
			m_monitor_log, &QPlainTextEdit::clear);
		monitor_layout->addWidget(m_monitor_clear_btn, 0,
			Qt::AlignRight);

		m_monitor_container->hide(); // collapsed by default
		root->addWidget(m_monitor_container);

		connect(m_monitor_toggle, &QPushButton::toggled,
			this, &MidiControlPopup::toggle_monitor);
	}

	setMinimumWidth(320);
}

void MidiControlPopup::populate_devices()
{
	m_device_combo->clear();
	m_device_combo->addItem("Any Device", -1);

	QStringList devices = MidiRouter::instance()->available_devices();
	for (int i = 0; i < devices.size(); i++) {
		m_device_combo->addItem(devices[i], i);
	}
}

void MidiControlPopup::refresh_from_binding()
{
	auto bindings = MidiRouter::instance()->bindings_for(m_widget_id);

	const MidiBinding *found = nullptr;
	for (const auto &b : bindings) {
		if (b.control_name == m_control_name) {
			found = &b;
			break;
		}
	}

	if (found) {
		// MIDI source
		int combo_idx = m_device_combo->findData(found->device_index);
		if (combo_idx >= 0)
			m_device_combo->setCurrentIndex(combo_idx);
		m_channel_spin->setValue(found->channel);
		m_cc_spin->setValue(found->cc);

		// Value mapping
		m_input_min_spin->setValue(found->input_min);
		m_input_max_spin->setValue(found->input_max);
		m_output_min_spin->setValue(found->output_min);
		m_output_max_spin->setValue(found->output_max);
		m_invert_check->setChecked(found->invert);
		m_enabled_check->setChecked(found->enabled);

		QString dev_name = (found->device_index == -1)
			? "Any"
			: m_device_combo->currentText();
		QString status_text =
			QString("Bound: %1 \xc2\xb7 Ch %2 \xc2\xb7 CC %3")
				.arg(dev_name)
				.arg(found->channel)
				.arg(found->cc);
		if (!found->enabled)
			status_text += " (disabled)";
		m_status_label->setText(status_text);

		if (found->enabled) {
			m_status_label->setStyleSheet(
				"QLabel#status { color: #8f8; background-color: rgba(30, 80, 30, 150); "
				"border-radius: 4px; padding: 4px; font-size: 11px; }");
		} else {
			m_status_label->setStyleSheet(
				"QLabel#status { color: #fa5; background-color: rgba(80, 60, 20, 150); "
				"border-radius: 4px; padding: 4px; font-size: 11px; }");
		}

		m_unassign_btn->setEnabled(true);
	} else {
		m_device_combo->setCurrentIndex(0);
		m_channel_spin->setValue(0);
		m_cc_spin->setValue(0);

		// Defaults for value mapping
		m_input_min_spin->setValue(0);
		m_input_max_spin->setValue(127);
		m_output_min_spin->setValue(m_default_out_min);
		m_output_max_spin->setValue(m_default_out_max);
		m_invert_check->setChecked(false);
		m_enabled_check->setChecked(true);

		m_status_label->setText("Not assigned");
		m_status_label->setStyleSheet(
			"QLabel#status { color: #aaa; background-color: rgba(50, 50, 50, 150); "
			"border-radius: 4px; padding: 4px; font-size: 11px; }");

		m_unassign_btn->setEnabled(false);
	}

	m_learn_btn->setChecked(false);
}

void MidiControlPopup::show_near(QWidget *target)
{
	// Position below the target widget, or above if no room
	QPoint global_tl = target->mapToGlobal(
		QPoint(0, target->height() + 4));

	// Ensure popup stays on screen
	adjustSize();
	QScreen *screen = QApplication::screenAt(global_tl);
	if (screen) {
		QRect avail = screen->availableGeometry();
		if (global_tl.y() + height() > avail.bottom()) {
			global_tl = target->mapToGlobal(
				QPoint(0, -height() - 4));
		}
		if (global_tl.x() + width() > avail.right()) {
			global_tl.setX(avail.right() - width());
		}
	}

	move(global_tl);
	show();
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void MidiControlPopup::on_learn_clicked()
{
	if (m_learn_btn->isChecked()) {
		MidiRouter::instance()->open_all_devices();
		MidiRouter::instance()->start_learn(m_widget_id, m_control_name);

		m_status_label->setText("Move a MIDI knob or fader...");
		m_status_label->setStyleSheet(
			"QLabel#status { color: #ff0; background-color: rgba(80, 80, 0, 150); "
			"border-radius: 4px; padding: 4px; font-size: 11px; font-weight: bold; }");
	} else {
		MidiRouter::instance()->cancel_learn();
	}
}

void MidiControlPopup::on_apply_clicked()
{
	MidiBinding binding;
	binding.device_index = m_device_combo->currentData().toInt();
	binding.channel = m_channel_spin->value();
	binding.cc = m_cc_spin->value();
	binding.type = MidiBinding::CC;
	binding.widget_id = m_widget_id;
	binding.control_name = m_control_name;

	// Value mapping
	binding.input_min = m_input_min_spin->value();
	binding.input_max = m_input_max_spin->value();
	binding.output_min = m_output_min_spin->value();
	binding.output_max = m_output_max_spin->value();
	binding.invert = m_invert_check->isChecked();
	binding.enabled = m_enabled_check->isChecked();

	MidiRouter::instance()->add_binding(binding);
	refresh_from_binding();
}

void MidiControlPopup::on_unassign_clicked()
{
	MidiRouter::instance()->remove_binding(m_widget_id, m_control_name);
	refresh_from_binding();
}

void MidiControlPopup::on_binding_learned(const MidiBinding &binding)
{
	if (binding.widget_id != m_widget_id ||
	    binding.control_name != m_control_name)
		return;

	// After learn, update the mapping fields from the UI back into the binding
	// so the user's mapping preferences are preserved
	MidiBinding updated = binding;
	updated.input_min = m_input_min_spin->value();
	updated.input_max = m_input_max_spin->value();
	updated.output_min = m_output_min_spin->value();
	updated.output_max = m_output_max_spin->value();
	updated.invert = m_invert_check->isChecked();
	updated.enabled = m_enabled_check->isChecked();
	MidiRouter::instance()->add_binding(updated);

	refresh_from_binding();
}

void MidiControlPopup::on_learn_cancelled()
{
	m_learn_btn->setChecked(false);
	refresh_from_binding();
}

// ---------------------------------------------------------------------------
// Event overrides
// ---------------------------------------------------------------------------

bool MidiControlPopup::event(QEvent *event)
{
	if (event->type() == QEvent::KeyPress) {
		auto *ke = static_cast<QKeyEvent *>(event);
		if (ke->key() == Qt::Key_Escape) {
			close();
			return true;
		}
	}
	return QFrame::event(event);
}

// ---------------------------------------------------------------------------
// MIDI Monitor
// ---------------------------------------------------------------------------

void MidiControlPopup::toggle_monitor(bool expanded)
{
	m_monitor_container->setVisible(expanded);
	m_monitor_toggle->setText(
		expanded
			? QString::fromUtf8("\xe2\x96\xbc MIDI Monitor")
			: QString::fromUtf8("\xe2\x96\xb6 MIDI Monitor"));

	// Open devices if expanding so we actually receive data
	if (expanded) {
		MidiRouter::instance()->open_all_devices();
		m_monitor_msg_count = 0;
	}

	adjustSize();
}

void MidiControlPopup::on_raw_midi(int device, int status, int data1, int data2)
{
	if (!m_monitor_container->isVisible())
		return;

	int msg_type = status & 0xF0;
	int channel = status & 0x0F;

	QString type_str;
	QString detail;

	switch (msg_type) {
	case 0xB0:
		type_str = "CC";
		detail = QString("CC %1 = %2").arg(data1).arg(data2);
		break;
	case 0x90:
		type_str = data2 > 0 ? "NoteOn" : "NoteOff";
		detail = QString("Note %1 vel %2").arg(data1).arg(data2);
		break;
	case 0x80:
		type_str = "NoteOff";
		detail = QString("Note %1 vel %2").arg(data1).arg(data2);
		break;
	case 0xE0:
		type_str = "PitchBend";
		detail = QString("val %1").arg(data1 | (data2 << 7));
		break;
	case 0xD0:
		type_str = "ChanPress";
		detail = QString("val %1").arg(data1);
		break;
	default:
		type_str = QString("0x%1").arg(msg_type, 2, 16, QChar('0'));
		detail = QString("d1=%1 d2=%2").arg(data1).arg(data2);
		break;
	}

	++m_monitor_msg_count;

	QString line = QString("[%1] Dev%2 Ch%3  %-10s %4")
		.arg(m_monitor_msg_count, 4)
		.arg(device)
		.arg(channel, 2)
		.arg(QString("%1  %2").arg(type_str, -10).arg(detail));

	m_monitor_log->appendPlainText(line);
}
