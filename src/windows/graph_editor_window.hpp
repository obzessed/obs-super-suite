#pragma once
// ============================================================================
// GraphEditorWindow — Node-based signal-flow graph editor (Tools window).
//
// Full-featured graph builder:
//   - Drag-and-drop port-to-port wiring
//   - Double-click nodes for property editing
//   - Right-click context menu for all operations
//   - Scroll-wheel zoom
//   - No toolbar — clean canvas-first UX
// ============================================================================

#include <QDialog>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QJsonObject>
#include <QJsonArray>
#include <QMenu>
#include <QWheelEvent>

// ═══════════════════════════════════════════════════════════════════════════
namespace graph {
// ═══════════════════════════════════════════════════════════════════════════

struct PortDef {
	enum Dir { In, Out };
	Dir dir;
	QString name;
	int index;
};

// ---------------------------------------------------------------------------
// GraphNode
// ---------------------------------------------------------------------------
class GraphNode : public QGraphicsItem {
public:
	enum Type {
		MidiInput, Filter, Interp, Math,
		Output, Constant, Splitter, Merger
	};

	GraphNode(Type type, const QString &label, QGraphicsItem *parent = nullptr);

	QRectF boundingRect() const override;
	void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;

	Type node_type() const { return m_type; }
	QString label() const { return m_label; }
	void set_label(const QString &l) { m_label = l; update(); }

	QPointF port_center(PortDef::Dir dir, int index) const;
	int port_at(const QPointF &local_pos, PortDef::Dir &out_dir) const;

	int in_count() const { return m_ins.size(); }
	int out_count() const { return m_outs.size(); }

	QVector<PortDef> m_ins;
	QVector<PortDef> m_outs;

	// Type-specific properties
	QJsonObject properties;

	QJsonObject to_json() const;
	static GraphNode *from_json(const QJsonObject &o);

	static QString type_name(Type t);
	static QColor type_color(Type t);

	static constexpr int W = 150, H = 64, PORT_R = 6;

protected:
	QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
	Type m_type;
	QString m_label;
};

// ---------------------------------------------------------------------------
// GraphEdge — Cubic bezier connection between ports.
// ---------------------------------------------------------------------------
class GraphEdge : public QGraphicsItem {
public:
	GraphEdge(GraphNode *src, int src_port, GraphNode *dst, int dst_port,
		QGraphicsItem *parent = nullptr);

	QRectF boundingRect() const override;
	void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;
	QPainterPath shape() const override;

	GraphNode *source() const { return m_src; }
	GraphNode *dest()   const { return m_dst; }
	int src_port() const { return m_src_port; }
	int dst_port() const { return m_dst_port; }

	void refresh();

private:
	QPainterPath build_path() const;
	GraphNode *m_src; int m_src_port;
	GraphNode *m_dst; int m_dst_port;
};

// ---------------------------------------------------------------------------
// TempWire — Rubber-band line shown while dragging a new connection.
// ---------------------------------------------------------------------------
class TempWire : public QGraphicsItem {
public:
	TempWire(const QPointF &start); 
	void set_end(const QPointF &end);
	QRectF boundingRect() const override;
	void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;
private:
	QPointF m_start, m_end;
};

// ---------------------------------------------------------------------------
// GraphScene — Custom scene handling port-to-port DnD wiring.
// ---------------------------------------------------------------------------
class GraphScene : public QGraphicsScene {
	Q_OBJECT
public:
	explicit GraphScene(QObject *parent = nullptr);

signals:
	void edge_created(GraphNode *src, int src_port, GraphNode *dst, int dst_port);
	void node_double_clicked(GraphNode *node);

protected:
	void mousePressEvent(QGraphicsSceneMouseEvent *e) override;
	void mouseMoveEvent(QGraphicsSceneMouseEvent *e) override;
	void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override;
	void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) override;

private:
	// Wire-drag state
	bool m_wiring = false;
	GraphNode *m_wire_src = nullptr;
	int m_wire_src_port = -1;
	TempWire *m_temp_wire = nullptr;
};

// ---------------------------------------------------------------------------
// GraphView — Custom view with scroll-wheel zoom.
// ---------------------------------------------------------------------------
class GraphView : public QGraphicsView {
	Q_OBJECT
public:
	explicit GraphView(QGraphicsScene *scene, QWidget *parent = nullptr);
protected:
	void wheelEvent(QWheelEvent *e) override;
	void drawBackground(QPainter *p, const QRectF &rect) override;
};

} // namespace graph

// ═══════════════════════════════════════════════════════════════════════════
// GraphEditorWindow — The dialog hosting the graph editor.
// ═══════════════════════════════════════════════════════════════════════════
class GraphEditorWindow : public QDialog {
	Q_OBJECT

public:
	explicit GraphEditorWindow(QWidget *parent = nullptr);

private:
	void setup_ui();
	void add_node(graph::GraphNode::Type type, const QPointF &pos = {});
	void delete_selected();
	void clear_graph();
	void show_node_properties(graph::GraphNode *node);
	void context_menu(const QPoint &view_pos);

	graph::GraphView  *m_view  = nullptr;
	graph::GraphScene *m_scene = nullptr;

	QList<graph::GraphNode*> m_nodes;
	QList<graph::GraphEdge*> m_edges;
};
