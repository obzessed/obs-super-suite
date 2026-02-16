#include "surface_editor_window.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QPainterPath>
#include <QMimeData>
#include <QDrag>
#include <QJsonDocument>
#include <QFileDialog>
#include <QMessageBox>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QLabel>
#include <QGroupBox>
#include <QWheelEvent>
#include <QScrollBar>
#include <QtMath>

static const char *WIN_STYLE = R"(
QDialog    { background:#12121a; }
QSplitter::handle { background:#2a2a3a; width:2px; }
QListWidget {
	background:#16161e; color:#c8c8d8; border:1px solid #2a2a3a; font-size:12px;
	outline:none;
}
QListWidget::item {
	padding:8px 10px; border-bottom:1px solid #1e1e2a;
}
QListWidget::item:hover { background:#22223a; }
QListWidget::item:selected { background:#2d3390; }
QGroupBox {
	background:#16161e; border:1px solid #2a2a3a; border-radius:6px;
	margin-top:14px; padding:12px 8px 8px 8px; font-size:11px; color:#808090;
}
QGroupBox::title { subcontrol-position:top left; padding:0 6px; color:#6080c0; font-weight:bold; }
QLineEdit, QComboBox, QDoubleSpinBox {
	background:#22222e; color:#e0e0f0; border:1px solid #3a3a4a;
	border-radius:4px; padding:4px 6px; font-size:11px;
}
QCheckBox { color:#c0c0d0; font-size:11px; }
QPushButton {
	background:#2a2a3a; color:#c0c0d0; border:1px solid #3a3a4a;
	border-radius:4px; padding:6px 12px; font-size:11px;
}
QPushButton:hover { background:#3a3a4a; }
QPushButton#exportBtn { background:#1a6b30; border-color:#27ae60; }
QPushButton#exportBtn:hover { background:#27ae60; }
)";

namespace surf_ed {

// ===== SurfaceItem =========================================================

QString SurfaceItem::type_name(ElemType t) {
	static const char *n[] = {"Fader","HFader","Knob","Button","Toggle","Label","Encoder","XYPad"};
	return n[static_cast<int>(t)];
}

QColor SurfaceItem::type_color(ElemType t) {
	static const QColor c[] = {
		{60,140,210}, {60,140,210}, {130,105,245}, {210,70,70},
		{60,190,110}, {140,140,140},{210,170,50},  {170,110,190}
	};
	return c[static_cast<int>(t)];
}

QSizeF SurfaceItem::type_size(ElemType t) {
	switch (t) {
	case Fader:   return {40, 120};
	case HFader:  return {120, 40};
	case Knob:    return {60, 70};
	case Button:  return {80, 40};
	case Toggle:  return {60, 30};
	case Label:   return {100, 30};
	case Encoder: return {60, 70};
	case XYPad:   return {120, 120};
	}
	return {80, 60};
}

SurfaceItem::SurfaceItem(ElemType type, const QString &label, QGraphicsItem *parent)
	: QGraphicsItem(parent), m_type(type), m_label(label)
{
	setFlag(ItemIsMovable);
	setFlag(ItemIsSelectable);
	setFlag(ItemSendsGeometryChanges);
	setZValue(10);
}

QRectF SurfaceItem::boundingRect() const {
	QSizeF s = type_size(m_type);
	return QRectF(-2, -2, s.width() + 4, s.height() + 4);
}

void SurfaceItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) {
	p->setRenderHint(QPainter::Antialiasing);
	QSizeF sz = type_size(m_type);
	QColor base = type_color(m_type);
	bool sel = isSelected();

	// Shadow
	p->setPen(Qt::NoPen);
	p->setBrush(QColor(0, 0, 0, 45));
	p->drawRoundedRect(QRectF(2, 2, sz.width(), sz.height()), 6, 6);

	// Body
	QLinearGradient bg(0, 0, 0, sz.height());
	bg.setColorAt(0, base.lighter(sel ? 140 : 115));
	bg.setColorAt(1, base.darker(135));
	p->setBrush(bg);
	p->setPen(QPen(sel ? QColor(255, 255, 255, 200) : base.darker(160), sel ? 2.0 : 1.0));
	p->drawRoundedRect(QRectF(0, 0, sz.width(), sz.height()), 6, 6);

	// Type icon / visual hint
	QRectF inner(4, 4, sz.width() - 8, sz.height() - 8);
	p->setPen(QPen(QColor(255, 255, 255, 50), 1));
	p->setBrush(Qt::NoBrush);

	switch (m_type) {
	case Fader: {
		// Vertical track + thumb
		double cx = sz.width() / 2;
		p->drawLine(QPointF(cx, 8), QPointF(cx, sz.height() - 8));
		p->setBrush(QColor(255, 255, 255, 100));
		p->drawRoundedRect(QRectF(cx - 8, sz.height() * 0.4, 16, 8), 3, 3);
		break;
	}
	case HFader: {
		double cy = sz.height() / 2;
		p->drawLine(QPointF(8, cy), QPointF(sz.width() - 8, cy));
		p->setBrush(QColor(255, 255, 255, 100));
		p->drawRoundedRect(QRectF(sz.width() * 0.4, cy - 4, 8, 8), 3, 3);
		break;
	}
	case Knob:
	case Encoder: {
		double cx = sz.width() / 2, cy = sz.height() / 2 - 2;
		double r = qMin(sz.width(), sz.height()) / 2 - 10;
		p->setPen(QPen(QColor(255, 255, 255, 70), 2));
		p->drawArc(QRectF(cx - r, cy - r, r * 2, r * 2), -210 * 16, 240 * 16);
		// Indicator line
		p->setPen(QPen(base.lighter(180), 2));
		p->drawLine(QPointF(cx, cy), QPointF(cx + r * 0.7, cy - r * 0.3));
		break;
	}
	case Button:
		p->setBrush(QColor(255, 255, 255, 25));
		p->drawRoundedRect(inner, 4, 4);
		break;
	case Toggle:
		p->setBrush(QColor(255, 255, 255, 25));
		p->drawRoundedRect(QRectF(sz.width() - 32, (sz.height() - 14) / 2, 28, 14), 7, 7);
		break;
	case XYPad:
		p->drawLine(QPointF(sz.width() / 2, 4), QPointF(sz.width() / 2, sz.height() - 4));
		p->drawLine(QPointF(4, sz.height() / 2), QPointF(sz.width() - 4, sz.height() / 2));
		p->setBrush(QColor(255, 255, 255, 100));
		p->drawEllipse(QPointF(sz.width() * 0.6, sz.height() * 0.4), 4, 4);
		break;
	default:
		break;
	}

	// Label
	QFont f; f.setPixelSize(10);
	p->setFont(f);
	p->setPen(QColor(255, 255, 255, 200));
	double labelY = (m_type == Knob || m_type == Encoder) ? sz.height() - 16 : 2;
	p->drawText(QRectF(2, labelY, sz.width() - 4, 14), Qt::AlignCenter, m_label);
}

QJsonObject SurfaceItem::to_json() const {
	QJsonObject o;
	o["type"] = type_name(m_type);
	o["label"] = m_label;
	o["id"] = m_label.toLower().replace(' ', '_');
	o["port"] = port_binding;
	o["x"] = pos().x();
	o["y"] = pos().y();
	o["min"] = min_val;
	o["max"] = max_val;
	o["default"] = default_val;
	if (checkable) o["checkable"] = true;
	return o;
}

SurfaceItem *SurfaceItem::from_json(const QJsonObject &o) {
	int type_idx = 0;
	QString tname = o["type"].toString();
	for (int i = 0; i <= static_cast<int>(XYPad); i++) {
		if (type_name(static_cast<ElemType>(i)) == tname) { type_idx = i; break; }
	}
	auto *item = new SurfaceItem(static_cast<ElemType>(type_idx), o["label"].toString());
	item->setPos(o["x"].toDouble(), o["y"].toDouble());
	item->port_binding = o["port"].toString();
	item->min_val = o["min"].toDouble(0);
	item->max_val = o["max"].toDouble(1);
	item->default_val = o["default"].toDouble(0);
	item->checkable = o["checkable"].toBool(false);
	return item;
}

// ===== CanvasScene =========================================================

CanvasScene::CanvasScene(QObject *parent) : QGraphicsScene(parent) {}

void CanvasScene::dragEnterEvent(QGraphicsSceneDragDropEvent *e) {
	if (e->mimeData()->hasFormat("application/x-surface-element"))
		e->acceptProposedAction();
}

void CanvasScene::dragMoveEvent(QGraphicsSceneDragDropEvent *e) {
	if (e->mimeData()->hasFormat("application/x-surface-element"))
		e->acceptProposedAction();
}

void CanvasScene::dropEvent(QGraphicsSceneDragDropEvent *e) {
	if (!e->mimeData()->hasFormat("application/x-surface-element")) return;
	int type = e->mimeData()->data("application/x-surface-element").toInt();
	// Snap to grid
	QPointF pos = e->scenePos();
	pos.setX(std::round(pos.x() / GRID) * GRID);
	pos.setY(std::round(pos.y() / GRID) * GRID);
	emit item_dropped(static_cast<SurfaceItem::ElemType>(type), pos);
	e->acceptProposedAction();
}

void CanvasScene::mousePressEvent(QGraphicsSceneMouseEvent *e) {
	QGraphicsScene::mousePressEvent(e);
	auto *item = dynamic_cast<SurfaceItem*>(itemAt(e->scenePos(), QTransform()));
	emit item_selected(item); // null if nothing clicked
}

// ===== CanvasView ==========================================================

CanvasView::CanvasView(QGraphicsScene *scene, QWidget *parent)
	: QGraphicsView(scene, parent)
{
	setRenderHint(QPainter::Antialiasing);
	setAcceptDrops(true);
	setDragMode(RubberBandDrag);
	setTransformationAnchor(AnchorUnderMouse);
	setViewportUpdateMode(FullViewportUpdate);
}

void CanvasView::wheelEvent(QWheelEvent *e) {
	double f = (e->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
	scale(f, f);
}

void CanvasView::drawBackground(QPainter *p, const QRectF &rect) {
	p->fillRect(rect, QColor(18, 18, 26));

	double grid = CanvasScene::GRID;
	double left = std::floor(rect.left() / grid) * grid;
	double top  = std::floor(rect.top()  / grid) * grid;

	p->setPen(Qt::NoPen);
	p->setBrush(QColor(50, 50, 70, 35));
	for (double x = left; x < rect.right(); x += grid)
		for (double y = top; y < rect.bottom(); y += grid)
			p->drawRect(QRectF(x, y, 1.5, 1.5));
}

} // namespace surf_ed

// ═══════════════════════════════════════════════════════════════════════════
// SurfaceEditorWindow
// ═══════════════════════════════════════════════════════════════════════════

SurfaceEditorWindow::SurfaceEditorWindow(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle("Surface Editor");
	setMinimumSize(900, 550);
	resize(1200, 700);
	setStyleSheet(WIN_STYLE);
	setup_ui();
}

void SurfaceEditorWindow::setup_ui()
{
	auto *top = new QVBoxLayout(this);
	top->setContentsMargins(0, 0, 0, 0);
	top->setSpacing(0);

	// Bottom bar
	auto *bottomBar = new QHBoxLayout();
	bottomBar->setContentsMargins(8, 4, 8, 4);
	auto *importBtn = new QPushButton("📂 Import JSON", this);
	auto *exportBtn = new QPushButton("💾 Export JSON", this);
	exportBtn->setObjectName("exportBtn");
	auto *clearBtn = new QPushButton("🧹 Clear", this);
	bottomBar->addWidget(importBtn);
	bottomBar->addWidget(exportBtn);
	bottomBar->addStretch();
	bottomBar->addWidget(clearBtn);

	// Main splitter
	auto *splitter = new QSplitter(Qt::Horizontal, this);

	// === Left: Palette ===
	auto *leftPanel = new QWidget(splitter);
	auto *lv = new QVBoxLayout(leftPanel);
	lv->setContentsMargins(4, 4, 0, 4);
	lv->setSpacing(4);

	auto *palLabel = new QLabel("Elements", leftPanel);
	palLabel->setStyleSheet("color:#6080c0; font-weight:bold; font-size:12px; padding:4px;");
	lv->addWidget(palLabel);

	m_palette = new QListWidget(leftPanel);
	m_palette->setDragEnabled(true);
	m_palette->setFixedWidth(140);
	lv->addWidget(m_palette);

	populate_palette();

	// Drag initiation from palette
	connect(m_palette, &QListWidget::itemPressed, this, [this](QListWidgetItem *item) {
		int type = item->data(Qt::UserRole).toInt();
		auto *drag = new QDrag(this);
		auto *mime = new QMimeData();
		mime->setData("application/x-surface-element", QByteArray::number(type));
		drag->setMimeData(mime);
		drag->exec(Qt::CopyAction);
	});

	splitter->addWidget(leftPanel);

	// === Center: Canvas ===
	m_canvas_scene = new surf_ed::CanvasScene(this);
	m_canvas_scene->setSceneRect(-1000, -1000, 2000, 2000);

	m_canvas_view = new surf_ed::CanvasView(m_canvas_scene, this);
	splitter->addWidget(m_canvas_view);

	// === Right: Properties ===
	m_props_panel = new QWidget(splitter);
	m_props_panel->setFixedWidth(220);
	auto *rv = new QVBoxLayout(m_props_panel);
	rv->setContentsMargins(4, 4, 4, 4);
	rv->setSpacing(4);

	auto *propsBox = new QGroupBox("Properties", m_props_panel);
	auto *pf = new QFormLayout(propsBox);
	pf->setSpacing(6);

	m_prop_label = new QLineEdit(propsBox);
	m_prop_label->setPlaceholderText("Element label");
	pf->addRow("Label:", m_prop_label);

	m_prop_type = new QComboBox(propsBox);
	for (int i = 0; i <= static_cast<int>(surf_ed::SurfaceItem::XYPad); i++)
		m_prop_type->addItem(surf_ed::SurfaceItem::type_name(static_cast<surf_ed::SurfaceItem::ElemType>(i)), i);
	m_prop_type->setEnabled(false); // read-only for now
	pf->addRow("Type:", m_prop_type);

	m_prop_port = new QLineEdit(propsBox);
	m_prop_port->setPlaceholderText("e.g. MyDock.slider1");
	pf->addRow("Port:", m_prop_port);

	m_prop_min = new QDoubleSpinBox(propsBox);
	m_prop_min->setRange(-1e6, 1e6); m_prop_min->setDecimals(3);
	pf->addRow("Min:", m_prop_min);

	m_prop_max = new QDoubleSpinBox(propsBox);
	m_prop_max->setRange(-1e6, 1e6); m_prop_max->setDecimals(3); m_prop_max->setValue(1.0);
	pf->addRow("Max:", m_prop_max);

	m_prop_default = new QDoubleSpinBox(propsBox);
	m_prop_default->setRange(-1e6, 1e6); m_prop_default->setDecimals(3);
	pf->addRow("Default:", m_prop_default);

	m_prop_checkable = new QCheckBox("Checkable", propsBox);
	pf->addRow(m_prop_checkable);

	rv->addWidget(propsBox);
	rv->addStretch();

	splitter->addWidget(m_props_panel);
	splitter->setStretchFactor(0, 0); // palette fixed
	splitter->setStretchFactor(1, 1); // canvas stretches
	splitter->setStretchFactor(2, 0); // props fixed

	top->addWidget(splitter, 1);
	top->addLayout(bottomBar);

	// Connections
	connect(m_canvas_scene, &surf_ed::CanvasScene::item_dropped, this,
		[this](surf_ed::SurfaceItem::ElemType type, const QPointF &pos) {
			auto name = surf_ed::SurfaceItem::type_name(type);
			auto *item = new surf_ed::SurfaceItem(type, name + " " + QString::number(m_items.size() + 1));
			item->setPos(pos);
			m_canvas_scene->addItem(item);
			m_items.append(item);
		});

	connect(m_canvas_scene, &surf_ed::CanvasScene::item_selected, this,
		&SurfaceEditorWindow::on_item_selected);

	// Property changes → update selected item
	connect(m_prop_label, &QLineEdit::textChanged, this, &SurfaceEditorWindow::update_props_from_ui);
	connect(m_prop_port, &QLineEdit::textChanged, this, &SurfaceEditorWindow::update_props_from_ui);
	connect(m_prop_min, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SurfaceEditorWindow::update_props_from_ui);
	connect(m_prop_max, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SurfaceEditorWindow::update_props_from_ui);
	connect(m_prop_default, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SurfaceEditorWindow::update_props_from_ui);
	connect(m_prop_checkable, &QCheckBox::toggled, this, &SurfaceEditorWindow::update_props_from_ui);

	connect(importBtn, &QPushButton::clicked, this, &SurfaceEditorWindow::import_schema);
	connect(exportBtn, &QPushButton::clicked, this, &SurfaceEditorWindow::export_schema);
	connect(clearBtn,  &QPushButton::clicked, this, [this]{
		for (auto *i : m_items) { m_canvas_scene->removeItem(i); delete i; }
		m_items.clear();
		m_selected = nullptr;
		on_item_selected(nullptr);
	});
}

void SurfaceEditorWindow::populate_palette()
{
	static const struct { const char *icon; const char *name; int type; } palette_items[] = {
		{"▐",  "Fader",   0}, {"━",  "HFader",  1}, {"◎",  "Knob",    2},
		{"⏺",  "Button",  3}, {"⏼",  "Toggle",  4}, {"Aa", "Label",   5},
		{"↻",  "Encoder", 6}, {"✛",  "XY Pad",  7},
	};
	for (auto &p : palette_items) {
		auto *item = new QListWidgetItem(QString("%1  %2").arg(p.icon, p.name), m_palette);
		item->setData(Qt::UserRole, p.type);
	}
}

void SurfaceEditorWindow::on_item_selected(surf_ed::SurfaceItem *item)
{
	m_selected = item;
	bool has = (item != nullptr);

	m_prop_label->blockSignals(true);
	m_prop_port->blockSignals(true);
	m_prop_min->blockSignals(true);
	m_prop_max->blockSignals(true);
	m_prop_default->blockSignals(true);
	m_prop_checkable->blockSignals(true);

	if (has) {
		m_prop_label->setText(item->label());
		m_prop_type->setCurrentIndex(static_cast<int>(item->elem_type()));
		m_prop_port->setText(item->port_binding);
		m_prop_min->setValue(item->min_val);
		m_prop_max->setValue(item->max_val);
		m_prop_default->setValue(item->default_val);
		m_prop_checkable->setChecked(item->checkable);
	} else {
		m_prop_label->clear();
		m_prop_port->clear();
		m_prop_min->setValue(0);
		m_prop_max->setValue(1);
		m_prop_default->setValue(0);
		m_prop_checkable->setChecked(false);
	}

	m_prop_label->setEnabled(has);
	m_prop_port->setEnabled(has);
	m_prop_min->setEnabled(has);
	m_prop_max->setEnabled(has);
	m_prop_default->setEnabled(has);
	m_prop_checkable->setEnabled(has);

	m_prop_label->blockSignals(false);
	m_prop_port->blockSignals(false);
	m_prop_min->blockSignals(false);
	m_prop_max->blockSignals(false);
	m_prop_default->blockSignals(false);
	m_prop_checkable->blockSignals(false);
}

void SurfaceEditorWindow::update_props_from_ui()
{
	if (!m_selected) return;
	m_selected->set_label(m_prop_label->text());
	m_selected->port_binding = m_prop_port->text();
	m_selected->min_val = m_prop_min->value();
	m_selected->max_val = m_prop_max->value();
	m_selected->default_val = m_prop_default->value();
	m_selected->checkable = m_prop_checkable->isChecked();
}

void SurfaceEditorWindow::export_schema()
{
	QString path = QFileDialog::getSaveFileName(this, "Export Surface Schema", QString(), "JSON (*.json)");
	if (path.isEmpty()) return;

	QJsonObject schema;
	schema["name"] = "Exported Surface";
	schema["columns"] = 4;
	QJsonArray elements;
	for (auto *item : m_items)
		elements.append(item->to_json());
	schema["elements"] = elements;

	QFile f(path);
	if (f.open(QIODevice::WriteOnly))
		f.write(QJsonDocument(schema).toJson(QJsonDocument::Indented));
}

void SurfaceEditorWindow::import_schema()
{
	QString path = QFileDialog::getOpenFileName(this, "Import Surface Schema", QString(), "JSON (*.json);;All (*)");
	if (path.isEmpty()) return;

	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) return;

	QJsonParseError err;
	auto doc = QJsonDocument::fromJson(f.readAll(), &err);
	if (doc.isNull()) {
		QMessageBox::warning(this, "Parse Error", err.errorString());
		return;
	}

	// Clear existing
	for (auto *i : m_items) { m_canvas_scene->removeItem(i); delete i; }
	m_items.clear();
	m_selected = nullptr;
	on_item_selected(nullptr);

	// Load elements
	auto schema = doc.object();
	auto elements = schema["elements"].toArray();
	for (auto v : elements) {
		auto *item = surf_ed::SurfaceItem::from_json(v.toObject());
		m_canvas_scene->addItem(item);
		m_items.append(item);
	}
}
