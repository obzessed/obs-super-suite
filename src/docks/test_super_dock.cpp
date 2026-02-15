#include "test_super_dock.hpp"

#include "../super/core/control_registry.hpp"
#include "../super/core/control_port.hpp"
#include "../super/core/control_variable.hpp"
#include "../super/core/control_filters.hpp"
#include "../super/core/animation.hpp"
#include "../super/io/midi_adapter.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QScrollArea>
#include <QTimer>
#include <QRandomGenerator>
#include <QJsonDocument>

TestSuperDock::TestSuperDock(QWidget *parent)
	: super::SuperWidget("test_super_dock", parent)
{
	// Create filter instances (shared_ptr for ControlPort pipeline)
	m_smooth_filter = std::make_shared<super::SmoothingFilter>(0.3);
	m_deadzone_filter = std::make_shared<super::DeadzoneFilter>(2.0);
	m_quantize_filter = std::make_shared<super::QuantizeFilter>(5.0);

	// Create variables in the ControlRegistry
	auto &reg = super::ControlRegistry::instance();

	m_session_counter = reg.create_variable(
		"test_super_dock.session_counter",
		super::ControlType::Int,
		super::PersistencePolicy::Session);

	m_persist_counter = reg.create_variable(
		"test_super_dock.persist_counter",
		super::ControlType::Int,
		super::PersistencePolicy::Persist);

	setup_ui();
	setup_user_toolbar_actions();
	setup_rhs_toolbar();

	// Listen to modifier changes
	connect(&reg, &super::ControlRegistry::modifier_changed,
		this, [this](const QString & /*mod_id*/, bool /*active*/) {
			refresh_modifier_display();
		});
}

// ---------------------------------------------------------------------------
// UI Setup
// ---------------------------------------------------------------------------

void TestSuperDock::setup_ui()
{
	// Outer layout: [scrollable content] [rhs toolbar]
	auto *outer = new QHBoxLayout(content_area());
	outer->setContentsMargins(0, 0, 0, 0);
	outer->setSpacing(0);

	// --- Scrollable content area ---
	auto *scroll = new QScrollArea(content_area());
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	outer->addWidget(scroll, 1);

	auto *container = new QWidget(scroll);
	scroll->setWidget(container);

	auto *layout = new QVBoxLayout(container);
	layout->setContentsMargins(8, 8, 8, 8);
	layout->setSpacing(6);

	// --- Volume (QSlider H) -------------------------------------------------
	{
		auto *group = new QGroupBox("Volume", container);
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
			this, &TestSuperDock::update_labels);
	}

	// --- Pan (QSlider, bipolar) ---------------------------------------------
	{
		auto *group = new QGroupBox("Pan", container);
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
			this, &TestSuperDock::update_labels);
	}

	// --- Send (QDial) -------------------------------------------------------
	{
		auto *group = new QGroupBox("Send", container);
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
			this, &TestSuperDock::update_labels);
	}

	// --- Delay & Gain (QSpinBox, QDoubleSpinBox) ----------------------------
	{
		auto *group = new QGroupBox("Parameters", container);
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

	// --- Mode (QComboBox) ---------------------------------------------------
	{
		auto *group = new QGroupBox("Mode", container);
		auto *row = new QHBoxLayout(group);

		m_mode_combo = new QComboBox(group);
		m_mode_combo->setObjectName("mode");
		m_mode_combo->addItems({"Normal", "Sidechain", "Ducking",
			"Gate", "Compressor", "Limiter"});
		row->addWidget(m_mode_combo, 1);

		layout->addWidget(group);

		connect(m_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, [this](int idx) {
				log_to_console(
					QString("[Mode] Changed to: %1 (index %2)")
						.arg(m_mode_combo->itemText(idx)).arg(idx));
			});
	}

	// --- Toggles (CheckBox + Checkable Buttons) -----------------------------
	{
		auto *group = new QGroupBox("Toggles", container);
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

		connect(m_solo_check, &QCheckBox::toggled, this, [this](bool on) {
			log_to_console(QString("[Solo] %1").arg(on ? "ON" : "OFF"));
		});
		connect(m_mute_btn, &QPushButton::toggled, this, [this](bool on) {
			log_to_console(QString("[Mute] %1").arg(on ? "ON" : "OFF"));
		});
		connect(m_rec_btn, &QPushButton::toggled, this, [this](bool on) {
			log_to_console(QString("[Rec] %1").arg(on ? "ON" : "OFF"));
		});
	}

	// --- Triggers (OneShot + Trigger + Toggle Button) -----------------------
	{
		auto *group = new QGroupBox("Triggers", container);
		auto *row = new QHBoxLayout(group);

		// OneShot (non-checkable, fires once per click)
		m_oneshot_btn = new QPushButton("OneShot", group);
		m_oneshot_btn->setObjectName("oneshot");
		m_oneshot_btn->setStyleSheet(
			"QPushButton { padding: 4px 12px; background-color: #3498db; color: white; }"
			"QPushButton:pressed { background-color: #2980b9; }");
		row->addWidget(m_oneshot_btn);

		m_oneshot_counter = new QLabel("0", group);
		m_oneshot_counter->setFixedWidth(24);
		m_oneshot_counter->setAlignment(Qt::AlignCenter);
		m_oneshot_counter->setStyleSheet(
			"color: #7cf; font-size: 11px; font-weight: bold;");
		m_oneshot_counter->setToolTip("OneShot fire count");
		row->addWidget(m_oneshot_counter);

		row->addSpacing(8);

		// Trigger (non-checkable, used for testing continuous fire via MIDI)
		m_sample_trigger_btn = new QPushButton("Trigger", group);
		m_sample_trigger_btn->setObjectName("sample_trigger");
		m_sample_trigger_btn->setStyleSheet(
			"QPushButton { padding: 4px 12px; background-color: #8e44ad; color: white; }"
			"QPushButton:pressed { background-color: #7d3c98; }");
		m_sample_trigger_btn->setToolTip(
			"Trigger — fires once per click; test continuous fire via MIDI");
		row->addWidget(m_sample_trigger_btn);

		m_sample_trigger_counter = new QLabel("0", group);
		m_sample_trigger_counter->setFixedWidth(24);
		m_sample_trigger_counter->setAlignment(Qt::AlignCenter);
		m_sample_trigger_counter->setStyleSheet(
			"color: #d8b; font-size: 11px; font-weight: bold;");
		m_sample_trigger_counter->setToolTip("Trigger fire count");
		row->addWidget(m_sample_trigger_counter);

		row->addSpacing(8);

		// Arm — a toggle (checkable) button in the triggers group
		m_arm_btn = new QPushButton("Arm", group);
		m_arm_btn->setObjectName("arm");
		m_arm_btn->setCheckable(true);
		m_arm_btn->setStyleSheet(
			"QPushButton { padding: 4px 12px; }"
			"QPushButton:checked { background-color: #d35400; color: white; }");
		m_arm_btn->setToolTip(
			"Toggle button (checkable) — demonstrates a non-oneshot button in triggers");
		row->addWidget(m_arm_btn);

		row->addStretch();
		layout->addWidget(group);

		connect(m_oneshot_btn, &QPushButton::clicked,
			this, &TestSuperDock::on_oneshot_fired);
		connect(m_sample_trigger_btn, &QPushButton::clicked,
			this, &TestSuperDock::on_sample_trigger_fired);
		connect(m_arm_btn, &QPushButton::toggled, this, [this](bool on) {
			log_to_console(QString("[Arm] %1").arg(on ? "ARMED" : "DISARMED"));
		});
	}

	// --- Modifiers ----------------------------------------------------------
	{
		auto *group = new QGroupBox("Modifiers", container);
		auto *row = new QHBoxLayout(group);

		m_shift_btn = new QPushButton("Shift", group);
		m_shift_btn->setCheckable(true);
		m_shift_btn->setStyleSheet(
			"QPushButton { padding: 4px 12px; }"
			"QPushButton:checked { background-color: #2980b9; color: white; }");
		m_shift_btn->setToolTip("Toggle Shift modifier layer");
		row->addWidget(m_shift_btn);

		m_alt_btn = new QPushButton("Alt", group);
		m_alt_btn->setCheckable(true);
		m_alt_btn->setStyleSheet(
			"QPushButton { padding: 4px 12px; }"
			"QPushButton:checked { background-color: #27ae60; color: white; }");
		m_alt_btn->setToolTip("Toggle Alt modifier layer");
		row->addWidget(m_alt_btn);

		m_modifier_status = new QLabel("Active: none", group);
		m_modifier_status->setStyleSheet(
			"color: #aaa; font-size: 10px; font-style: italic;");
		row->addWidget(m_modifier_status, 1);

		layout->addWidget(group);

		connect(m_shift_btn, &QPushButton::toggled, this, [this](bool on) {
			super::ControlRegistry::instance().set_modifier("shift", on);
			log_to_console(QString("[Modifier] Shift %1").arg(on ? "ON" : "OFF"));
		});
		connect(m_alt_btn, &QPushButton::toggled, this, [this](bool on) {
			super::ControlRegistry::instance().set_modifier("alt", on);
			log_to_console(QString("[Modifier] Alt %1").arg(on ? "ON" : "OFF"));
		});
	}

	// --- ControlVariables ---------------------------------------------------
	{
		auto *group = new QGroupBox("Variables", container);
		auto *form = new QFormLayout(group);
		form->setContentsMargins(8, 4, 8, 8);
		form->setSpacing(4);

		// Session counter (lost on restart)
		auto *session_row = new QHBoxLayout();
		m_session_var_label = new QLabel("0", group);
		m_session_var_label->setFixedWidth(40);
		m_session_var_label->setAlignment(Qt::AlignCenter);
		m_session_var_label->setStyleSheet(
			"font-weight: bold; color: #e67e22; font-size: 12px; "
			"background: rgba(230, 126, 34, 30); border-radius: 3px; padding: 2px;");

		m_session_inc_btn = new QPushButton("+", group);
		m_session_inc_btn->setFixedWidth(28);
		m_session_inc_btn->setToolTip("Increment session counter");

		session_row->addWidget(m_session_var_label);
		session_row->addWidget(m_session_inc_btn);
		session_row->addStretch();
		form->addRow("Session:", session_row);

		// Persist counter (survives restart)
		auto *persist_row = new QHBoxLayout();
		m_persist_var_label = new QLabel("0", group);
		m_persist_var_label->setFixedWidth(40);
		m_persist_var_label->setAlignment(Qt::AlignCenter);
		m_persist_var_label->setStyleSheet(
			"font-weight: bold; color: #2ecc71; font-size: 12px; "
			"background: rgba(46, 204, 113, 30); border-radius: 3px; padding: 2px;");

		m_persist_inc_btn = new QPushButton("+", group);
		m_persist_inc_btn->setFixedWidth(28);
		m_persist_inc_btn->setToolTip("Increment persistent counter");

		persist_row->addWidget(m_persist_var_label);
		persist_row->addWidget(m_persist_inc_btn);
		persist_row->addStretch();
		form->addRow("Persist:", persist_row);

		// Reset button
		m_vars_reset_btn = new QPushButton("Reset Both", group);
		m_vars_reset_btn->setToolTip("Reset both counters to 0");
		form->addRow("", m_vars_reset_btn);

		layout->addWidget(group);

		connect(m_session_inc_btn, &QPushButton::clicked, this, [this]() {
			if (m_session_counter) {
				int v = m_session_counter->as_int() + 1;
				m_session_counter->set_value(QVariant(v));
				refresh_variable_display();
				log_to_console(QString("[Var] Session counter -> %1").arg(v));
			}
		});
		connect(m_persist_inc_btn, &QPushButton::clicked, this, [this]() {
			if (m_persist_counter) {
				int v = m_persist_counter->as_int() + 1;
				m_persist_counter->set_value(QVariant(v));
				refresh_variable_display();
				log_to_console(QString("[Var] Persist counter -> %1").arg(v));
			}
		});
		connect(m_vars_reset_btn, &QPushButton::clicked, this, [this]() {
			if (m_session_counter)
				m_session_counter->set_value(QVariant(0));
			if (m_persist_counter)
				m_persist_counter->set_value(QVariant(0));
			refresh_variable_display();
			log_to_console("[Var] Both counters reset to 0");
		});
	}

	// --- Filter Pipeline Demo -----------------------------------------------
	{
		auto *group = new QGroupBox("Filter Pipeline (Volume)", container);
		auto *row = new QHBoxLayout(group);

		m_smooth_check = new QCheckBox("Smooth", group);
		m_smooth_check->setToolTip(
			"Exponential smoothing filter (factor=0.3)");
		row->addWidget(m_smooth_check);

		m_deadzone_check = new QCheckBox("Deadzone", group);
		m_deadzone_check->setToolTip(
			"Ignores changes smaller than 2 units");
		row->addWidget(m_deadzone_check);

		m_quantize_check = new QCheckBox("Quantize", group);
		m_quantize_check->setToolTip(
			"Snaps to nearest 5 (e.g. 0, 5, 10, 15...)");
		row->addWidget(m_quantize_check);

		layout->addWidget(group);

		auto apply_filters = [this]() {
			QString port_id = "test_super_dock.volume";
			auto *port = super::ControlRegistry::instance().find(port_id);
			if (!port) return;

			port->clear_filters();
			QStringList active;

			if (m_smooth_check->isChecked()) {
				port->add_filter(m_smooth_filter);
				active << "Smooth";
			}
			if (m_deadzone_check->isChecked()) {
				port->add_filter(m_deadzone_filter);
				active << "Deadzone";
			}
			if (m_quantize_check->isChecked()) {
				port->add_filter(m_quantize_filter);
				active << "Quantize";
			}

			if (active.isEmpty())
				log_to_console("[Filters] Volume: none");
			else
				log_to_console(
					QString("[Filters] Volume: %1").arg(active.join(" > ")));
		};

		connect(m_smooth_check, &QCheckBox::toggled, this, apply_filters);
		connect(m_deadzone_check, &QCheckBox::toggled, this, apply_filters);
		connect(m_quantize_check, &QCheckBox::toggled, this, apply_filters);
	}

	// --- Activity Indicator -------------------------------------------------
	{
		m_activity_bar = new QProgressBar(container);
		m_activity_bar->setRange(0, 100);
		m_activity_bar->setValue(0);
		m_activity_bar->setTextVisible(false);
		m_activity_bar->setFixedHeight(4);
		m_activity_bar->setStyleSheet(
			"QProgressBar { background-color: rgba(30,30,40,200); border: none; border-radius: 2px; }"
			"QProgressBar::chunk { background-color: qlineargradient("
			"x1:0, y1:0, x2:1, y2:0, stop:0 #3498db, stop:1 #2ecc71); border-radius: 2px; }");
		layout->addWidget(m_activity_bar);
	}

	layout->addStretch();

	// --- Register all controls with the Universal Control API ---------------
	register_control(m_volume_slider, "volume");
	register_control(m_pan_slider, "pan");
	register_control(m_send_dial, "send");
	register_control(m_delay_spin, "delay");
	register_control(m_gain_spin, "gain");
	register_control(m_mode_combo, "mode");
	register_control(m_solo_check, "solo");
	register_control(m_mute_btn, "mute");
	register_control(m_rec_btn, "rec");
	register_control(m_oneshot_btn, "oneshot");
	register_control(m_sample_trigger_btn, "sample_trigger");
	register_control(m_arm_btn, "arm");

	// Connect volume to activity bar
	connect(m_volume_slider, &QSlider::valueChanged, this, [this](int val) {
		m_activity_bar->setValue(val);
	});
	m_activity_bar->setValue(m_volume_slider->value());

	// Initial display
	refresh_variable_display();
	refresh_modifier_display();
}

// ---------------------------------------------------------------------------
// RHS Toolbar (vertical, right side of the content area)
// ---------------------------------------------------------------------------

void TestSuperDock::setup_rhs_toolbar()
{
	m_rhs_toolbar = new QToolBar(content_area());
	m_rhs_toolbar->setOrientation(Qt::Vertical);
	m_rhs_toolbar->setIconSize(QSize(16, 16));
	m_rhs_toolbar->setMovable(false);
	m_rhs_toolbar->setFloatable(false);
	m_rhs_toolbar->setObjectName("rhs_toolbar");
	m_rhs_toolbar->setStyleSheet(
		"QToolBar { background: rgba(30, 30, 40, 200); border-left: 1px solid rgba(255,255,255,0.06); }"
		"QToolButton { color: #bbb; padding: 4px; margin: 1px; border-radius: 3px; font-size: 10px; }"
		"QToolButton:hover { background: rgba(80, 120, 200, 150); color: #fff; }"
		"QToolButton:checked { background: rgba(50, 120, 200, 200); color: #fff; }");

	auto *snap_action = m_rhs_toolbar->addAction(
		QString::fromUtf8("\xF0\x9F\x93\xB7"));
	snap_action->setToolTip("Capture snapshot");
	connect(snap_action, &QAction::triggered, this, [this]() {
		m_saved_snapshot =
			super::ControlRegistry::instance().capture_snapshot();
		log_to_console(QString("[Snap] Captured %1 ports")
			.arg(m_saved_snapshot.size()));
	});

	auto *restore_action = m_rhs_toolbar->addAction(
		QString::fromUtf8("\xe2\x86\xa9"));
	restore_action->setToolTip("Restore snapshot");
	connect(restore_action, &QAction::triggered, this, [this]() {
		if (m_saved_snapshot.isEmpty()) {
			log_to_console("[Snap] No snapshot saved yet");
			return;
		}
		super::ControlRegistry::instance().restore_snapshot(m_saved_snapshot);
		log_to_console(QString("[Snap] Restored %1 ports")
			.arg(m_saved_snapshot.size()));
	});

	m_rhs_toolbar->addSeparator();

	auto *anim_action = m_rhs_toolbar->addAction(
		QString::fromUtf8("\xF0\x9F\x8E\xAC"));
	anim_action->setToolTip("Animate volume (OutBounce)");
	connect(anim_action, &QAction::triggered, this, [this]() {
		auto *port = super::ControlRegistry::instance().find(
			"test_super_dock.volume");
		if (!port) return;

		if (m_current_tween_handle >= 0) {
			super::TweenManager::instance().cancel(m_current_tween_handle);
			m_current_tween_handle = -1;
		}

		double target = (port->as_double() < 50.0) ? 100.0 : 0.0;
		log_to_console(
			QString("[Anim] Tweening volume -> %1 (OutBounce)")
				.arg(target, 0, 'f', 0));

		m_current_tween_handle =
			super::TweenManager::instance().animate_port(
				port, target, 1000, QEasingCurve::OutBounce);
	});

	auto *soft_action = m_rhs_toolbar->addAction("ST");
	soft_action->setCheckable(true);
	soft_action->setToolTip("Toggle soft takeover on Volume");
	connect(soft_action, &QAction::toggled, this, [this](bool on) {
		auto *port = super::ControlRegistry::instance().find(
			"test_super_dock.volume");
		if (port) {
			port->set_soft_takeover(on);
			log_to_console(
				QString("[SoftTO] Volume: %1").arg(on ? "ON" : "OFF"));
		}
	});

	m_rhs_toolbar->addSeparator();

	auto *log_action = m_rhs_toolbar->addAction(
		QString::fromUtf8("\xF0\x9F\x93\x8B"));
	log_action->setToolTip("Log state to console");
	connect(log_action, &QAction::triggered, this, [this]() {
		log_to_console("--- State Dump ---");
		log_to_console(QString("  Volume: %1").arg(m_volume_slider->value()));
		log_to_console(QString("  Pan: %1").arg(m_pan_slider->value()));
		log_to_console(QString("  Send: %1").arg(m_send_dial->value()));
		log_to_console(QString("  Delay: %1 ms").arg(m_delay_spin->value()));
		log_to_console(QString("  Gain: %1 dB")
			.arg(m_gain_spin->value(), 0, 'f', 1));
		log_to_console(QString("  Mode: %1").arg(m_mode_combo->currentText()));
		log_to_console(QString("  Solo: %1")
			.arg(m_solo_check->isChecked() ? "ON" : "OFF"));
		log_to_console(QString("  Mute: %1")
			.arg(m_mute_btn->isChecked() ? "ON" : "OFF"));
		log_to_console(QString("  Rec: %1")
			.arg(m_rec_btn->isChecked() ? "ON" : "OFF"));
		log_to_console(QString("  Arm: %1")
			.arg(m_arm_btn->isChecked() ? "ON" : "OFF"));
		log_to_console(QString("  OneShot fires: %1").arg(m_oneshot_count));
		log_to_console(QString("  Trigger fires: %1")
			.arg(m_sample_trigger_count));

		// Modifiers
		auto mods = super::ControlRegistry::instance().active_modifiers();
		log_to_console(QString("  Modifiers: %1")
			.arg(mods.isEmpty() ? "none" : mods.join(", ")));

		// Variables
		log_to_console(QString("  Session var: %1")
			.arg(m_session_counter ? m_session_counter->as_int() : 0));
		log_to_console(QString("  Persist var: %1")
			.arg(m_persist_counter ? m_persist_counter->as_int() : 0));
	});

	// Add RHS toolbar to the outer layout
	auto *outer = qobject_cast<QHBoxLayout *>(content_area()->layout());
	if (outer)
		outer->addWidget(m_rhs_toolbar);
}

// ---------------------------------------------------------------------------
// User Toolbar Actions (top bar)
// ---------------------------------------------------------------------------

void TestSuperDock::setup_user_toolbar_actions()
{
	// --- Reset ---
	auto *reset_action = new QAction(
		QString::fromUtf8("\xe2\x86\xba Reset"), this);
	reset_action->setToolTip("Reset all controls to defaults");
	connect(reset_action, &QAction::triggered, this, [this]() {
		m_volume_slider->setValue(80);
		m_pan_slider->setValue(0);
		m_send_dial->setValue(0);
		m_delay_spin->setValue(0);
		m_gain_spin->setValue(0.0);
		m_mode_combo->setCurrentIndex(0);
		m_solo_check->setChecked(false);
		m_mute_btn->setChecked(false);
		m_rec_btn->setChecked(false);
		m_arm_btn->setChecked(false);
		m_oneshot_count = 0;
		m_oneshot_counter->setText("0");
		m_sample_trigger_count = 0;
		m_sample_trigger_counter->setText("0");
		log_to_console("[Reset] All controls reset to defaults");
	});
	add_user_action(reset_action);

	// --- Random ---
	auto *random_action = new QAction(
		QString::fromUtf8("\xF0\x9F\x8E\xB2 Random"), this);
	random_action->setToolTip("Randomize all continuous controls");
	connect(random_action, &QAction::triggered, this, [this]() {
		m_volume_slider->setValue(QRandomGenerator::global()->bounded(101));
		m_pan_slider->setValue(QRandomGenerator::global()->bounded(-100, 101));
		m_send_dial->setValue(QRandomGenerator::global()->bounded(101));
		m_delay_spin->setValue(QRandomGenerator::global()->bounded(501));
		m_gain_spin->setValue(
			(QRandomGenerator::global()->bounded(481) - 240) / 10.0);
		m_mode_combo->setCurrentIndex(
			QRandomGenerator::global()->bounded(m_mode_combo->count()));
		log_to_console("[Random] Controls randomized");
	});
	add_user_action(random_action);
}

// ---------------------------------------------------------------------------
// Trigger Handlers
// ---------------------------------------------------------------------------

void TestSuperDock::on_oneshot_fired()
{
	m_oneshot_count++;
	m_oneshot_counter->setText(QString::number(m_oneshot_count));
	log_to_console(QString("[OneShot] Fire #%1").arg(m_oneshot_count));
}

void TestSuperDock::on_sample_trigger_fired()
{
	m_sample_trigger_count++;
	m_sample_trigger_counter->setText(
		QString::number(m_sample_trigger_count));
	log_to_console(
		QString("[Trigger] Fire #%1").arg(m_sample_trigger_count));
}

// ---------------------------------------------------------------------------
// Display Updates
// ---------------------------------------------------------------------------

void TestSuperDock::update_labels()
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

void TestSuperDock::refresh_variable_display()
{
	if (m_session_counter && m_session_var_label)
		m_session_var_label->setText(
			QString::number(m_session_counter->as_int()));
	if (m_persist_counter && m_persist_var_label)
		m_persist_var_label->setText(
			QString::number(m_persist_counter->as_int()));
}

void TestSuperDock::refresh_modifier_display()
{
	auto mods = super::ControlRegistry::instance().active_modifiers();
	if (mods.isEmpty())
		m_modifier_status->setText("Active: none");
	else
		m_modifier_status->setText(
			QString("Active: %1").arg(mods.join(", ")));

	// Sync button states (might be changed externally)
	bool shift = super::ControlRegistry::instance().modifier("shift");
	bool alt = super::ControlRegistry::instance().modifier("alt");
	m_shift_btn->blockSignals(true);
	m_shift_btn->setChecked(shift);
	m_shift_btn->blockSignals(false);
	m_alt_btn->blockSignals(true);
	m_alt_btn->setChecked(alt);
	m_alt_btn->blockSignals(false);
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

QJsonObject TestSuperDock::save_state() const
{
	QJsonObject obj = super::SuperWidget::save_state();
	obj["volume"] = m_volume_slider->value();
	obj["pan"] = m_pan_slider->value();
	obj["send"] = m_send_dial->value();
	obj["delay"] = m_delay_spin->value();
	obj["gain"] = m_gain_spin->value();
	obj["mode"] = m_mode_combo->currentIndex();
	obj["solo"] = m_solo_check->isChecked();
	obj["muted"] = m_mute_btn->isChecked();
	obj["rec"] = m_rec_btn->isChecked();
	obj["arm"] = m_arm_btn->isChecked();

	// Filter states
	obj["filter_smooth"] = m_smooth_check->isChecked();
	obj["filter_deadzone"] = m_deadzone_check->isChecked();
	obj["filter_quantize"] = m_quantize_check->isChecked();

	// Trigger counts
	obj["oneshot_count"] = m_oneshot_count;
	obj["sample_trigger_count"] = m_sample_trigger_count;

	return obj;
}

void TestSuperDock::load_state(const QJsonObject &state)
{
	super::SuperWidget::load_state(state);

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
	if (state.contains("arm"))
		m_arm_btn->setChecked(state["arm"].toBool(false));

	// Restore filter states
	if (state.contains("filter_smooth"))
		m_smooth_check->setChecked(state["filter_smooth"].toBool(false));
	if (state.contains("filter_deadzone"))
		m_deadzone_check->setChecked(
			state["filter_deadzone"].toBool(false));
	if (state.contains("filter_quantize"))
		m_quantize_check->setChecked(
			state["filter_quantize"].toBool(false));

	// Restore trigger counts
	if (state.contains("oneshot_count")) {
		m_oneshot_count = state["oneshot_count"].toInt(0);
		m_oneshot_counter->setText(QString::number(m_oneshot_count));
	}
	if (state.contains("sample_trigger_count")) {
		m_sample_trigger_count =
			state["sample_trigger_count"].toInt(0);
		m_sample_trigger_counter->setText(
			QString::number(m_sample_trigger_count));
	}

	update_labels();
	refresh_variable_display();
	refresh_modifier_display();
}
