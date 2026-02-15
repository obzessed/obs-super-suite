// ============================================================================
// SuperWidget — Implementation
// ============================================================================

#include "super_widget.hpp"
#include "control_assign_popup.hpp"
#include "../core/control_registry.hpp"
#include "../core/control_types.hpp"
#include "../io/midi_adapter.hpp"

#include "../../utils/midi/midi_router.hpp"

#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QSlider>
#include <QDial>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QStyle>
#include <QTimer>

namespace super {

// ============================================================================
// SuperWidget
// ============================================================================

SuperWidget::SuperWidget(const QString &widget_id, QWidget *parent)
	: QWidget(parent), m_widget_id(widget_id)
{
	// Create our own MidiAdapter that shares the backend with MidiRouter
	m_midi_adapter = new MidiAdapter(this);
	m_midi_adapter->attach(MidiRouter::instance()->backend());

	setup_base_ui();
}

SuperWidget::~SuperWidget()
{
	// Close any open assign popup
	if (m_assign_popup)
		m_assign_popup->close();

	// Detach MIDI adapter
	if (m_midi_adapter)
		m_midi_adapter->detach();

	// Unregister all our ports from the global registry
	auto &reg = ControlRegistry::instance();
	for (auto it = m_registered_controls.cbegin();
		 it != m_registered_controls.cend(); ++it) {
		reg.destroy_port(m_widget_id + "." + it.key());
	}
}

// ---------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------

QString SuperWidget::widget_id() const { return m_widget_id; }
QWidget *SuperWidget::content_area() const { return m_content_area; }
QToolBar *SuperWidget::system_toolbar() const { return m_system_toolbar; }
QToolBar *SuperWidget::user_toolbar() const { return m_user_toolbar; }

// ---------------------------------------------------------------------------
// Base UI Setup
// ---------------------------------------------------------------------------

void SuperWidget::setup_base_ui()
{
	m_main_layout = new QVBoxLayout(this);
	m_main_layout->setContentsMargins(0, 0, 0, 0);
	m_main_layout->setSpacing(0);

	setup_system_toolbar();
	setup_user_toolbar();

	// Content area
	m_content_area = new QWidget(this);
	m_main_layout->addWidget(m_content_area, 1);

	// Monitor console (bottom, hidden by default)
	setup_monitor_console();

	// Assign overlay (hidden)
	m_overlay = new AssignOverlay(this);
	m_overlay->hide();
	connect(m_overlay, &AssignOverlay::control_clicked,
			this, &SuperWidget::on_control_clicked_for_assign);

	// Grab handle (always visible, top-right corner)
	setup_grab_handle();
}

// ---------------------------------------------------------------------------
// System Toolbar (Top)
// ---------------------------------------------------------------------------

void SuperWidget::setup_system_toolbar()
{
	m_system_toolbar = new QToolBar(this);
	m_system_toolbar->setIconSize(QSize(16, 16));
	m_system_toolbar->setMovable(false);
	m_system_toolbar->setFloatable(false);
	m_system_toolbar->setObjectName("system_toolbar");

	// --- Start Actions ---

	// Enable toggle
	m_enable_action = m_system_toolbar->addAction(
		QString::fromUtf8("⚡"));
	m_enable_action->setCheckable(true);
	m_enable_action->setToolTip("Enable/Disable External Control");
	connect(m_enable_action, &QAction::toggled, this, [this](bool on) {
		m_controls_enabled = on;
	});

	// Assign mode toggle
	m_assign_action = m_system_toolbar->addAction("Assign");
	m_assign_action->setCheckable(true);
	m_assign_action->setToolTip("Toggle Control Assign Mode");
	connect(m_assign_action, &QAction::toggled,
			this, &SuperWidget::toggle_assign_mode);

	// Flexible spacer between start and end zones
	auto *spacer = new QWidget(m_system_toolbar);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_system_toolbar->addWidget(spacer);

	// Console toggle (end zone)
	m_console_action = m_system_toolbar->addAction(
		QString::fromUtf8("🖥"));
	m_console_action->setCheckable(true);
	m_console_action->setToolTip("Toggle Monitor Console");
	connect(m_console_action, &QAction::toggled, this, [this](bool on) {
		set_console_visible(on);
	});

	m_main_layout->addWidget(m_system_toolbar);
}

// ---------------------------------------------------------------------------
// User Toolbar (Below System)
// ---------------------------------------------------------------------------

void SuperWidget::setup_user_toolbar()
{
	m_user_toolbar = new QToolBar(this);
	m_user_toolbar->setIconSize(QSize(16, 16));
	m_user_toolbar->setMovable(false);
	m_user_toolbar->setFloatable(false);
	m_user_toolbar->setObjectName("user_toolbar");
	m_user_toolbar->hide(); // Hidden by default until plugin adds actions

	m_main_layout->addWidget(m_user_toolbar);
}

// ---------------------------------------------------------------------------
// Grab Handle (Emergency Toolbar Restore)
// ---------------------------------------------------------------------------

void SuperWidget::setup_grab_handle()
{
	m_grab_handle = new GrabHandle(this);
	m_grab_handle->setFixedSize(12, 12);
	m_grab_handle->raise();  // Always on top
	update_grab_handle_position();

	connect(m_grab_handle, &GrabHandle::clicked, this, [this]() {
		// Toggle system toolbar visibility
		set_system_toolbar_visible(!is_system_toolbar_visible());
	});
}

// ---------------------------------------------------------------------------
// Monitor Console (Bottom Panel)
// ---------------------------------------------------------------------------

void SuperWidget::setup_monitor_console()
{
	m_console_container = new QWidget(this);
	auto *layout = new QVBoxLayout(m_console_container);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->setSpacing(4);

	// Header with title and clear
	auto *header = new QHBoxLayout();
	auto *title = new QLabel(
		QString::fromUtf8("🖥 Monitor Console"), m_console_container);
	title->setStyleSheet("color: #0f0; font-size: 11px; font-weight: bold;");
	header->addWidget(title);
	header->addStretch();

	m_console_clear_btn = new QPushButton("Clear", m_console_container);
	m_console_clear_btn->setFixedWidth(50);
	m_console_clear_btn->setStyleSheet(
		"QPushButton { background-color: rgba(40, 40, 50, 200); color: #aaa; "
		"border: 1px solid rgba(255,255,255,0.08); border-radius: 3px; "
		"padding: 2px 6px; font-size: 10px; }");
	header->addWidget(m_console_clear_btn);
	layout->addLayout(header);

	m_console_log = new QPlainTextEdit(m_console_container);
	m_console_log->setReadOnly(true);
	m_console_log->setMaximumBlockCount(500);
	m_console_log->setFixedHeight(140);
	m_console_log->setPlaceholderText("Monitor output will appear here...");
	m_console_log->setStyleSheet(
		"QPlainTextEdit { background-color: rgba(10, 10, 15, 220); "
		"color: #0f0; font-family: 'Consolas', 'Courier New', monospace; "
		"font-size: 10px; border: 1px solid rgba(0, 255, 0, 0.15); "
		"border-radius: 3px; }");
	layout->addWidget(m_console_log);

	connect(m_console_clear_btn, &QPushButton::clicked,
			m_console_log, &QPlainTextEdit::clear);

	m_console_container->setStyleSheet(
		"background-color: rgba(15, 15, 20, 230); "
		"border-top: 1px solid rgba(0, 255, 0, 0.2);");
	m_console_container->hide();
	m_main_layout->addWidget(m_console_container);
}

void SuperWidget::log_to_console(const QString &message)
{
	if (!m_console_log) return;
	m_console_log->appendPlainText(message);

	// Auto-show console if hidden? No, let user decide.
}

void SuperWidget::set_console_visible(bool visible)
{
	if (m_console_container)
		m_console_container->setVisible(visible);
	if (m_console_action && m_console_action->isChecked() != visible)
		m_console_action->setChecked(visible);
}

bool SuperWidget::is_console_visible() const
{
	return m_console_container && m_console_container->isVisible();
}

// ---------------------------------------------------------------------------
// Toolbar Visibility
// ---------------------------------------------------------------------------

void SuperWidget::set_system_toolbar_visible(bool visible)
{
	m_system_toolbar->setVisible(visible);
}

bool SuperWidget::is_system_toolbar_visible() const
{
	return m_system_toolbar->isVisible();
}

void SuperWidget::set_user_toolbar_visible(bool visible)
{
	m_user_toolbar->setVisible(visible);
}

bool SuperWidget::is_user_toolbar_visible() const
{
	return m_user_toolbar->isVisible();
}

// ---------------------------------------------------------------------------
// Toolbar Action Management
// ---------------------------------------------------------------------------

void SuperWidget::add_system_start_action(QAction *action)
{
	// Insert before the spacer
	auto actions = m_system_toolbar->actions();
	// Find the spacer widget action
	for (auto *a : actions) {
		auto *w = m_system_toolbar->widgetForAction(a);
		if (w && w->sizePolicy().horizontalPolicy() == QSizePolicy::Expanding) {
			m_system_toolbar->insertAction(a, action);
			return;
		}
	}
	m_system_toolbar->addAction(action);
}

void SuperWidget::add_system_end_action(QAction *action)
{
	m_system_toolbar->addAction(action);
}

void SuperWidget::add_user_action(QAction *action)
{
	m_user_toolbar->addAction(action);
	if (!m_user_toolbar->isVisible())
		m_user_toolbar->show();
}

// ---------------------------------------------------------------------------
// Control Registration
// ---------------------------------------------------------------------------

void SuperWidget::register_control(QWidget *control, const QString &name)
{
	QString ctrl_name = name.isEmpty() ? control->objectName() : name;
	if (ctrl_name.isEmpty())
		ctrl_name = QString("control_%1").arg(m_registered_controls.size());

	m_registered_controls[ctrl_name] = control;

	// Create a ControlPort in the global registry
	QString port_id = m_widget_id + "." + ctrl_name;
	auto &reg = ControlRegistry::instance();

	ControlPort *port = nullptr;
	if (reg.has_port(port_id)) {
		port = reg.find(port_id);
	} else {
		ControlDescriptor desc;
		desc.id = port_id;
		desc.display_name = ctrl_name;
		desc.group = m_widget_id;

		// Auto-detect type from widget
		if (auto *slider = qobject_cast<QSlider *>(control)) {
			desc.type = ControlType::Float; // Use Float to support arbitrary ranges (0-100) vs Range (0-1)
			desc.range_min = slider->minimum();
			desc.range_max = slider->maximum();
			desc.default_value = slider->value();
		} else if (auto *dial = qobject_cast<QDial *>(control)) {
			desc.type = ControlType::Float;
			desc.range_min = dial->minimum();
			desc.range_max = dial->maximum();
			desc.default_value = dial->value();
		} else if (auto *check = qobject_cast<QCheckBox *>(control)) {
			desc.type = ControlType::Toggle;
			desc.default_value = check->isChecked();
		} else if (auto *combo = qobject_cast<QComboBox *>(control)) {
			desc.type = ControlType::Select;
			desc.range_min = 0;
			desc.range_max = qMax(0, combo->count() - 1);
			desc.default_value = combo->currentIndex();
			// Populate options for metadata
			for (int i = 0; i < combo->count(); i++)
				desc.select_options << combo->itemText(i);
		} else if (auto *btn = qobject_cast<QPushButton *>(control)) {
			desc.type = btn->isCheckable() ? ControlType::Toggle
											: ControlType::Command;
			if (btn->isCheckable())
				desc.default_value = btn->isChecked();
		} else if (auto *spin = qobject_cast<QSpinBox *>(control)) {
			desc.type = ControlType::Float; // Treat Int as Float for uniformity in ranges
			desc.range_min = spin->minimum();
			desc.range_max = spin->maximum();
			desc.default_value = spin->value();
		} else if (auto *dspin = qobject_cast<QDoubleSpinBox *>(control)) {
			desc.type = ControlType::Float;
			desc.range_min = dspin->minimum();
			desc.range_max = dspin->maximum();
			desc.default_value = dspin->value();
		} else {
			desc.type = ControlType::Range;
		}

		port = reg.create_port(desc);
	}

	// Connect port value changes to widget updates
	if (port) {
		// Disconnect first to avoid duplicates if registered multiple times
		disconnect(port, &ControlPort::value_changed, this, nullptr);
		connect(port, &ControlPort::value_changed, this,
			[this, ctrl_name](const QVariant & /*val*/) {
				auto *port = ControlRegistry::instance().find(
					m_widget_id + "." + ctrl_name);
				if (port)
					on_control_value(ctrl_name, port->as_double());
			});
	}
}

void SuperWidget::unregister_control(const QString &name)
{
	m_registered_controls.remove(name);
	ControlRegistry::instance().destroy_port(m_widget_id + "." + name);
}

QStringList SuperWidget::control_names() const
{
	return m_registered_controls.keys();
}

// ---------------------------------------------------------------------------
// Assign Mode
// ---------------------------------------------------------------------------

bool SuperWidget::is_assign_active() const
{
	return m_overlay && m_overlay->is_active();
}

void SuperWidget::toggle_assign_mode(bool active)
{
	if (active) {
		m_overlay->set_controls(m_registered_controls);
		update_overlay_geometry();
		m_overlay->activate();
		m_overlay->raise();
	} else {
		m_overlay->deactivate();
	}
}

void SuperWidget::on_control_clicked_for_assign(const QString &control_name)
{
	if (!m_midi_adapter)
		return;

	// Close any existing popup
	if (m_assign_popup)
		m_assign_popup->close();

	// Determine port ID and map mode from control type
	QString port_id = m_widget_id + "." + control_name;
	auto *port = ControlRegistry::instance().find(port_id);
	if (!port)
		return;

	// Determine map mode from control type
	int map_mode = 0; // Range
	double out_min = 0.0;
	double out_max = 1.0;
	QStringList combo_items;

	auto it = m_registered_controls.find(control_name);
	QWidget *control = (it != m_registered_controls.end()) ? it.value() : nullptr;

	if (control) {
		if (auto *slider = qobject_cast<QSlider *>(control)) {
			map_mode = 0; // Range
			out_min = slider->minimum();
			out_max = slider->maximum();
		} else if (auto *dial = qobject_cast<QDial *>(control)) {
			map_mode = 0;
			out_min = dial->minimum();
			out_max = dial->maximum();
		} else if (auto *spin = qobject_cast<QSpinBox *>(control)) {
			map_mode = 0;
			out_min = spin->minimum();
			out_max = spin->maximum();
		} else if (auto *dspin = qobject_cast<QDoubleSpinBox *>(control)) {
			map_mode = 0;
			out_min = dspin->minimum();
			out_max = dspin->maximum();
		} else if (qobject_cast<QCheckBox *>(control)) {
			map_mode = 1; // Toggle
		} else if (auto *btn = qobject_cast<QPushButton *>(control)) {
			map_mode = btn->isCheckable() ? 1 : 2; // Toggle or Trigger
		} else if (auto *combo = qobject_cast<QComboBox *>(control)) {
			map_mode = 3; // Select
			out_min = 0;
			out_max = qMax(0, combo->count() - 1);
			for (int i = 0; i < combo->count(); i++)
				combo_items << combo->itemText(i);
		}
	}

	m_assign_popup = new ControlAssignPopup(
		port_id, control_name, map_mode,
		out_min, out_max, combo_items, m_midi_adapter, this);

	// Position near the clicked control
	if (control)
		m_assign_popup->show_near(control);
	else
		m_assign_popup->show();
}

// ---------------------------------------------------------------------------
// Default Control Value Handling
// ---------------------------------------------------------------------------

void SuperWidget::on_control_value(const QString &control_name, double value)
{
	auto it = m_registered_controls.find(control_name);
	if (it == m_registered_controls.end())
		return;

	QWidget *control = it.value();

	if (auto *slider = qobject_cast<QSlider *>(control)) {
		slider->setValue(static_cast<int>(qRound(value)));
	} else if (auto *dial = qobject_cast<QDial *>(control)) {
		dial->setValue(static_cast<int>(qRound(value)));
	} else if (auto *spin = qobject_cast<QSpinBox *>(control)) {
		spin->setValue(static_cast<int>(qRound(value)));
	} else if (auto *dspin = qobject_cast<QDoubleSpinBox *>(control)) {
		dspin->setValue(value);
	} else if (auto *combo = qobject_cast<QComboBox *>(control)) {
		int count = combo->count();
		if (count > 0) {
			int idx = std::clamp(static_cast<int>(qRound(value)), 0, count - 1);
			combo->setCurrentIndex(idx);
		}
	} else if (auto *check = qobject_cast<QCheckBox *>(control)) {
		check->setChecked(value > 0.5);
	} else if (auto *btn = qobject_cast<QPushButton *>(control)) {
		if (btn->isCheckable()) {
			btn->setChecked(value > 0.5);
		} else if (value > 0.5) {
			// Visual flash for OneShot/Trigger buttons
			QString original = btn->styleSheet();
			btn->setStyleSheet(
				original +
				" QPushButton { background-color: rgba(255, 200, 50, 220); color: #000; }");
			btn->click();
			QTimer::singleShot(120, btn, [btn, original]() {
				if (btn)
					btn->setStyleSheet(original);
			});
		}
	}
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

QJsonObject SuperWidget::save_state() const
{
	QJsonObject obj;
	obj["controls_enabled"] = m_controls_enabled;
	obj["system_toolbar_visible"] = is_system_toolbar_visible();
	obj["user_toolbar_visible"] = is_user_toolbar_visible();
	obj["console_visible"] = is_console_visible();

	// Save MIDI adapter bindings
	if (m_midi_adapter)
		obj["midi_adapter"] = m_midi_adapter->save();

	return obj;
}

void SuperWidget::load_state(const QJsonObject &state)
{
	if (state.contains("controls_enabled")) {
		m_controls_enabled = state["controls_enabled"].toBool(false);
		m_enable_action->setChecked(m_controls_enabled);
	}
	if (state.contains("system_toolbar_visible"))
		set_system_toolbar_visible(state["system_toolbar_visible"].toBool(true));
	if (state.contains("user_toolbar_visible"))
		set_user_toolbar_visible(state["user_toolbar_visible"].toBool(false));
	if (state.contains("console_visible"))
		set_console_visible(state["console_visible"].toBool(false));

	// Load MIDI adapter bindings
	if (state.contains("midi_adapter") && m_midi_adapter)
		m_midi_adapter->load(state["midi_adapter"].toObject());
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

void SuperWidget::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	update_overlay_geometry();
	update_grab_handle_position();
}

void SuperWidget::update_overlay_geometry()
{
	if (m_overlay && m_content_area)
		m_overlay->setGeometry(m_content_area->geometry());
}

void SuperWidget::update_grab_handle_position()
{
	if (m_grab_handle) {
		// Top-right corner, inset 4px
		m_grab_handle->move(width() - m_grab_handle->width() - 4, 4);
		m_grab_handle->raise();
	}
}

// ============================================================================
// AssignOverlay
// ============================================================================

AssignOverlay::AssignOverlay(QWidget *parent)
	: QWidget(parent)
{
	setMouseTracking(true);
	setAttribute(Qt::WA_TransparentForMouseEvents, false);
}

void AssignOverlay::set_controls(const QMap<QString, QWidget *> &controls)
{
	m_controls = controls;
}

void AssignOverlay::activate()
{
	m_active = true;
	m_hovered_control.clear();
	setCursor(Qt::ArrowCursor);
	show();
	update();
}

void AssignOverlay::deactivate()
{
	m_active = false;
	m_hovered_control.clear();
	setCursor(Qt::ArrowCursor);
	hide();
}

bool AssignOverlay::is_active() const { return m_active; }

QString AssignOverlay::find_control_at(const QPoint &pos) const
{
	QPoint global_pos = mapToGlobal(pos);
	for (auto it = m_controls.constBegin(); it != m_controls.constEnd(); ++it) {
		QWidget *ctrl = it.value();
		if (!ctrl || !ctrl->isVisible())
			continue;
		QPoint local = ctrl->mapFromGlobal(global_pos);
		if (ctrl->rect().contains(local))
			return it.key();
	}
	return {};
}

void AssignOverlay::paintEvent(QPaintEvent * /*event*/)
{
	if (!m_active)
		return;

	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	// Semi-transparent dark overlay
	p.fillRect(rect(), QColor(0, 0, 0, 100));

	// Draw highlights for each registered control
	for (auto it = m_controls.constBegin(); it != m_controls.constEnd(); ++it) {
		QWidget *ctrl = it.value();
		if (!ctrl || !ctrl->isVisible())
			continue;

		QPoint tl = ctrl->mapToGlobal(QPoint(0, 0));
		QPoint br = ctrl->mapToGlobal(QPoint(ctrl->width(), ctrl->height()));
		QRect r(mapFromGlobal(tl), mapFromGlobal(br));

		bool hovered = (it.key() == m_hovered_control);

		if (hovered) {
			p.setPen(QPen(QColor(80, 200, 255), 2));
			p.setBrush(QColor(80, 200, 255, 50));
		} else {
			p.setPen(QPen(QColor(80, 200, 255, 120), 1));
			p.setBrush(QColor(80, 200, 255, 20));
		}
		p.drawRoundedRect(r.adjusted(-2, -2, 2, 2), 4, 4);

		// Control name label
		QFont f = font();
		f.setPointSize(8);
		f.setBold(hovered);
		p.setFont(f);
		p.setPen(QColor(220, 240, 255));
		p.drawText(r.adjusted(0, -16, 0, -r.height()),
				   Qt::AlignLeft | Qt::AlignBottom, it.key());
	}
}

void AssignOverlay::mouseMoveEvent(QMouseEvent *event)
{
	if (!m_active)
		return;
	QString found = find_control_at(event->pos());
	if (found != m_hovered_control) {
		m_hovered_control = found;
		update();
	}
}

void AssignOverlay::mousePressEvent(QMouseEvent *event)
{
	if (!m_active || event->button() != Qt::LeftButton)
		return;
	QString found = find_control_at(event->pos());
	if (!found.isEmpty())
		emit control_clicked(found);
}

// ============================================================================
// GrabHandle — Tiny corner indicator for toolbar restore
// ============================================================================

GrabHandle::GrabHandle(QWidget *parent)
	: QWidget(parent)
{
	setCursor(Qt::PointingHandCursor);
	setAttribute(Qt::WA_TransparentForMouseEvents, false);
	setToolTip("Toggle System Toolbar");
}

void GrabHandle::paintEvent(QPaintEvent * /*event*/)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	QColor color = m_hovered ? QColor(80, 200, 255, 200)
							 : QColor(150, 150, 150, 100);
	p.setBrush(color);
	p.setPen(Qt::NoPen);

	// Draw a small circle
	p.drawEllipse(rect().adjusted(1, 1, -1, -1));
}

void GrabHandle::enterEvent(QEnterEvent * /*event*/)
{
	m_hovered = true;
	update();
}

void GrabHandle::leaveEvent(QEvent * /*event*/)
{
	m_hovered = false;
	update();
}

void GrabHandle::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton)
		emit clicked();
}

} // namespace super
