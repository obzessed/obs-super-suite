#pragma once

// ============================================================================
// SuperWidget — The Universal Base Class for all Super Suite docks.
//
// Replaces PersistableWidget. Provides:
//   1. Persistence (save/load state as JSON)
//   2. Dual Toolbar (System + User)
//   3. Control Registration via ControlRegistry
//   4. Assign Overlay for mapping controls
//   5. Grab Handle for toolbar emergency restore
//   6. Monitor Console (slide-up debug panel)
// ============================================================================

#include <QWidget>
#include <QToolBar>
#include <QAction>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMap>
#include <QPushButton>
#include <QPointer>
#include <QPlainTextEdit>

namespace super {

class ControlPort;
class MidiAdapter;
class AssignOverlay;
class GrabHandle;
class ControlAssignPopup;

// ---------------------------------------------------------------------------
// SuperWidget — The base class for all plugin docks.
// ---------------------------------------------------------------------------
class SuperWidget : public QWidget {
	Q_OBJECT

public:
	explicit SuperWidget(const QString &widget_id, QWidget *parent = nullptr);
	~SuperWidget() override;

	// -- Identity ----------------------------------------------------------
	QString widget_id() const;

	// -- Content Area ------------------------------------------------------
	// The area where subclasses place their UI.
	// Has no layout by default — subclass should set one.
	QWidget *content_area() const;

	// -- Persistence -------------------------------------------------------
	virtual QJsonObject save_state() const;
	virtual void load_state(const QJsonObject &state);

	// -- Control Registration (New: uses ControlRegistry) ------------------
	// Register a child widget as a controllable port.
	// Creates a ControlPort in the global registry with ID:
	//   "<widget_id>.<control_name>"
	void register_control(QWidget *control, const QString &name = {});
	void unregister_control(const QString &name);
	QStringList control_names() const;

	// -- Assign Mode -------------------------------------------------------
	bool is_assign_active() const;

	// -- Toolbar Visibility ------------------------------------------------
	void set_system_toolbar_visible(bool visible);
	bool is_system_toolbar_visible() const;
	void set_user_toolbar_visible(bool visible);
	bool is_user_toolbar_visible() const;

	// -- Monitor Console ---------------------------------------------------
	void log_to_console(const QString &message);
	void set_console_visible(bool visible);
	bool is_console_visible() const;

protected:
	// Called when a control port value changes from an external source.
	// Subclasses can override for custom behavior.
	virtual void on_control_value(const QString &control_name, double value);

	// Access toolbars to add custom actions
	QToolBar *system_toolbar() const;
	QToolBar *user_toolbar() const;

	// Add actions to specific toolbar zones
	void add_system_start_action(QAction *action);
	void add_system_end_action(QAction *action);
	void add_user_action(QAction *action);

	void resizeEvent(QResizeEvent *event) override;

private:
	void setup_base_ui();
	void setup_system_toolbar();
	void setup_user_toolbar();
	void setup_grab_handle();
	void setup_monitor_console();
	void toggle_assign_mode(bool active);
	void on_control_clicked_for_assign(const QString &control_name);
	void update_overlay_geometry();
	void update_grab_handle_position();

	// Identity
	QString m_widget_id;

	// Layout
	QVBoxLayout *m_main_layout = nullptr;
	QWidget *m_content_area = nullptr;

	// System Toolbar (top): [Name] [Learn] [Lock] ... [Snapshot] [Debug] [Overflow]
	QToolBar *m_system_toolbar = nullptr;
	QAction *m_assign_action = nullptr;
	QAction *m_enable_action = nullptr;
	QAction *m_console_action = nullptr;
	QWidget *m_system_start_spacer = nullptr;
	QList<QAction *> m_system_start_actions;
	QList<QAction *> m_system_end_actions;
	bool m_controls_enabled = false;

	// User Toolbar (below system): plugin-specific actions
	QToolBar *m_user_toolbar = nullptr;

	// Assign Overlay
	AssignOverlay *m_overlay = nullptr;
	QMap<QString, QWidget *> m_registered_controls;

	// Grab Handle (emergency toolbar restore)
	GrabHandle *m_grab_handle = nullptr;

	// Monitor Console (bottom slide-up)
	QWidget *m_console_container = nullptr;
	QPlainTextEdit *m_console_log = nullptr;
	QPushButton *m_console_clear_btn = nullptr;

	// MidiAdapter (owned, bridges MIDI → ControlPorts)
	MidiAdapter *m_midi_adapter = nullptr;

	// Active assign popup
	QPointer<ControlAssignPopup> m_assign_popup;
};

// ---------------------------------------------------------------------------
// AssignOverlay — Translucent overlay for control mapping.
// Shows registered controls highlighted; click to start mapping.
// ---------------------------------------------------------------------------
class AssignOverlay : public QWidget {
	Q_OBJECT

public:
	explicit AssignOverlay(QWidget *parent = nullptr);

	void set_controls(const QMap<QString, QWidget *> &controls);
	void activate();
	void deactivate();
	bool is_active() const;

signals:
	void control_clicked(const QString &control_name);

protected:
	void paintEvent(QPaintEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;

private:
	QString find_control_at(const QPoint &pos) const;

	QMap<QString, QWidget *> m_controls;
	QString m_hovered_control;
	bool m_active = false;
};

// ---------------------------------------------------------------------------
// GrabHandle — A small persistent overlay widget in the corner of the dock.
// Always visible (even when toolbars are hidden). Click to restore toolbars.
// ---------------------------------------------------------------------------
class GrabHandle : public QWidget {
	Q_OBJECT

public:
	explicit GrabHandle(QWidget *parent = nullptr);

signals:
	void clicked();

protected:
	void paintEvent(QPaintEvent *event) override;
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;

private:
	bool m_hovered = false;
};

} // namespace super
