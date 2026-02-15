#pragma once

#include <QWidget>
#include <QToolBar>
#include <QAction>
#include <QLabel>
#include <QVBoxLayout>
#include <QJsonObject>
#include <QMap>

class MidiAssignOverlay;

// Base class for widgets that support:
//   1. Persistent state via save_state() / load_state()
//   2. MIDI assignment via a toolbar toggle + click-overlay
//
// Subclasses should:
//   - Add their UI to content_area() (using its layout or setting one)
//   - Call register_midi_control() for each MIDI-assignable child control
//   - Override save_state() / load_state() to persist custom data
//   - Optionally override on_midi_cc() for custom value mapping
class PersistableWidget : public QWidget {
	Q_OBJECT

public:
	explicit PersistableWidget(const QString &widget_id, QWidget *parent = nullptr);
	~PersistableWidget() override;

	// Unique persistent identifier for this widget instance
	QString widget_id() const;

	// Container widget where subclasses place their controls.
	// Has no layout by default — subclass should set one.
	QWidget *content_area() const;

	// --- Persistence ---
	// Override to save/load your widget's state.
	// Base implementation saves/loads nothing.
	virtual QJsonObject save_state() const;
	virtual void load_state(const QJsonObject &state);

	// --- MIDI ---
	// Register a child control as MIDI-assignable.
	// 'name' is the binding key; defaults to control->objectName().
	void register_midi_control(QWidget *control, const QString &name = {});
	void unregister_midi_control(const QString &name);
	QStringList midi_control_names() const;

	// Dock-level MIDI enable/disable
	bool is_midi_enabled() const;
	void set_midi_enabled(bool enabled);

protected:
	// Called when a matched MIDI CC arrives for a registered control.
	// Default maps 0-127 → QSlider range, or toggles QPushButton.
	virtual void on_midi_cc(const QString &control_name, int value);

	// Access the toolbar to add custom actions
	QToolBar *toolbar() const;

	void resizeEvent(QResizeEvent *event) override;

private:
	void setup_base_ui();
	void toggle_midi_assign(bool active);
	void on_control_clicked_for_learn(const QString &control_name);
	void on_binding_learned();
	void on_learn_cancelled();
	void update_overlay_geometry();

	QString m_widget_id;
	QToolBar *m_toolbar;
	QAction *m_midi_assign_action;
	QAction *m_midi_enable_action;
	bool m_midi_enabled = false;
	QWidget *m_content_area;
	QVBoxLayout *m_main_layout;
	MidiAssignOverlay *m_overlay;
	QMap<QString, QWidget *> m_midi_controls;
};

// Translucent overlay shown when MIDI assign mode is active.
// Highlights registered controls on hover and starts MIDI Learn on click.
class MidiAssignOverlay : public QWidget {
	Q_OBJECT

public:
	explicit MidiAssignOverlay(QWidget *parent = nullptr);

	void set_controls(const QMap<QString, QWidget *> &controls);
	void activate();
	void deactivate();
	void show_status(const QString &text);
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
	QLabel *m_status_label;
	bool m_active = false;
};
