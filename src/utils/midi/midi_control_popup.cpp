#include "midi_control_popup.hpp"
#include "midi_router.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QApplication>
#include <QScreen>
#include <QKeyEvent>
#include <QMouseEvent>

// ---- Style ----------------------------------------------------------------

static const char *POPUP_STYLE = R"(
MidiControlPopup {
	background-color: rgba(28, 28, 32, 240);
	border: 1px solid rgba(100, 200, 255, 0.5);
	border-radius: 8px;
}
QLabel#title {
	font-size: 14px;
	font-weight: bold;
	color: #ddd;
}
QLabel#mode_label {
	font-size: 10px;
	color: #888;
	padding: 2px 6px;
	background-color: rgba(60, 60, 80, 150);
	border-radius: 3px;
}
QLabel#status {
	color: #aaa;
	background-color: rgba(50, 50, 50, 150);
	border-radius: 4px;
	padding: 4px;
	font-size: 11px;
}
QLabel#preview {
	color: #7cf;
	background-color: rgba(30, 50, 70, 150);
	border: 1px solid rgba(100, 200, 255, 0.2);
	border-radius: 4px;
	padding: 4px 8px;
	font-size: 11px;
	font-family: "Consolas", "Courier New", monospace;
}
QGroupBox {
	color: #aaa;
	border: 1px solid rgba(255, 255, 255, 0.08);
	border-radius: 4px;
	margin-top: 6px;
	padding-top: 14px;
}
QGroupBox::title {
	subcontrol-origin: margin;
	left: 8px;
	padding: 0 4px;
}
QPushButton {
	background-color: rgba(60, 60, 70, 200);
	color: #ccc;
	border: 1px solid rgba(255, 255, 255, 0.1);
	border-radius: 4px;
	padding: 4px 10px;
}
QPushButton:hover { background-color: rgba(80, 80, 100, 220); }
QPushButton:checked { background-color: rgba(50, 120, 200, 200); color: #fff; }
QPushButton#learn:checked {
	background-color: rgba(200, 180, 0, 200);
	color: #111;
	font-weight: bold;
}
QPushButton#remove_btn {
	background-color: rgba(160, 40, 40, 180);
	color: #fcc;
}
QPushButton#monitor_toggle {
	font-size: 11px;
	text-align: left;
	padding: 3px 6px;
}
QPlainTextEdit#monitor_log {
	background-color: rgba(0, 0, 0, 150);
	color: #0f0;
	font-family: "Consolas", "Courier New", monospace;
	font-size: 10px;
	border: 1px solid rgba(255, 255, 255, 0.08);
	border-radius: 3px;
}
QComboBox, QSpinBox, QDoubleSpinBox {
	background-color: rgba(40, 40, 50, 200);
	color: #ccc;
	border: 1px solid rgba(255, 255, 255, 0.1);
	border-radius: 3px;
	padding: 2px 4px;
}
QCheckBox {
	color: #ccc;
	spacing: 4px;
	padding: 4px;
}
QLabel.select_item {
	color: #ddd;
	font-size: 11px;
}
QLabel.select_range {
	color: #8bb;
	font-size: 10px;
	font-family: "Consolas", "Courier New", monospace;
}
)";

static const char *mode_names[] = { "Range", "Toggle", "Select", "Trigger" };

// ---- Constructor / Destructor ----------------------------------------------

MidiControlPopup::MidiControlPopup(const QString &widget_id,
	const QString &control_name,
	int map_mode,
	double output_range_min, double output_range_max,
	const QStringList &combo_items,
	QWidget *parent)
	: QFrame(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
	, m_widget_id(widget_id)
	, m_control_name(control_name)
	, m_map_mode(map_mode)
	, m_default_out_min(output_range_min)
	, m_default_out_max(output_range_max)
	, m_combo_items(combo_items)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setStyleSheet(POPUP_STYLE);

	setup_ui();
	populate_devices();
	rebuild_binding_selector();

	auto *router = MidiRouter::instance();
	connect(router, &MidiRouter::binding_learned,
		this, &MidiControlPopup::on_binding_learned);
	connect(router, &MidiRouter::learn_cancelled,
		this, &MidiControlPopup::on_learn_cancelled);

	if (router->backend()) {
		connect(router->backend(), &MidiBackend::midi_message,
			this, &MidiControlPopup::on_raw_midi);
	}
}

MidiControlPopup::~MidiControlPopup()
{
	if (MidiRouter::instance()->is_learning())
		MidiRouter::instance()->cancel_learn();
	emit closed();
}

// ---- Helpers ---------------------------------------------------------------

QVector<int> MidiControlPopup::compute_default_thresholds(int count) const
{
	// Evenly divide 0-127 into count segments
	QVector<int> thresholds;
	if (count <= 1)
		return thresholds;
	for (int i = 0; i < count - 1; i++) {
		thresholds.append((127 * (i + 1)) / count);
	}
	return thresholds;
}

// ---- UI Setup --------------------------------------------------------------

void MidiControlPopup::setup_ui()
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(12, 10, 12, 10);
	root->setSpacing(6);

	// --- Title row with mode label ---
	{
		auto *title_row = new QHBoxLayout();
		m_title_label = new QLabel(this);
		m_title_label->setObjectName("title");
		m_title_label->setText(
			QString::fromUtf8("\xF0\x9F\x8E\xB9  %1").arg(m_control_name));
		title_row->addWidget(m_title_label);

		title_row->addStretch();

		auto *mode_label = new QLabel(
			mode_names[std::clamp(m_map_mode, 0, 3)], this);
		mode_label->setObjectName("mode_label");
		title_row->addWidget(mode_label);

		root->addLayout(title_row);
	}

	// --- Binding selector ---
	{
		auto *sel_row = new QHBoxLayout();
		sel_row->setSpacing(4);

		m_binding_combo = new QComboBox(this);
		m_binding_combo->setMinimumWidth(140);
		connect(m_binding_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &MidiControlPopup::select_binding);
		sel_row->addWidget(m_binding_combo, 1);

		m_add_btn = new QPushButton("+", this);
		m_add_btn->setFixedWidth(28);
		m_add_btn->setToolTip("Add new binding");
		connect(m_add_btn, &QPushButton::clicked,
			this, &MidiControlPopup::on_add_clicked);
		sel_row->addWidget(m_add_btn);

		m_remove_btn = new QPushButton("\xe2\x88\x92", this);
		m_remove_btn->setFixedWidth(28);
		m_remove_btn->setObjectName("remove_btn");
		m_remove_btn->setToolTip("Remove selected binding");
		connect(m_remove_btn, &QPushButton::clicked,
			this, &MidiControlPopup::on_remove_clicked);
		sel_row->addWidget(m_remove_btn);

		root->addLayout(sel_row);
	}

	// --- Status ---
	m_status_label = new QLabel("No bindings", this);
	m_status_label->setObjectName("status");
	m_status_label->setAlignment(Qt::AlignCenter);
	root->addWidget(m_status_label);

	// --- Live Preview ---
	m_preview_label = new QLabel("", this);
	m_preview_label->setObjectName("preview");
	m_preview_label->setAlignment(Qt::AlignCenter);
	m_preview_label->hide();
	root->addWidget(m_preview_label);

	// --- MIDI Source (always shown) ---
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

	// --- Range mode: Input/Output ranges ---
	if (m_map_mode == MidiBinding::Range) {
		m_range_group = new QGroupBox("Value Mapping", this);
		auto *form = new QFormLayout(m_range_group);
		form->setContentsMargins(8, 4, 8, 8);
		form->setSpacing(4);
		form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

		auto *input_row = new QHBoxLayout();
		m_input_min_spin = new QSpinBox(m_range_group);
		m_input_min_spin->setRange(0, 127);
		m_input_min_spin->setValue(0);
		auto *input_dash = new QLabel("\xe2\x86\x92", m_range_group);
		input_dash->setAlignment(Qt::AlignCenter);
		input_dash->setFixedWidth(20);
		m_input_max_spin = new QSpinBox(m_range_group);
		m_input_max_spin->setRange(0, 127);
		m_input_max_spin->setValue(127);
		input_row->addWidget(m_input_min_spin);
		input_row->addWidget(input_dash);
		input_row->addWidget(m_input_max_spin);
		form->addRow("MIDI In:", input_row);

		auto *output_row = new QHBoxLayout();
		m_output_min_spin = new QDoubleSpinBox(m_range_group);
		m_output_min_spin->setRange(-100000.0, 100000.0);
		m_output_min_spin->setDecimals(2);
		m_output_min_spin->setValue(m_default_out_min);
		auto *output_dash = new QLabel("\xe2\x86\x92", m_range_group);
		output_dash->setAlignment(Qt::AlignCenter);
		output_dash->setFixedWidth(20);
		m_output_max_spin = new QDoubleSpinBox(m_range_group);
		m_output_max_spin->setRange(-100000.0, 100000.0);
		m_output_max_spin->setDecimals(2);
		m_output_max_spin->setValue(m_default_out_max);
		output_row->addWidget(m_output_min_spin);
		output_row->addWidget(output_dash);
		output_row->addWidget(m_output_max_spin);
		form->addRow("Output:", output_row);

		root->addWidget(m_range_group);
	}

	// --- Toggle/Trigger mode: Threshold ---
	if (m_map_mode == MidiBinding::Toggle || m_map_mode == MidiBinding::Trigger) {
		m_threshold_group = new QGroupBox("Threshold", this);
		auto *form = new QFormLayout(m_threshold_group);
		form->setContentsMargins(8, 4, 8, 8);
		form->setSpacing(4);
		form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

		m_threshold_spin = new QSpinBox(m_threshold_group);
		m_threshold_spin->setRange(0, 127);
		m_threshold_spin->setValue(63);
		m_threshold_spin->setToolTip("MIDI values above this trigger the action");
		form->addRow("Value >", m_threshold_spin);

		root->addWidget(m_threshold_group);
	}

	// --- Select mode: Per-item mapping table ---
	if (m_map_mode == MidiBinding::Select && m_combo_items.size() > 0) {
		m_select_group = new QGroupBox(
			QString("Item Mapping (%1 items)").arg(m_combo_items.size()), this);
		auto *grid = new QGridLayout(m_select_group);
		grid->setContentsMargins(8, 4, 8, 8);
		grid->setSpacing(4);

		QVector<int> defaults = compute_default_thresholds(m_combo_items.size());

		for (int i = 0; i < m_combo_items.size(); i++) {
			// Item name
			auto *name = new QLabel(m_combo_items[i], m_select_group);
			name->setProperty("class", "select_item");
			grid->addWidget(name, i, 0);

			// Range label (computed)
			auto *range = new QLabel(m_select_group);
			range->setProperty("class", "select_range");
			range->setMinimumWidth(80);
			m_select_range_labels.append(range);
			grid->addWidget(range, i, 1);

			// Boundary spinbox (all but last item)
			if (i < m_combo_items.size() - 1) {
				auto *spin = new QSpinBox(m_select_group);
				spin->setRange(0, 127);
				spin->setValue(defaults.value(i, 127));
				spin->setPrefix("\xe2\x89\xa4 "); // ≤
				spin->setToolTip(
					QString("Upper MIDI value boundary for \"%1\"")
						.arg(m_combo_items[i]));
				connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
					this, [this](int) { update_select_labels(); });
				m_select_boundary_spins.append(spin);
				grid->addWidget(spin, i, 2);
			} else {
				auto *end = new QLabel("\xe2\x89\xa4 127", m_select_group);
				end->setProperty("class", "select_range");
				grid->addWidget(end, i, 2);
			}
		}

		root->addWidget(m_select_group);
		update_select_labels();
	}

	// --- Options (always shown) ---
	{
		auto *opts_row = new QHBoxLayout();
		opts_row->setSpacing(12);

		m_invert_check = new QCheckBox("Invert", this);
		m_invert_check->setToolTip("Reverse the mapping direction");
		opts_row->addWidget(m_invert_check);

		m_enabled_check = new QCheckBox("Enabled", this);
		m_enabled_check->setChecked(true);
		m_enabled_check->setToolTip("Enable/disable this binding");
		opts_row->addWidget(m_enabled_check);

		opts_row->addStretch();
		root->addLayout(opts_row);
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

	m_close_btn = new QPushButton("Close", this);
	connect(m_close_btn, &QPushButton::clicked,
		this, &MidiControlPopup::close);

	btn_row->addWidget(m_learn_btn);
	btn_row->addWidget(m_apply_btn);
	btn_row->addStretch();
	btn_row->addWidget(m_close_btn);

	root->addLayout(btn_row);

	// --- MIDI Monitor ---
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
		monitor_layout->addWidget(m_monitor_clear_btn, 0, Qt::AlignRight);

		m_monitor_container->hide();
		root->addWidget(m_monitor_container);

		connect(m_monitor_toggle, &QPushButton::toggled,
			this, &MidiControlPopup::toggle_monitor);
	}

	setMinimumWidth(340);
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

// ---- Select Mapping Labels -------------------------------------------------

void MidiControlPopup::update_select_labels()
{
	int start = 0;
	for (int i = 0; i < m_select_range_labels.size(); i++) {
		int end;
		if (i < m_select_boundary_spins.size())
			end = m_select_boundary_spins[i]->value();
		else
			end = 127;

		m_select_range_labels[i]->setText(
			QString("MIDI %1\xe2\x80\x93%2").arg(start).arg(end));
		start = end + 1;
	}
}

// ---- Binding Selector Logic ------------------------------------------------

void MidiControlPopup::rebuild_binding_selector()
{
	m_binding_combo->blockSignals(true);
	m_binding_combo->clear();

	m_binding_indices = MidiRouter::instance()->binding_indices_for(
		m_widget_id, m_control_name);

	auto &all = MidiRouter::instance()->all_bindings();
	for (int i = 0; i < m_binding_indices.size(); i++) {
		int gi = m_binding_indices[i];
		const MidiBinding &b = all[gi];
		QString label = QString("Binding %1: Ch%2 CC%3")
			.arg(i + 1).arg(b.channel).arg(b.cc);
		if (!b.enabled)
			label += " (off)";
		m_binding_combo->addItem(label);
	}

	m_binding_combo->blockSignals(false);

	if (m_binding_indices.isEmpty()) {
		m_selected_local = -1;
		reset_ui_to_defaults();
		m_remove_btn->setEnabled(false);
		m_status_label->setText("No bindings \xe2\x80\x93 click Learn or + to add");
		m_status_label->setStyleSheet(
			"QLabel#status { color: #aaa; background-color: rgba(50, 50, 50, 150); "
			"border-radius: 4px; padding: 4px; font-size: 11px; }");
	} else {
		int sel = (m_selected_local >= 0 && m_selected_local < m_binding_indices.size())
			? m_selected_local : 0;
		m_binding_combo->setCurrentIndex(sel);
		select_binding(sel);
		m_remove_btn->setEnabled(true);
	}

	m_learn_btn->setChecked(false);
}

void MidiControlPopup::select_binding(int local_index)
{
	if (local_index < 0 || local_index >= m_binding_indices.size()) {
		m_selected_local = -1;
		return;
	}

	m_selected_local = local_index;
	int gi = m_binding_indices[local_index];
	const auto &all = MidiRouter::instance()->all_bindings();
	if (gi < 0 || gi >= all.size())
		return;

	load_binding_to_ui(all[gi]);
}

void MidiControlPopup::load_binding_to_ui(const MidiBinding &b)
{
	// MIDI source
	int combo_idx = m_device_combo->findData(b.device_index);
	if (combo_idx >= 0)
		m_device_combo->setCurrentIndex(combo_idx);
	m_channel_spin->setValue(b.channel);
	m_cc_spin->setValue(b.cc);

	// Mode-specific
	if (m_map_mode == MidiBinding::Range && m_range_group) {
		m_input_min_spin->setValue(b.input_min);
		m_input_max_spin->setValue(b.input_max);
		m_output_min_spin->setValue(b.output_min);
		m_output_max_spin->setValue(b.output_max);
	} else if ((m_map_mode == MidiBinding::Toggle ||
	            m_map_mode == MidiBinding::Trigger) && m_threshold_group) {
		m_threshold_spin->setValue(b.threshold);
	} else if (m_map_mode == MidiBinding::Select && m_select_group) {
		// Load thresholds into boundary spinboxes
		for (int i = 0; i < m_select_boundary_spins.size(); i++) {
			if (i < b.select_thresholds.size())
				m_select_boundary_spins[i]->setValue(b.select_thresholds[i]);
		}
		update_select_labels();
	}

	m_invert_check->setChecked(b.invert);
	m_enabled_check->setChecked(b.enabled);

	// Status
	QString dev_name = (b.device_index == -1) ? "Any" : m_device_combo->currentText();
	QString status_text = QString("Bound: %1 \xc2\xb7 Ch %2 \xc2\xb7 CC %3")
		.arg(dev_name).arg(b.channel).arg(b.cc);
	if (!b.enabled)
		status_text += " (disabled)";
	m_status_label->setText(status_text);

	if (b.enabled) {
		m_status_label->setStyleSheet(
			"QLabel#status { color: #8f8; background-color: rgba(30, 80, 30, 150); "
			"border-radius: 4px; padding: 4px; font-size: 11px; }");
	} else {
		m_status_label->setStyleSheet(
			"QLabel#status { color: #fa5; background-color: rgba(80, 60, 20, 150); "
			"border-radius: 4px; padding: 4px; font-size: 11px; }");
	}
}

void MidiControlPopup::reset_ui_to_defaults()
{
	m_device_combo->setCurrentIndex(0);
	m_channel_spin->setValue(0);
	m_cc_spin->setValue(0);

	if (m_map_mode == MidiBinding::Range && m_range_group) {
		m_input_min_spin->setValue(0);
		m_input_max_spin->setValue(127);
		m_output_min_spin->setValue(m_default_out_min);
		m_output_max_spin->setValue(m_default_out_max);
	} else if ((m_map_mode == MidiBinding::Toggle ||
	            m_map_mode == MidiBinding::Trigger) && m_threshold_group) {
		m_threshold_spin->setValue(63);
	} else if (m_map_mode == MidiBinding::Select && m_select_group) {
		QVector<int> defaults = compute_default_thresholds(m_combo_items.size());
		for (int i = 0; i < m_select_boundary_spins.size(); i++) {
			m_select_boundary_spins[i]->setValue(defaults.value(i, 127));
		}
		update_select_labels();
	}

	m_invert_check->setChecked(false);
	m_enabled_check->setChecked(true);
}

MidiBinding MidiControlPopup::build_binding_from_ui() const
{
	MidiBinding b;
	b.device_index = m_device_combo->currentData().toInt();
	b.channel = m_channel_spin->value();
	b.cc = m_cc_spin->value();
	b.type = MidiBinding::CC;
	b.widget_id = m_widget_id;
	b.control_name = m_control_name;
	b.map_mode = (MidiBinding::MapMode)m_map_mode;
	b.invert = m_invert_check->isChecked();
	b.enabled = m_enabled_check->isChecked();

	if (m_map_mode == MidiBinding::Range && m_range_group) {
		b.input_min = m_input_min_spin->value();
		b.input_max = m_input_max_spin->value();
		b.output_min = m_output_min_spin->value();
		b.output_max = m_output_max_spin->value();
	} else if ((m_map_mode == MidiBinding::Toggle ||
	            m_map_mode == MidiBinding::Trigger) && m_threshold_group) {
		b.threshold = m_threshold_spin->value();
	} else if (m_map_mode == MidiBinding::Select) {
		b.select_count = m_combo_items.size();
		for (auto *spin : m_select_boundary_spins)
			b.select_thresholds.append(spin->value());
	}

	return b;
}

void MidiControlPopup::show_near(QWidget *target)
{
	QPoint global_tl = target->mapToGlobal(
		QPoint(0, target->height() + 4));

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

// ---- Live Preview ----------------------------------------------------------

void MidiControlPopup::update_preview(int raw)
{
	MidiBinding temp = build_binding_from_ui();
	double mapped = temp.map_value(raw);

	QString text;
	switch (m_map_mode) {
	case MidiBinding::Range:
		text = QString("MIDI %1 \xe2\x86\x92 %2").arg(raw).arg(mapped, 0, 'f', 2);
		break;
	case MidiBinding::Select: {
		int idx = (int)qRound(mapped);
		QString item_name = (idx >= 0 && idx < m_combo_items.size())
			? QString("\"%1\"").arg(m_combo_items[idx])
			: QString("index %1").arg(idx);
		text = QString("MIDI %1 \xe2\x86\x92 %2").arg(raw).arg(item_name);
		break;
	}
	case MidiBinding::Toggle:
		text = QString("MIDI %1 \xe2\x86\x92 %2")
			.arg(raw)
			.arg(mapped > 0.5 ? "Toggle ON" : "(below threshold)");
		break;
	case MidiBinding::Trigger:
		text = QString("MIDI %1 \xe2\x86\x92 %2")
			.arg(raw)
			.arg(mapped > 0.5 ? "FIRE" : "(below threshold)");
		break;
	}

	m_preview_label->setText(QString("\xF0\x9F\x8E\xB5 %1").arg(text));
	m_preview_label->show();
}

// ---- Actions ---------------------------------------------------------------

void MidiControlPopup::on_add_clicked()
{
	MidiBinding b = build_binding_from_ui();
	MidiRouter::instance()->add_binding(b);
	m_selected_local = m_binding_indices.size();
	rebuild_binding_selector();
}

void MidiControlPopup::on_remove_clicked()
{
	if (m_selected_local < 0 || m_selected_local >= m_binding_indices.size())
		return;

	int gi = m_binding_indices[m_selected_local];
	MidiRouter::instance()->remove_binding_at(gi);

	if (m_selected_local > 0)
		m_selected_local--;
	rebuild_binding_selector();
}

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
	MidiBinding b = build_binding_from_ui();

	if (m_selected_local >= 0 && m_selected_local < m_binding_indices.size()) {
		// Update existing binding
		int gi = m_binding_indices[m_selected_local];
		MidiRouter::instance()->update_binding_at(gi, b);
	} else {
		// Add as new binding
		MidiRouter::instance()->add_binding(b);
		m_selected_local = m_binding_indices.size();
	}
	rebuild_binding_selector();
}

void MidiControlPopup::on_binding_learned(const MidiBinding &binding)
{
	if (binding.widget_id != m_widget_id ||
	    binding.control_name != m_control_name)
		return;

	// Don't add the binding yet — just update the UI source fields.
	// User clicks Apply to commit.
	int idx = m_device_combo->findData(binding.device_index);
	if (idx >= 0)
		m_device_combo->setCurrentIndex(idx);
	m_channel_spin->setValue(binding.channel);
	m_cc_spin->setValue(binding.cc);

	m_learn_btn->setChecked(false);

	m_status_label->setText("Learned \xe2\x80\x93 click Apply to save");
	m_status_label->setStyleSheet(
		"QLabel#status { color: #7cf; background-color: rgba(30, 60, 90, 150); "
		"border-radius: 4px; padding: 4px; font-size: 11px; font-weight: bold; }");
}

void MidiControlPopup::on_learn_cancelled()
{
	m_learn_btn->setChecked(false);
	rebuild_binding_selector();
}


// ---- MIDI Monitor & Raw Input ----------------------------------------------

void MidiControlPopup::on_raw_midi(int device, int status, int data1, int data2)
{
	int msg_type = status & 0xF0;
	int channel = status & 0x0F;

	// Live preview: check if this CC matches current UI settings
	if (msg_type == 0xB0) {
		int ui_device = m_device_combo->currentData().toInt();
		int ui_channel = m_channel_spin->value();
		int ui_cc = m_cc_spin->value();

		if ((ui_device == -1 || ui_device == device) &&
		    ui_channel == channel &&
		    ui_cc == data1) {
			update_preview(data2);
		}
	}

	// Monitor log (only when visible)
	if (!m_monitor_container->isVisible())
		return;

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

	QString line = QString("[%1] Dev%2 Ch%3  %4  %5")
		.arg(m_monitor_msg_count, 4)
		.arg(device)
		.arg(channel, 2)
		.arg(type_str, -10)
		.arg(detail);

	m_monitor_log->appendPlainText(line);
}

void MidiControlPopup::toggle_monitor(bool expanded)
{
	m_monitor_container->setVisible(expanded);
	m_monitor_toggle->setText(
		expanded
			? QString::fromUtf8("\xe2\x96\xbc MIDI Monitor")
			: QString::fromUtf8("\xe2\x96\xb6 MIDI Monitor"));

	if (expanded) {
		MidiRouter::instance()->open_all_devices();
		m_monitor_msg_count = 0;
	}

	adjustSize();
}

// ---- Event / Drag ----------------------------------------------------------

bool MidiControlPopup::event(QEvent *e)
{
	if (e->type() == QEvent::KeyPress) {
		auto *ke = dynamic_cast<QKeyEvent *>(e);
		if (ke->key() == Qt::Key_Escape) {
			close();
			return true;
		}
	}
	return QFrame::event(e);
}

void MidiControlPopup::mousePressEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton) {
		m_dragging = true;
		m_drag_offset = e->globalPosition().toPoint() - frameGeometry().topLeft();
		e->accept();
	}
}

void MidiControlPopup::mouseMoveEvent(QMouseEvent *e)
{
	if (m_dragging && (e->buttons() & Qt::LeftButton)) {
		move(e->globalPosition().toPoint() - m_drag_offset);
		e->accept();
	}
}

void MidiControlPopup::mouseReleaseEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton) {
		m_dragging = false;
		e->accept();
	}
}
