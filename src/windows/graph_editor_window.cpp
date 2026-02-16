#include "graph_editor_window.hpp"

#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QApplication>
#include <QLabel>
#include <QJsonDocument>
#include <QtMath>

// ═══════════════════════════════════════════════════════════════════════════
namespace graph {
// ═══════════════════════════════════════════════════════════════════════════

// ===== GraphNode ==========================================================

QString GraphNode::type_name(Type t) {
	static const char *names[] = {
		"MIDI In","Filter","Interp","Math",
		"Output","Const","Split","Merge"
	};
	return names[static_cast<int>(t)];
}

QColor GraphNode::type_color(Type t) {
	static const QColor cols[] = {
		{50,140,210}, {60,190,110}, {130,105,245}, {210,170,50},
		{210,70,70},  {140,140,140},{170,110,190}, {90,170,190}
	};
	return cols[static_cast<int>(t)];
}

GraphNode::GraphNode(Type type, const QString &label, QGraphicsItem *parent)
	: QGraphicsItem(parent), m_type(type), m_label(label)
{
	setFlag(ItemIsMovable);
	setFlag(ItemIsSelectable);
	setFlag(ItemSendsGeometryChanges);
	setZValue(10);
	setCacheMode(DeviceCoordinateCache);

	switch (type) {
	case MidiInput:
		m_outs.append({PortDef::Out, "out", 0});
		break;
	case Output:
		m_ins.append({PortDef::In, "in", 0});
		break;
	case Splitter:
		m_ins.append({PortDef::In, "in", 0});
		m_outs.append({PortDef::Out, "a", 0});
		m_outs.append({PortDef::Out, "b", 1});
		break;
	case Merger:
		m_ins.append({PortDef::In, "a", 0});
		m_ins.append({PortDef::In, "b", 1});
		m_outs.append({PortDef::Out, "out", 0});
		break;
	default:
		m_ins.append({PortDef::In, "in", 0});
		m_outs.append({PortDef::Out, "out", 0});
		break;
	}
}

QRectF GraphNode::boundingRect() const {
	return QRectF(-PORT_R, -PORT_R, W + PORT_R * 2, H + PORT_R * 2);
}

void GraphNode::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) {
	p->setRenderHint(QPainter::Antialiasing);
	QColor base = type_color(m_type);
	bool sel = isSelected();

	// Drop shadow
	p->setPen(Qt::NoPen);
	p->setBrush(QColor(0, 0, 0, 50));
	p->drawRoundedRect(QRectF(3, 3, W, H), 8, 8);

	// Body gradient
	QLinearGradient bg(0, 0, 0, H);
	bg.setColorAt(0, base.lighter(sel ? 145 : 115));
	bg.setColorAt(1, base.darker(140));
	p->setBrush(bg);
	p->setPen(QPen(sel ? QColor(255, 255, 255, 200) : base.darker(170), sel ? 2.5 : 1.2));
	p->drawRoundedRect(QRectF(0, 0, W, H), 8, 8);

	// Header bar
	QPainterPath header;
	header.addRoundedRect(QRectF(0, 0, W, 22), 8, 8);
	header.addRect(QRectF(0, 12, W, 10));
	p->setClipPath(header);
	p->setBrush(QColor(0, 0, 0, 60));
	p->setPen(Qt::NoPen);
	p->drawRect(QRectF(0, 0, W, 22));
	p->setClipping(false);

	// Header text
	QFont hf;
	hf.setPixelSize(10); hf.setBold(true);
	p->setFont(hf);
	p->setPen(QColor(255, 255, 255, 230));
	p->drawText(QRectF(8, 0, W - 16, 22), Qt::AlignLeft | Qt::AlignVCenter, type_name(m_type));

	// Label text
	QFont lf;
	lf.setPixelSize(12);
	p->setFont(lf);
	p->setPen(QColor(255, 255, 255, 210));
	p->drawText(QRectF(8, 24, W - 16, H - 28), Qt::AlignLeft | Qt::AlignVCenter, m_label);

	// Separator line
	p->setPen(QPen(QColor(255, 255, 255, 40), 1));
	p->drawLine(QPointF(1, 22), QPointF(W - 1, 22));

	// === Ports ===
	auto draw_port = [&](QPointF center, bool is_input, bool hovered) {
		// Glow
		if (hovered) {
			QRadialGradient glow(center, PORT_R * 2.5);
			glow.setColorAt(0, QColor(is_input ? 80 : 255, is_input ? 200 : 160, is_input ? 255 : 80, 100));
			glow.setColorAt(1, Qt::transparent);
			p->setBrush(glow);
			p->setPen(Qt::NoPen);
			p->drawEllipse(center, PORT_R * 2.5, PORT_R * 2.5);
		}
		// Port circle
		QRadialGradient pg(center + QPointF(-1, -1), PORT_R);
		pg.setColorAt(0, is_input ? QColor(120, 210, 255) : QColor(255, 180, 90));
		pg.setColorAt(1, is_input ? QColor(50, 120, 200)  : QColor(200, 100, 30));
		p->setBrush(pg);
		p->setPen(QPen(QColor(0, 0, 0, 120), 1));
		p->drawEllipse(center, PORT_R, PORT_R);
	};

	for (int i = 0; i < m_ins.size(); i++)
		draw_port(port_center(PortDef::In, i), true, false);
	for (int i = 0; i < m_outs.size(); i++)
		draw_port(port_center(PortDef::Out, i), false, false);
}

QPointF GraphNode::port_center(PortDef::Dir dir, int index) const {
	if (dir == PortDef::In) {
		double sp = H / (m_ins.size() + 1.0);
		return QPointF(0, sp * (index + 1));
	} else {
		double sp = H / (m_outs.size() + 1.0);
		return QPointF(W, sp * (index + 1));
	}
}

int GraphNode::port_at(const QPointF &local, PortDef::Dir &out_dir) const {
	double hit = PORT_R * 2.2;
	for (int i = 0; i < m_outs.size(); i++) {
		QPointF c = port_center(PortDef::Out, i);
		if (QLineF(local, c).length() < hit) { out_dir = PortDef::Out; return i; }
	}
	for (int i = 0; i < m_ins.size(); i++) {
		QPointF c = port_center(PortDef::In, i);
		if (QLineF(local, c).length() < hit) { out_dir = PortDef::In; return i; }
	}
	return -1;
}

QVariant GraphNode::itemChange(GraphicsItemChange change, const QVariant &value) {
	if (change == ItemPositionHasChanged) {
		// Tell edges to refresh — we iterate scene items
		if (scene()) {
			for (auto *item : scene()->items()) {
				auto *edge = dynamic_cast<GraphEdge*>(item);
				if (edge && (edge->source() == this || edge->dest() == this))
					edge->refresh();
			}
		}
	}
	return QGraphicsItem::itemChange(change, value);
}

QJsonObject GraphNode::to_json() const {
	QJsonObject o;
	o["type"]  = static_cast<int>(m_type);
	o["label"] = m_label;
	o["x"]     = pos().x();
	o["y"]     = pos().y();
	if (!properties.isEmpty()) o["props"] = properties;
	return o;
}

GraphNode *GraphNode::from_json(const QJsonObject &o) {
	auto *n = new GraphNode(static_cast<Type>(o["type"].toInt()), o["label"].toString());
	n->setPos(o["x"].toDouble(), o["y"].toDouble());
	if (o.contains("props")) n->properties = o["props"].toObject();
	return n;
}

// ===== GraphEdge ===========================================================

GraphEdge::GraphEdge(GraphNode *src, int src_port, GraphNode *dst, int dst_port,
	QGraphicsItem *parent)
	: QGraphicsItem(parent), m_src(src), m_src_port(src_port), m_dst(dst), m_dst_port(dst_port)
{
	setZValue(5);
	setFlag(ItemIsSelectable);
}

QPainterPath GraphEdge::build_path() const {
	QPointF s = m_src->mapToScene(m_src->port_center(PortDef::Out, m_src_port));
	QPointF d = m_dst->mapToScene(m_dst->port_center(PortDef::In, m_dst_port));
	double dx = qMax(qAbs(d.x() - s.x()) * 0.5, 40.0);

	QPainterPath path;
	path.moveTo(s);
	path.cubicTo(s + QPointF(dx, 0), d - QPointF(dx, 0), d);
	return path;
}

QRectF GraphEdge::boundingRect() const {
	return build_path().boundingRect().adjusted(-8, -8, 8, 8);
}

QPainterPath GraphEdge::shape() const {
	QPainterPathStroker stroker;
	stroker.setWidth(10);
	return stroker.createStroke(build_path());
}

void GraphEdge::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) {
	p->setRenderHint(QPainter::Antialiasing);
	QPainterPath path = build_path();
	bool sel = isSelected();

	// Glow for selected
	if (sel) {
		p->setPen(QPen(QColor(100, 180, 255, 60), 8, Qt::SolidLine, Qt::RoundCap));
		p->setBrush(Qt::NoBrush);
		p->drawPath(path);
	}

	QLinearGradient grad(path.pointAtPercent(0), path.pointAtPercent(1));
	grad.setColorAt(0, QColor(255, 180, 90, sel ? 255 : 180));
	grad.setColorAt(1, QColor(100, 200, 255, sel ? 255 : 180));

	p->setPen(QPen(QBrush(grad), sel ? 3.0 : 2.0, Qt::SolidLine, Qt::RoundCap));
	p->setBrush(Qt::NoBrush);
	p->drawPath(path);
}

void GraphEdge::refresh() { prepareGeometryChange(); update(); }

// ===== TempWire ============================================================

TempWire::TempWire(const QPointF &start)
	: m_start(start), m_end(start)
{
	setZValue(100);
}

void TempWire::set_end(const QPointF &end) {
	prepareGeometryChange();
	m_end = end;
	update();
}

QRectF TempWire::boundingRect() const {
	return QRectF(m_start, m_end).normalized().adjusted(-10, -10, 10, 10);
}

void TempWire::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) {
	p->setRenderHint(QPainter::Antialiasing);
	double dx = qMax(qAbs(m_end.x() - m_start.x()) * 0.5, 30.0);
	QPainterPath path;
	path.moveTo(m_start);
	path.cubicTo(m_start + QPointF(dx, 0), m_end - QPointF(dx, 0), m_end);

	p->setPen(QPen(QColor(255, 255, 255, 120), 2.0, Qt::DashLine, Qt::RoundCap));
	p->setBrush(Qt::NoBrush);
	p->drawPath(path);

	// Animated dot at end
	p->setPen(Qt::NoPen);
	p->setBrush(QColor(255, 255, 255, 180));
	p->drawEllipse(m_end, 4, 4);
}

// ===== GraphScene ==========================================================

GraphScene::GraphScene(QObject *parent) : QGraphicsScene(parent) {}

void GraphScene::mousePressEvent(QGraphicsSceneMouseEvent *e) {
	if (e->button() == Qt::LeftButton) {
		auto *item = itemAt(e->scenePos(), QTransform());
		auto *node = dynamic_cast<GraphNode*>(item);
		if (!node) {
			// Check parent (port circle paints don't create child items)
			for (auto *it : items(e->scenePos())) {
				node = dynamic_cast<GraphNode*>(it);
				if (node) break;
			}
		}
		if (node) {
			QPointF local = node->mapFromScene(e->scenePos());
			PortDef::Dir dir;
			int port = node->port_at(local, dir);
			if (port >= 0 && dir == PortDef::Out) {
				// Start wiring from output port
				m_wiring = true;
				m_wire_src = node;
				m_wire_src_port = port;
				QPointF start = node->mapToScene(node->port_center(PortDef::Out, port));
				m_temp_wire = new TempWire(start);
				addItem(m_temp_wire);
				e->accept();
				return;
			}
		}
	}
	QGraphicsScene::mousePressEvent(e);
}

void GraphScene::mouseMoveEvent(QGraphicsSceneMouseEvent *e) {
	if (m_wiring && m_temp_wire) {
		m_temp_wire->set_end(e->scenePos());
		e->accept();
		return;
	}
	QGraphicsScene::mouseMoveEvent(e);
}

void GraphScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *e) {
	if (m_wiring && e->button() == Qt::LeftButton) {
		m_wiring = false;
		if (m_temp_wire) {
			removeItem(m_temp_wire);
			delete m_temp_wire;
			m_temp_wire = nullptr;
		}

		// Check if we released on an input port
		for (auto *it : items(e->scenePos())) {
			auto *target = dynamic_cast<GraphNode*>(it);
			if (target && target != m_wire_src) {
				QPointF local = target->mapFromScene(e->scenePos());
				PortDef::Dir dir;
				int port = target->port_at(local, dir);
				if (port >= 0 && dir == PortDef::In) {
					emit edge_created(m_wire_src, m_wire_src_port, target, port);
					break;
				}
			}
		}
		m_wire_src = nullptr;
		m_wire_src_port = -1;
		e->accept();
		return;
	}
	QGraphicsScene::mouseReleaseEvent(e);
}

void GraphScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) {
	for (auto *it : items(e->scenePos())) {
		auto *node = dynamic_cast<GraphNode*>(it);
		if (node) {
			emit node_double_clicked(node);
			e->accept();
			return;
		}
	}
	QGraphicsScene::mouseDoubleClickEvent(e);
}

// ===== GraphView ===========================================================

GraphView::GraphView(QGraphicsScene *scene, QWidget *parent)
	: QGraphicsView(scene, parent)
{
	setRenderHint(QPainter::Antialiasing);
	setDragMode(RubberBandDrag);
	setTransformationAnchor(AnchorUnderMouse);
	setViewportUpdateMode(FullViewportUpdate);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void GraphView::wheelEvent(QWheelEvent *e) {
	double factor = (e->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
	scale(factor, factor);
}

void GraphView::drawBackground(QPainter *p, const QRectF &rect) {
	p->fillRect(rect, QColor(13, 13, 20));

	// Grid dots
	double grid = 20.0;
	double left = std::floor(rect.left() / grid) * grid;
	double top  = std::floor(rect.top()  / grid) * grid;

	p->setPen(Qt::NoPen);
	p->setBrush(QColor(60, 60, 80, 40));
	for (double x = left; x < rect.right(); x += grid)
		for (double y = top; y < rect.bottom(); y += grid)
			p->drawRect(QRectF(x, y, 1.5, 1.5));

	// Major grid lines every 5
	p->setPen(QPen(QColor(60, 60, 80, 25), 0.5));
	double major = grid * 5;
	double ml = std::floor(rect.left() / major) * major;
	double mt = std::floor(rect.top()  / major) * major;
	for (double x = ml; x < rect.right(); x += major)
		p->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
	for (double y = mt; y < rect.bottom(); y += major)
		p->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
}

} // namespace graph

// ═══════════════════════════════════════════════════════════════════════════
// GraphEditorWindow
// ═══════════════════════════════════════════════════════════════════════════

static const char *WIN_STYLE = R"(
QDialog { background:#0d0d14; }
QMenu    { background:#1a1a24; color:#c8c8d8; border:1px solid #2a2a3a; }
QMenu::item:selected { background:#2d3390; }
QMenu::separator { background:#2a2a3a; height:1px; margin:4px 8px; }
)";

GraphEditorWindow::GraphEditorWindow(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle("Graph Editor");
	setMinimumSize(800, 550);
	resize(1100, 700);
	setStyleSheet(WIN_STYLE);
	setup_ui();
}

void GraphEditorWindow::setup_ui()
{
	auto *top = new QVBoxLayout(this);
	top->setContentsMargins(0, 0, 0, 0);

	m_scene = new graph::GraphScene(this);
	m_scene->setSceneRect(-3000, -3000, 6000, 6000);

	m_view = new graph::GraphView(m_scene, this);
	m_view->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_view, &QGraphicsView::customContextMenuRequested, this, &GraphEditorWindow::context_menu);

	top->addWidget(m_view);

	// DnD wiring
	connect(m_scene, &graph::GraphScene::edge_created, this,
		[this](graph::GraphNode *src, int sp, graph::GraphNode *dst, int dp) {
			// Prevent duplicates
			for (auto *e : m_edges) {
				if (e->source() == src && e->src_port() == sp &&
				    e->dest() == dst && e->dst_port() == dp)
					return;
			}
			auto *edge = new graph::GraphEdge(src, sp, dst, dp);
			m_scene->addItem(edge);
			m_edges.append(edge);
		});

	// Double-click properties
	connect(m_scene, &graph::GraphScene::node_double_clicked,
		this, &GraphEditorWindow::show_node_properties);
}

void GraphEditorWindow::add_node(graph::GraphNode::Type type, const QPointF &pos)
{
	int n = m_nodes.size() + 1;
	auto *node = new graph::GraphNode(type, graph::GraphNode::type_name(type) + " " + QString::number(n));
	QPointF p = pos.isNull() ? m_view->mapToScene(m_view->viewport()->rect().center()) : pos;
	node->setPos(p);
	m_scene->addItem(node);
	m_nodes.append(node);
}

void GraphEditorWindow::delete_selected()
{
	auto sel = m_scene->selectedItems();
	// Remove edges first
	for (auto *item : sel) {
		auto *edge = dynamic_cast<graph::GraphEdge*>(item);
		if (edge) {
			m_edges.removeOne(edge);
			m_scene->removeItem(edge);
			delete edge;
		}
	}
	// Then nodes (and their connected edges)
	for (auto *item : sel) {
		auto *node = dynamic_cast<graph::GraphNode*>(item);
		if (node) {
			for (int i = m_edges.size() - 1; i >= 0; i--) {
				if (m_edges[i]->source() == node || m_edges[i]->dest() == node) {
					m_scene->removeItem(m_edges[i]);
					delete m_edges.takeAt(i);
				}
			}
			m_nodes.removeOne(node);
			m_scene->removeItem(node);
			delete node;
		}
	}
}

void GraphEditorWindow::clear_graph()
{
	for (auto *e : m_edges) { m_scene->removeItem(e); delete e; }
	for (auto *n : m_nodes) { m_scene->removeItem(n); delete n; }
	m_edges.clear();
	m_nodes.clear();
}

void GraphEditorWindow::show_node_properties(graph::GraphNode *node)
{
	QDialog dlg(this);
	dlg.setWindowTitle("Node Properties — " + graph::GraphNode::type_name(node->node_type()));
	dlg.setStyleSheet(R"(
		QDialog { background:#16161e; color:#c8c8d8; }
		QLabel  { color:#a0a0b0; }
		QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
			background:#22222e; color:#e0e0f0; border:1px solid #3a3a4a;
			border-radius:4px; padding:4px 6px;
		}
		QDialogButtonBox QPushButton {
			background:#2a2a3a; color:#c0c0d0; border:1px solid #3a3a4a;
			border-radius:4px; padding:6px 16px;
		}
		QDialogButtonBox QPushButton:hover { background:#3a3a4a; }
	)");
	dlg.setMinimumWidth(320);

	auto *form = new QFormLayout(&dlg);
	form->setSpacing(8);
	form->setContentsMargins(16, 16, 16, 16);

	// Label
	auto *labelEdit = new QLineEdit(node->label(), &dlg);
	form->addRow("Label:", labelEdit);

	// Type (read-only)
	auto *typeLabel = new QLabel(graph::GraphNode::type_name(node->node_type()), &dlg);
	typeLabel->setStyleSheet("color:#6080c0; font-weight:bold;");
	form->addRow("Type:", typeLabel);

	// Type-specific properties
	QComboBox *subTypeCombo = nullptr;
	QDoubleSpinBox *param1Spin = nullptr;
	QDoubleSpinBox *param2Spin = nullptr;
	QLineEdit *portEdit = nullptr;

	switch (node->node_type()) {
	case graph::GraphNode::Filter:
		subTypeCombo = new QComboBox(&dlg);
		subTypeCombo->addItems({"Delay", "Debounce", "Rate Limit", "Deadzone", "Clamp", "Scale"});
		subTypeCombo->setCurrentText(node->properties["subtype"].toString("Delay"));
		form->addRow("Filter Type:", subTypeCombo);

		param1Spin = new QDoubleSpinBox(&dlg);
		param1Spin->setRange(0, 10000); param1Spin->setDecimals(2);
		param1Spin->setValue(node->properties["p1"].toDouble(0));
		form->addRow("Param 1:", param1Spin);

		param2Spin = new QDoubleSpinBox(&dlg);
		param2Spin->setRange(0, 10000); param2Spin->setDecimals(2);
		param2Spin->setValue(node->properties["p2"].toDouble(0));
		form->addRow("Param 2:", param2Spin);
		break;

	case graph::GraphNode::Interp:
		subTypeCombo = new QComboBox(&dlg);
		subTypeCombo->addItems({"Linear", "Quantize", "Smooth", "S-Curve", "Easing"});
		subTypeCombo->setCurrentText(node->properties["subtype"].toString("Linear"));
		form->addRow("Interp Type:", subTypeCombo);

		param1Spin = new QDoubleSpinBox(&dlg);
		param1Spin->setRange(0, 1000); param1Spin->setDecimals(3);
		param1Spin->setValue(node->properties["p1"].toDouble(0));
		form->addRow("Param 1:", param1Spin);
		break;

	case graph::GraphNode::Math:
		subTypeCombo = new QComboBox(&dlg);
		subTypeCombo->addItems({"Add", "Subtract", "Multiply", "Divide", "Clamp", "Abs", "Invert", "Map Range"});
		subTypeCombo->setCurrentText(node->properties["op"].toString("Add"));
		form->addRow("Operation:", subTypeCombo);

		param1Spin = new QDoubleSpinBox(&dlg);
		param1Spin->setRange(-1e6, 1e6); param1Spin->setDecimals(4);
		param1Spin->setValue(node->properties["value"].toDouble(0));
		form->addRow("Value:", param1Spin);
		break;

	case graph::GraphNode::Constant:
		param1Spin = new QDoubleSpinBox(&dlg);
		param1Spin->setRange(-1e6, 1e6); param1Spin->setDecimals(4);
		param1Spin->setValue(node->properties["value"].toDouble(0));
		form->addRow("Value:", param1Spin);
		break;

	case graph::GraphNode::MidiInput:
		portEdit = new QLineEdit(node->properties["channel"].toString("1"), &dlg);
		form->addRow("MIDI Channel:", portEdit);
		break;

	case graph::GraphNode::Output:
		portEdit = new QLineEdit(node->properties["port_id"].toString(), &dlg);
		form->addRow("Control Port:", portEdit);
		break;

	default:
		break;
	}

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	form->addRow(buttons);
	connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

	if (dlg.exec() == QDialog::Accepted) {
		node->set_label(labelEdit->text());

		QJsonObject props;
		switch (node->node_type()) {
		case graph::GraphNode::Filter:
			props["subtype"] = subTypeCombo->currentText();
			props["p1"] = param1Spin->value();
			props["p2"] = param2Spin->value();
			break;
		case graph::GraphNode::Interp:
			props["subtype"] = subTypeCombo->currentText();
			props["p1"] = param1Spin->value();
			break;
		case graph::GraphNode::Math:
			props["op"] = subTypeCombo->currentText();
			props["value"] = param1Spin->value();
			break;
		case graph::GraphNode::Constant:
			props["value"] = param1Spin->value();
			break;
		case graph::GraphNode::MidiInput:
			props["channel"] = portEdit->text();
			break;
		case graph::GraphNode::Output:
			props["port_id"] = portEdit->text();
			break;
		default: break;
		}
		node->properties = props;
	}
}

void GraphEditorWindow::context_menu(const QPoint &view_pos)
{
	QPointF scene_pos = m_view->mapToScene(view_pos);
	QMenu menu(this);

	// Add Node submenu
	auto *addMenu = menu.addMenu("➕ Add Node");
	for (int t = 0; t <= static_cast<int>(graph::GraphNode::Merger); t++) {
		auto type = static_cast<graph::GraphNode::Type>(t);
		addMenu->addAction(graph::GraphNode::type_name(type), this,
			[this, type, scene_pos]{ add_node(type, scene_pos); });
	}

	menu.addSeparator();

	// Selection-aware actions
	auto sel = m_scene->selectedItems();
	if (sel.size() == 2) {
		auto *n1 = dynamic_cast<graph::GraphNode*>(sel[0]);
		auto *n2 = dynamic_cast<graph::GraphNode*>(sel[1]);
		if (n1 && n2) {
			if (n1->out_count() > 0 && n2->in_count() > 0) {
				menu.addAction("🔗 Connect " + n1->label() + " → " + n2->label(), this,
					[this, n1, n2]{
						auto *e = new graph::GraphEdge(n1, 0, n2, 0);
						m_scene->addItem(e);
						m_edges.append(e);
					});
			}
			if (n2->out_count() > 0 && n1->in_count() > 0) {
				menu.addAction("🔗 Connect " + n2->label() + " → " + n1->label(), this,
					[this, n1, n2]{
						auto *e = new graph::GraphEdge(n2, 0, n1, 0);
						m_scene->addItem(e);
						m_edges.append(e);
					});
			}
			menu.addSeparator();
		}
	}

	if (!sel.isEmpty()) {
		menu.addAction("🗑 Delete Selected  (Del)", this, &GraphEditorWindow::delete_selected);
	}

	// Check if right-clicked on a specific node
	for (auto *it : m_scene->items(scene_pos)) {
		auto *node = dynamic_cast<graph::GraphNode*>(it);
		if (node) {
			menu.addAction("⚙ Properties…", this, [this, node]{ show_node_properties(node); });
			break;
		}
	}

	menu.addSeparator();
	menu.addAction("🔲 Select All  (Ctrl+A)", this, [this]{
		QPainterPath path;
		path.addRect(m_scene->sceneRect());
		m_scene->setSelectionArea(path);
	});
	menu.addAction("🧹 Clear All", this, &GraphEditorWindow::clear_graph);

	menu.exec(m_view->mapToGlobal(view_pos));
}
