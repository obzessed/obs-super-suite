#pragma once
// ============================================================================
// SurfaceEditorWindow — Visual Drag-and-Drop Surface Builder (Tools window).
//
// Three-panel layout:
//   Left:   Element palette (drag source)
//   Center: Grid canvas (drop target, visual layout)
//   Right:  Property editor (selected element)
// ============================================================================

#include <QDialog>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QListWidget>
#include <QFormLayout>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QSplitter>
#include <QJsonObject>
#include <QJsonArray>

namespace surf_ed {

// ---------------------------------------------------------------------------
// SurfaceItem — A draggable element on the canvas.
// ---------------------------------------------------------------------------
class SurfaceItem : public QGraphicsItem {
public:
	enum ElemType {
		Fader, HFader, Knob, Button, Toggle, Label, Encoder, XYPad
	};

	SurfaceItem(ElemType type, const QString &label, QGraphicsItem *parent = nullptr);

	QRectF boundingRect() const override;
	void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;

	ElemType elem_type() const { return m_type; }
	QString label() const { return m_label; }
	void set_label(const QString &l) { m_label = l; update(); }

	QString port_binding;
	double min_val = 0.0, max_val = 1.0, default_val = 0.0;
	bool checkable = false;

	QJsonObject to_json() const;
	static SurfaceItem *from_json(const QJsonObject &o);

	static QString type_name(ElemType t);
	static QColor type_color(ElemType t);
	static QSizeF type_size(ElemType t);

private:
	ElemType m_type;
	QString m_label;
};

// ---------------------------------------------------------------------------
// CanvasScene — Drop-target scene with grid snapping.
// ---------------------------------------------------------------------------
class CanvasScene : public QGraphicsScene {
	Q_OBJECT
public:
	explicit CanvasScene(QObject *parent = nullptr);
	static constexpr double GRID = 20.0;

signals:
	void item_selected(SurfaceItem *item);
	void item_dropped(SurfaceItem::ElemType type, const QPointF &pos);

protected:
	void dragEnterEvent(QGraphicsSceneDragDropEvent *e) override;
	void dragMoveEvent(QGraphicsSceneDragDropEvent *e) override;
	void dropEvent(QGraphicsSceneDragDropEvent *e) override;
	void mousePressEvent(QGraphicsSceneMouseEvent *e) override;
};

// ---------------------------------------------------------------------------
// CanvasView — View with grid background and scroll-wheel zoom.
// ---------------------------------------------------------------------------
class CanvasView : public QGraphicsView {
	Q_OBJECT
public:
	explicit CanvasView(QGraphicsScene *scene, QWidget *parent = nullptr);
protected:
	void wheelEvent(QWheelEvent *e) override;
	void drawBackground(QPainter *p, const QRectF &rect) override;
};

} // namespace surf_ed

// ═══════════════════════════════════════════════════════════════════════════
// SurfaceEditorWindow
// ═══════════════════════════════════════════════════════════════════════════
class SurfaceEditorWindow : public QDialog {
	Q_OBJECT
public:
	explicit SurfaceEditorWindow(QWidget *parent = nullptr);

private:
	void setup_ui();
	void populate_palette();
	void on_item_selected(surf_ed::SurfaceItem *item);
	void update_props_from_ui();
	void export_schema();
	void import_schema();

	// Left: palette
	QListWidget *m_palette = nullptr;

	// Center: canvas
	surf_ed::CanvasView  *m_canvas_view = nullptr;
	surf_ed::CanvasScene *m_canvas_scene = nullptr;

	// Right: properties
	QWidget        *m_props_panel = nullptr;
	QLineEdit      *m_prop_label = nullptr;
	QComboBox      *m_prop_type = nullptr;
	QLineEdit      *m_prop_port = nullptr;
	QDoubleSpinBox *m_prop_min = nullptr;
	QDoubleSpinBox *m_prop_max = nullptr;
	QDoubleSpinBox *m_prop_default = nullptr;
	QCheckBox      *m_prop_checkable = nullptr;

	surf_ed::SurfaceItem *m_selected = nullptr;
	QList<surf_ed::SurfaceItem*> m_items;
};
