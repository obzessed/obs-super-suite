#include "persistable_widget.hpp"
#include "midi/midi_router.hpp"
#include "midi/midi_control_popup.hpp"

#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QSlider>
#include <QPushButton>
#include <QTimer>

// ============================================================================
// PersistableWidget
// ============================================================================

PersistableWidget::PersistableWidget(const QString &widget_id, QWidget *parent)
	: QWidget(parent), m_widget_id(widget_id)
{
	setup_base_ui();

	// Connect to MIDI router for CC dispatch
	auto *router = MidiRouter::instance();

	connect(router, &MidiRouter::midi_cc_received, this,
		[this](const QString &wid, const QString &ctl, int val) {
			if (m_midi_enabled && wid == m_widget_id)
				on_midi_cc(ctl, val);
		});


}

PersistableWidget::~PersistableWidget()
{
}

QString PersistableWidget::widget_id() const
{
	return m_widget_id;
}

QWidget *PersistableWidget::content_area() const
{
	return m_content_area;
}

QToolBar *PersistableWidget::toolbar() const
{
	return m_toolbar;
}

// ---------------------------------------------------------------------------
// Base UI setup
// ---------------------------------------------------------------------------

void PersistableWidget::setup_base_ui()
{
	m_main_layout = new QVBoxLayout(this);
	m_main_layout->setContentsMargins(0, 0, 0, 0);
	m_main_layout->setSpacing(0);

	// --- Toolbar ---
	m_toolbar = new QToolBar(this);
	m_toolbar->setIconSize(QSize(16, 16));
	m_toolbar->setMovable(false);
	m_toolbar->setFloatable(false);

	// MIDI enable toggle (dock level)
	m_midi_enable_action = m_toolbar->addAction(QString::fromUtf8("🎹"));
	m_midi_enable_action->setCheckable(true);
	m_midi_enable_action->setToolTip("Enable/Disable MIDI Control");
	connect(m_midi_enable_action, &QAction::toggled,
		this, &PersistableWidget::set_midi_enabled);

	// MIDI assign mode toggle
	m_midi_assign_action = m_toolbar->addAction("Assign");
	m_midi_assign_action->setCheckable(true);
	m_midi_assign_action->setToolTip("Toggle MIDI Assign Mode");
	connect(m_midi_assign_action, &QAction::toggled,
		this, &PersistableWidget::toggle_midi_assign);

	m_main_layout->addWidget(m_toolbar);

	// --- Content area ---
	m_content_area = new QWidget(this);
	m_main_layout->addWidget(m_content_area, 1);

	// --- Overlay (hidden, parented to *this*, positioned over content area) ---
	m_overlay = new MidiAssignOverlay(this);
	m_overlay->hide();

	connect(m_overlay, &MidiAssignOverlay::control_clicked,
		this, &PersistableWidget::on_control_clicked_for_learn);
}

// ---------------------------------------------------------------------------
// MIDI assign toggle
// ---------------------------------------------------------------------------

void PersistableWidget::toggle_midi_assign(bool active)
{
	if (active) {
		// Open all MIDI devices so we can receive messages
		MidiRouter::instance()->open_all_devices();

		m_overlay->set_controls(m_midi_controls);
		update_overlay_geometry();
		m_overlay->activate();
		m_overlay->raise();
	} else {
		m_overlay->deactivate();
		if (MidiRouter::instance()->is_learning())
			MidiRouter::instance()->cancel_learn();
	}
}

void PersistableWidget::set_midi_enabled(bool enabled)
{
	m_midi_enabled = enabled;

	// Sync the toolbar toggle without re-triggering
	if (m_midi_enable_action->isChecked() != enabled)
		m_midi_enable_action->setChecked(enabled);

	if (enabled) {
		MidiRouter::instance()->open_all_devices();
	}
}

bool PersistableWidget::is_midi_enabled() const
{
	return m_midi_enabled;
}

void PersistableWidget::on_control_clicked_for_learn(const QString &control_name)
{
	// Find the target control widget to position the popup near it
	QWidget *target = m_midi_controls.value(control_name, nullptr);
	if (!target)
		return;

	// Determine the output range from the control itself
	int out_min = 0, out_max = 127;
	if (auto *slider = qobject_cast<QSlider *>(target)) {
		out_min = slider->minimum();
		out_max = slider->maximum();
	} else if (auto *btn = qobject_cast<QPushButton *>(target)) {
		Q_UNUSED(btn);
		out_min = 0;
		out_max = 127; // threshold-based toggle
	}

	auto *popup = new MidiControlPopup(
		m_widget_id, control_name, out_min, out_max, this);

	// When the popup closes, update the overlay status to reflect changes
	connect(popup, &MidiControlPopup::closed, this, [this]() {
		if (m_overlay->is_active()) {
			m_overlay->show_status("Click a control to assign MIDI");
			m_overlay->update();
		}
	});

	popup->show_near(target);
}

void PersistableWidget::on_binding_learned()
{
	// The popup handles its own UI updates now.
	// This is kept for any future non-popup learn flows.
}

void PersistableWidget::on_learn_cancelled()
{
	// The popup handles learn cancellation display.
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

void PersistableWidget::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	update_overlay_geometry();
}

void PersistableWidget::update_overlay_geometry()
{
	if (m_overlay && m_content_area) {
		m_overlay->setGeometry(m_content_area->geometry());
	}
}

// ---------------------------------------------------------------------------
// Persistence (defaults — no-op)
// ---------------------------------------------------------------------------

QJsonObject PersistableWidget::save_state() const
{
	QJsonObject obj;
	obj["midi_enabled"] = m_midi_enabled;
	return obj;
}

void PersistableWidget::load_state(const QJsonObject &state)
{
	if (state.contains("midi_enabled")) {
		set_midi_enabled(state["midi_enabled"].toBool(false));
	}
}

// ---------------------------------------------------------------------------
// MIDI control registration
// ---------------------------------------------------------------------------

void PersistableWidget::register_midi_control(QWidget *control, const QString &name)
{
	QString ctrl_name = name.isEmpty() ? control->objectName() : name;
	if (ctrl_name.isEmpty()) {
		ctrl_name = QString("control_%1").arg(m_midi_controls.size());
	}
	m_midi_controls[ctrl_name] = control;
}

void PersistableWidget::unregister_midi_control(const QString &name)
{
	m_midi_controls.remove(name);
}

QStringList PersistableWidget::midi_control_names() const
{
	return m_midi_controls.keys();
}

// ---------------------------------------------------------------------------
// Default MIDI CC handling
// ---------------------------------------------------------------------------

void PersistableWidget::on_midi_cc(const QString &control_name, int value)
{
	// The value is already mapped by MidiRouter through the binding's
	// input/output ranges. We just set it directly on the control.
	auto it = m_midi_controls.find(control_name);
	if (it == m_midi_controls.end())
		return;

	QWidget *control = it.value();

	if (auto *slider = qobject_cast<QSlider *>(control)) {
		slider->setValue(value);
	} else if (auto *btn = qobject_cast<QPushButton *>(control)) {
		if (btn->isCheckable()) {
			btn->setChecked(value > 63);
		} else if (value > 63) {
			btn->click();
		}
	}
}

// ============================================================================
// MidiAssignOverlay
// ============================================================================

MidiAssignOverlay::MidiAssignOverlay(QWidget *parent)
	: QWidget(parent)
{
	setMouseTracking(true);
	setAttribute(Qt::WA_TransparentForMouseEvents, false);

	m_status_label = new QLabel(this);
	m_status_label->setAlignment(Qt::AlignCenter);
	m_status_label->setStyleSheet(
		"QLabel {"
		"  color: #ffffff;"
		"  background-color: rgba(30, 30, 30, 220);"
		"  border: 1px solid rgba(100, 200, 255, 0.6);"
		"  border-radius: 6px;"
		"  padding: 8px 16px;"
		"  font-size: 12px;"
		"  font-weight: bold;"
		"}");
	m_status_label->hide();
}

void MidiAssignOverlay::set_controls(const QMap<QString, QWidget *> &controls)
{
	m_controls = controls;
}

void MidiAssignOverlay::activate()
{
	m_active = true;
	m_hovered_control.clear();
	setCursor(Qt::CrossCursor);
	show();
	show_status("Click a control to assign MIDI");
	update();
}

void MidiAssignOverlay::deactivate()
{
	m_active = false;
	m_hovered_control.clear();
	m_status_label->hide();
	setCursor(Qt::ArrowCursor);
	hide();
}

void MidiAssignOverlay::show_status(const QString &text)
{
	m_status_label->setText(text);
	m_status_label->adjustSize();
	// Center horizontally near the bottom
	int x = (width() - m_status_label->width()) / 2;
	int y = height() - m_status_label->height() - 12;
	if (y < 8)
		y = 8;
	m_status_label->move(x, y);
	m_status_label->show();
	m_status_label->raise();
}

bool MidiAssignOverlay::is_active() const
{
	return m_active;
}

// ---------------------------------------------------------------------------
// Find which registered control is under a point (in overlay coordinates)
// ---------------------------------------------------------------------------

QString MidiAssignOverlay::find_control_at(const QPoint &pos) const
{
	QPoint global_pos = mapToGlobal(pos);

	for (auto it = m_controls.constBegin(); it != m_controls.constEnd(); ++it) {
		QWidget *ctrl = it.value();
		if (!ctrl || !ctrl->isVisible())
			continue;

		QPoint local = ctrl->mapFromGlobal(global_pos);
		if (ctrl->rect().contains(local)) {
			return it.key();
		}
	}
	return {};
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

void MidiAssignOverlay::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
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

		// Map control rectangle to overlay coordinates
		QPoint tl = ctrl->mapToGlobal(QPoint(0, 0));
		QPoint br = ctrl->mapToGlobal(QPoint(ctrl->width(), ctrl->height()));
		QRect r(mapFromGlobal(tl), mapFromGlobal(br));

		bool hovered = (it.key() == m_hovered_control);

		if (hovered) {
			// Bright highlight
			p.setPen(QPen(QColor(80, 200, 255), 2));
			p.setBrush(QColor(80, 200, 255, 50));
		} else {
			// Subtle highlight
			p.setPen(QPen(QColor(80, 200, 255, 120), 1));
			p.setBrush(QColor(80, 200, 255, 20));
		}
		p.drawRoundedRect(r.adjusted(-2, -2, 2, 2), 4, 4);

		// Draw control name label
		QFont f = font();
		f.setPointSize(8);
		f.setBold(hovered);
		p.setFont(f);
		p.setPen(QColor(220, 240, 255));
		p.drawText(r.adjusted(0, -16, 0, -r.height()), Qt::AlignLeft | Qt::AlignBottom,
			it.key());
	}
}

// ---------------------------------------------------------------------------
// Mouse events
// ---------------------------------------------------------------------------

void MidiAssignOverlay::mouseMoveEvent(QMouseEvent *event)
{
	if (!m_active)
		return;

	QString found = find_control_at(event->pos());
	if (found != m_hovered_control) {
		m_hovered_control = found;
		update();
	}
}

void MidiAssignOverlay::mousePressEvent(QMouseEvent *event)
{
	if (!m_active || event->button() != Qt::LeftButton)
		return;

	QString found = find_control_at(event->pos());
	if (!found.isEmpty()) {
		emit control_clicked(found);
	}
}
