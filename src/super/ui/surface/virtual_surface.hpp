#pragma once

// ============================================================================
// Virtual Surface — JSON-Driven UI Builder
//
// Renders custom control panels from JSON schemas.
// Supports: Fader, Knob, Button, Toggle, Label, Encoder, XYPad, Group.
// Each widget auto-binds to a ControlPort in the Registry.
// ============================================================================

#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QDial>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QList>
#include <QString>

namespace super {

class ControlPort;

// ---------------------------------------------------------------------------
// SurfaceWidget — Type of widget in a surface layout.
// ---------------------------------------------------------------------------
enum class SurfaceWidgetType {
	Fader,		// Vertical slider
	HFader,		// Horizontal slider
	Knob,		// Rotary dial
	Button,		// Momentary or toggle button
	Toggle,		// Checkbox toggle
	Label,		// Display-only text
	Encoder,	// Knob with delta (infinite rotation)
	XYPad,		// 2D touch area
	Group		// Container for sub-widgets
};

// ---------------------------------------------------------------------------
// SurfaceElement — One item parsed from a surface JSON schema.
// ---------------------------------------------------------------------------
struct SurfaceElement {
	SurfaceWidgetType type = SurfaceWidgetType::Fader;
	QString id;				// Unique ID within the surface
	QString label;			// Display label
	QString port_binding;	// ControlPort ID to bind to
	int row = 0;			// Grid position
	int col = 0;

	// Widget-specific
	double min_val = 0.0;
	double max_val = 1.0;
	double default_val = 0.0;
	bool checkable = false;			// For Button type
	QList<SurfaceElement> children;	// For Group type

	// Parse from JSON
	static SurfaceElement from_json(const QJsonObject &obj);
};

// ---------------------------------------------------------------------------
// SurfaceSchema — A complete surface layout definition.
// ---------------------------------------------------------------------------
struct SurfaceSchema {
	QString name;
	int columns = 4;
	QList<SurfaceElement> elements;

	static SurfaceSchema from_json(const QJsonObject &obj);
};

// ---------------------------------------------------------------------------
// VirtualSurface — Renders a SurfaceSchema into live Qt widgets.
// ---------------------------------------------------------------------------
class VirtualSurface : public QWidget {
	Q_OBJECT

public:
	explicit VirtualSurface(QWidget *parent = nullptr);

	// Load and render from a JSON schema
	void load_schema(const QJsonObject &schema_json);
	void load_schema(const SurfaceSchema &schema);

	// Clear all widgets
	void clear();

	// Get the schema name
	QString schema_name() const;

signals:
	void control_changed(const QString &element_id, double value);

private:
	QWidget *create_widget(const SurfaceElement &element);
	void bind_to_port(QWidget *widget, const QString &port_id);

	QGridLayout *m_grid = nullptr;
	QString m_schema_name;
	QList<QWidget *> m_widgets;  // Owned widgets for cleanup
};

} // namespace super
