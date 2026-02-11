#include "encoding_graph_window.h"

#include "plugin-support.h"

#include <QMap>
#include <QVBoxLayout>
#include <QPainter>
#include <QBrush>
#include <QPen>
#include <QFontMetrics>
#include <QIODevice>
#include <QTimer>
#include <algorithm>
#include <QMimeData>
#include <QDrag>
#include <QApplication>
#include <QVector2D>
#include <QDataStream>
#include <QGraphicsSceneDragDropEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>
#include <obs-frontend-api.h>

// ----------------------------------------------------------------------------
// GraphNode
// ----------------------------------------------------------------------------

GraphNode::GraphNode(const QString &title, NodeType type, const QString &subtext)
	: m_title(title),
	  m_subtext(subtext),
	  m_type(type)
{
	setFlags(ItemIsMovable | ItemSendsGeometryChanges | ItemIsSelectable);
	setZValue(1);

	// Set nice size based on type
	if (m_type == NodeType::Encoder) {
		m_width = 240;
		m_height = 90;
	} else if (m_type == NodeType::Output) {
		m_width = 220;
		m_height = 80;
	} else if (m_type == NodeType::Canvas) {
		m_width = 260;
		m_height = 180; // Taller to simulate preview
	} else if (m_type == NodeType::AudioMixer) {
		m_width = 200;
		m_height = 160; // Tall enough for tracks
	} else if (m_type == NodeType::Scene) {
		m_width = 220;
		m_height = 80;
	} else if (m_type == NodeType::Transition) {
		m_width = 200;
		m_height = 70;
	} else {
		m_width = 200;
		m_height = 70;
	}

	setToolTip(m_subtext); // Show detailed info on hover
}

GraphNode::~GraphNode()
{
	if (m_weakSource)
		obs_weak_source_release(m_weakSource);
	if (m_weakEncoder)
		obs_weak_encoder_release(m_weakEncoder);
	if (m_weakOutput)
		obs_weak_output_release(m_weakOutput);
	if (m_canvas)
		obs_canvas_release(m_canvas);
}

void GraphNode::setSource(obs_source_t *source)
{
	if (m_weakSource)
		obs_weak_source_release(m_weakSource);
	m_weakSource = obs_source_get_weak_source(source);
}

void GraphNode::setEncoder(obs_encoder_t *encoder)
{
	if (m_weakEncoder)
		obs_weak_encoder_release(m_weakEncoder);
	m_weakEncoder = obs_encoder_get_weak_encoder(encoder);
}

void GraphNode::setOutput(obs_output_t *output)
{
	if (m_weakOutput)
		obs_weak_output_release(m_weakOutput);
	m_weakOutput = obs_output_get_weak_output(output);
}

void GraphNode::setCanvas(obs_canvas_t *canvas)
{
	if (m_canvas)
		obs_canvas_release(m_canvas);
	m_canvas = canvas;
	if (m_canvas)
		obs_canvas_get_ref(m_canvas);
}

obs_source_t *GraphNode::getSourceRef() const
{
	if (m_weakSource)
		return obs_weak_source_get_source(m_weakSource);
	return nullptr;
}

obs_encoder_t *GraphNode::getEncoderRef() const
{
	if (m_weakEncoder)
		return obs_weak_encoder_get_encoder(m_weakEncoder);
	return nullptr;
}

obs_output_t *GraphNode::getOutputRef() const
{
	if (m_weakOutput)
		return obs_weak_output_get_output(m_weakOutput);
	return nullptr;
}

obs_canvas_t *GraphNode::getCanvasRef() const
{
	if (m_canvas) {
		obs_canvas_get_ref(m_canvas);
		return m_canvas;
	}
	return nullptr;
}

void GraphNode::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
	QGraphicsItem::mouseDoubleClickEvent(event);

	if (m_type == NodeType::VideoInput || m_type == NodeType::AudioInput || m_type == NodeType::MediaInput ||
	    m_type == NodeType::Scene || m_type == NodeType::Transition) {
		if (obs_source_t *source = getSourceRef()) {
			obs_frontend_open_source_properties(source);
			obs_source_release(source);
		}
	} else if (m_type == NodeType::Canvas) {
		// Canvas properties? Not really standard.
	}
}

void GraphNode::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	QGraphicsItem::mouseReleaseEvent(event);
}

QRectF GraphNode::boundingRect() const
{
	return QRectF(-10, -10, m_width + 20, m_height + 20);
}

QColor GraphNode::getHeaderColor() const
{
	switch (m_type) {
	case NodeType::Canvas:
		return QColor(30, 30, 30);
	case NodeType::VideoInput:
		return QColor(30, 30, 30);
	case NodeType::AudioInput:
		return QColor(30, 30, 30);
	case NodeType::MediaInput:
		return QColor(30, 30, 30);
	case NodeType::Scene:
		return QColor(40, 30, 40); // Slightly reddish dark
	case NodeType::Transition:
		return QColor(30, 30, 30);
	case NodeType::Encoder:
		return QColor(30, 30, 30);
	case NodeType::Output:
		return QColor(30, 30, 30);
	default:
		return QColor(30, 30, 30);
	}
}

QColor GraphNode::getBodyColor() const
{
	return QColor(43, 43, 43); // Dark Grey Card
}

QColor GraphNode::getBorderColor() const
{
	switch (m_type) {
	case NodeType::Canvas:
		return QColor(200, 200, 200); // White/Grey
	case NodeType::VideoInput:
		return QColor(60, 100, 160); // Blue
	case NodeType::AudioInput:
		return QColor(60, 160, 100); // Green
	case NodeType::MediaInput:
		return QColor(60, 160, 160); // Cyan/Teal
	case NodeType::Scene:
		return QColor(160, 60, 100); // Magenta/Reddish
	case NodeType::Transition:
		return QColor(100, 100, 100); // Grey
	case NodeType::Encoder:
		return QColor(160, 100, 60); // Orange
	case NodeType::Output:
		return QColor(160, 60, 160); // Purple
	default:
		return QColor(100, 100, 100);
	}
}

void GraphNode::addInputPort(const QString &id, const QString &label)
{
	for (const auto &p : m_inputPorts) {
		if (p.id == id)
			return;
	}

	NodePort p;
	p.id = id;
	p.label = label;
	// Calculate Y offset relative to top.
	// Header is ~30px.
	// Let's start at 40.
	p.yOffset = 40 + m_inputPorts.size() * 20;
	m_inputPorts.append(p);

	if (p.yOffset + 20 > m_height) {
		m_height = p.yOffset + 25;
		prepareGeometryChange();
	}
}

void GraphNode::addOutputPort(const QString &id, const QString &label)
{
	for (const auto &p : m_outputPorts) {
		if (p.id == id)
			return;
	}

	NodePort p;
	p.id = id;
	p.label = label;
	// Output ports might align with inputs or be independent.
	// For Canvas, outputs are the tracks + main video.
	// Let's stack them similarly for now.
	p.yOffset = 40 + m_outputPorts.size() * 20;
	m_outputPorts.append(p);

	if (p.yOffset + 20 > m_height) {
		m_height = p.yOffset + 25;
		prepareGeometryChange();
	}
}

QPointF GraphNode::getInputPortPosition(const QString &id) const
{
	for (const auto &p : m_inputPorts) {
		if (p.id == id) {
			return QPointF(0, p.yOffset);
		}
	}
	return leftPort();
}

QPointF GraphNode::getOutputPortPosition(const QString &id) const
{
	for (const auto &p : m_outputPorts) {
		if (p.id == id) {
			return QPointF(m_width, p.yOffset);
		}
	}
	return rightPort();
}

void GraphNode::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	QRectF bodyRect(0, 0, m_width, m_height);

	// ---------------------------------------------------------
	// 1. Draw Port Dots (BEHIND Body)
	// ---------------------------------------------------------
	painter->setPen(Qt::NoPen);

	constexpr int PORT_RADIUS = 8; // For drawing, actual hit area is larger in shape()

	// Input Ports Dots
	for (const auto &p : m_inputPorts) {
		QPointF pos(0, p.yOffset);

		// Color Code
		QColor portColor(220, 220, 220); // Default Grey
		if (p.id.contains("video", Qt::CaseInsensitive))
			portColor = QColor(100, 150, 255); // Blue
		else if (p.id.contains("audio", Qt::CaseInsensitive) || p.id.startsWith("track"))
			portColor = QColor(100, 255, 150); // Green

		painter->setBrush(portColor);
		painter->drawEllipse(pos, PORT_RADIUS, PORT_RADIUS);
	}

	// Output Ports Dots
	for (const auto &p : m_outputPorts) {
		QPointF pos(m_width, p.yOffset);

		QColor portColor(220, 220, 220);
		if (p.id.contains("video", Qt::CaseInsensitive))
			portColor = QColor(100, 150, 255);
		else if (p.id.contains("audio", Qt::CaseInsensitive) || p.id.startsWith("track"))
			portColor = QColor(100, 255, 150);

		painter->setBrush(portColor);
		painter->drawEllipse(pos, PORT_RADIUS, PORT_RADIUS);
	}

	// Default Ports (if no specific ports)
	// TODO: no longer needed?
	if (m_inputPorts.isEmpty() && m_outputPorts.isEmpty()) {
		painter->setBrush(QColor(220, 220, 220));
		if (m_type != NodeType::Canvas && m_type != NodeType::AudioMixer) {
			if (m_type != NodeType::VideoInput && m_type != NodeType::AudioInput &&
			    m_type != NodeType::MediaInput && m_type != NodeType::Scene &&
			    m_type != NodeType::Transition)
				painter->drawEllipse(leftPort() + QPointF(-2.5, -2.5), 5, 5);
			if (m_type != NodeType::Output)
				painter->drawEllipse(rightPort() + QPointF(-2.5, -2.5), 5, 5);
		}
	}

	// ---------------------------------------------------------
	// 2. Draw Body
	// ---------------------------------------------------------
	painter->setBrush(getBodyColor());
	if (isSelected()) {
		painter->setPen(QPen(QColor(255, 200, 0), 2));
	} else {
		painter->setPen(QPen(getBorderColor(), 1));
	}
	painter->drawRoundedRect(bodyRect, 6, 6);

	// ---------------------------------------------------------
	// 3. Decorations
	// ---------------------------------------------------------

	// Type Indicator (Small Colored Dot)
	QColor indicatorColor = getBorderColor();
	painter->setBrush(indicatorColor);
	painter->setPen(Qt::NoPen);
	painter->drawEllipse(10, 10, 8, 8);

	// Globe Icon (if enabled)
	if (m_showGlobe) {
		painter->setPen(QPen(QColor(100, 180, 255), 1));
		painter->setBrush(Qt::NoBrush);
		painter->drawEllipse(m_width - 24, 6, 14, 14);
		painter->drawLine(m_width - 24, 13, m_width - 10, 13);
		painter->drawLine(m_width - 17, 6, m_width - 17, 20);
		painter->drawEllipse(m_width - 21, 6, 8, 14);
	}

	// Title
	painter->setPen(Qt::white);
	QFont titleFont = painter->font();
	titleFont.setBold(true);
	titleFont.setPointSize(9);
	painter->setFont(titleFont);
	painter->drawText(QRectF(25, 4, m_width - 30, 20), Qt::AlignLeft | Qt::AlignVCenter, m_title);

	// Divider
	painter->setPen(QPen(QColor(60, 60, 60), 1));
	painter->drawLine(10, 28, m_width - 10, 28);

	// ---------------------------------------------------------
	// 4. Port Labels (Text)
	// ---------------------------------------------------------
	QFont portFont = painter->font();
	portFont.setPointSize(8);
	portFont.setBold(false);
	painter->setFont(portFont);
	painter->setPen(QColor(200, 200, 200));

	const int LABEL_MARGIN = 12;

	// Input Labels
	for (const auto &p : m_inputPorts) {
		painter->drawText(QRectF(LABEL_MARGIN, p.yOffset - 10, m_width / 2 - LABEL_MARGIN, 20),
				  Qt::AlignLeft | Qt::AlignVCenter, p.label);
	}

	// Output Labels
	for (const auto &p : m_outputPorts) {
		painter->drawText(QRectF(m_width / 2, p.yOffset - 10, m_width / 2 - LABEL_MARGIN, 20),
				  Qt::AlignRight | Qt::AlignVCenter, p.label);
	}

	// Canvas Content Display (Always draw if Canvas)
	if (m_type == NodeType::Canvas) {
		// Draw a preview rectangle in the middle
		QRectF previewRect(10, 35, m_width - 20, m_height - 45);
		painter->setBrush(Qt::black);
		painter->setPen(QPen(QColor(60, 60, 60), 1));
		painter->drawRect(previewRect);

		// Optional: Draw text "Preview"
		painter->setPen(QColor(100, 100, 100));
		painter->drawText(previewRect, Qt::AlignCenter, "Content Display");
	}
}

QPointF GraphNode::leftPort() const
{
	return QPointF(0, m_height / 2);
}

QPointF GraphNode::rightPort() const
{
	return QPointF(m_width, m_height / 2);
}

void GraphNode::addEdge(GraphEdge *edge)
{
	m_edges.append(edge);
}

QVariant GraphNode::itemChange(GraphicsItemChange change, const QVariant &value)
{
	if (change == ItemPositionHasChanged && scene()) {
		for (GraphEdge *edge : m_edges) {
			edge->updatePath();
		}
	}
	return QGraphicsItem::itemChange(change, value);
}

// ----------------------------------------------------------------------------
// GraphEdge
// ----------------------------------------------------------------------------

GraphEdge::GraphEdge(GraphNode *start, GraphNode *end, const QString &startPortId, const QString &endPortId)
	: m_start(start),
	  m_end(end),
	  m_startPortId(startPortId),
	  m_endPortId(endPortId)
{
	// Selection disabled by default
	// setFlags(ItemIsSelectable);
	setAcceptHoverEvents(true);
	setZValue(0);

	// Determine color based on port ID (similar to NodePort logic)
	m_baseColor = QColor(150, 150, 150); // Default Grey
	if (m_startPortId.contains("video", Qt::CaseInsensitive)) {
		m_baseColor = QColor(100, 150, 255); // Blue
	} else if (m_startPortId.contains("audio", Qt::CaseInsensitive) || m_startPortId.startsWith("track")) {
		m_baseColor = QColor(100, 255, 150); // Green
	}

	setPen(QPen(m_baseColor, 2));
	updatePath();
}

QRectF GraphEdge::boundingRect() const
{
	qreal margin = 5.0;
	return QGraphicsPathItem::boundingRect().adjusted(-margin, -margin, margin, margin);
}

QVariant GraphEdge::itemChange(GraphicsItemChange change, const QVariant &value)
{
	if (change == ItemSelectedHasChanged) {
		if (value.toBool()) {
			setPen(QPen(QColor(255, 200, 0), 3)); // Gold, thicker when selected
		} else {
			setPen(QPen(m_baseColor, 2)); // Default
		}
	}
	return QGraphicsPathItem::itemChange(change, value);
}

void GraphEdge::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	Q_UNUSED(widget);

	// Custom painting if needed, or just default
	// We can add a halo if selected?
	if (isSelected()) {
		painter->save();
		painter->setPen(QPen(QColor(255, 200, 0, 100), 6));
		painter->drawPath(path());
		painter->restore();
	}

	// Draw the main line
	QGraphicsPathItem::paint(painter, option, widget);
}

QPainterPath GraphEdge::shape() const
{
	QPainterPathStroker stroker;
	stroker.setWidth(10); // Hit area width
	return stroker.createStroke(path());
}

void GraphEdge::updatePath()
{
	if (!m_start || !m_end)
		return;

	QPointF startPos;
	QString startLabel = "Default";
	if (m_startPortId.isEmpty()) {
		startPos = m_start->pos() + m_start->rightPort();
	} else {
		startPos = m_start->pos() + m_start->getOutputPortPosition(m_startPortId);
		startLabel = m_startPortId;
	}

	QPointF endPos;
	QString endLabel = "Default";
	if (m_endPortId.isEmpty()) {
		endPos = m_end->pos() + m_end->leftPort();
	} else {
		endPos = m_end->pos() + m_end->getInputPortPosition(m_endPortId);
		endLabel = m_endPortId;
	}

	// Update Tooltip
	setToolTip(QString("%1 (%2) -> %3 (%4)").arg(m_start->title(), startLabel, m_end->title(), endLabel));

	QPainterPath path;
	path.moveTo(startPos);

	qreal dx = endPos.x() - startPos.x();
	QPointF ctrl1(startPos.x() + dx * 0.5, startPos.y());
	QPointF ctrl2(endPos.x() - dx * 0.5, endPos.y());

	path.cubicTo(ctrl1, ctrl2, endPos);
	setPath(path);
}

// ----------------------------------------------------------------------------
// GraphScene
// ----------------------------------------------------------------------------

GraphScene::GraphScene(QObject *parent) : QGraphicsScene(parent) {}

GraphScene::~GraphScene() {}

// ----------------------------------------------------------------------------
// GraphView
// ----------------------------------------------------------------------------

class GraphView : public QGraphicsView {
public:
	GraphView(QWidget *parent = nullptr) : QGraphicsView(parent)
	{
		setRenderHint(QPainter::Antialiasing);
		setDragMode(QGraphicsView::RubberBandDrag);
		setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
		setResizeAnchor(QGraphicsView::AnchorUnderMouse);
		viewport()->setCursor(Qt::ArrowCursor);
	}

protected:
	void keyPressEvent(QKeyEvent *event) override
	{
		if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
			m_isSpacePressed = true;
			// When space is pressed, change cursor to indicate panning is available
			viewport()->setCursor(Qt::OpenHandCursor);
			event->accept();
		} else {
			QGraphicsView::keyPressEvent(event);
		}
	}

	void keyReleaseEvent(QKeyEvent *event) override
	{
		if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
			m_isSpacePressed = false;
			// Reset cursor if not currently dragging
			if (!m_isPanning) {
				viewport()->setCursor(Qt::ArrowCursor);
			}
			event->accept();
		} else {
			QGraphicsView::keyReleaseEvent(event);
		}
	}

	void wheelEvent(QWheelEvent *event) override
	{
		if (event->modifiers() & Qt::ControlModifier) {
			setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
			const double zoomFactor = 1.1;
			if (event->angleDelta().y() > 0)
				scale(zoomFactor, zoomFactor);
			else
				scale(1.0 / zoomFactor, 1.0 / zoomFactor);
			event->accept();
		} else if (event->modifiers() & Qt::ShiftModifier) {
			// Horizontal scrolling with Shift + Wheel
			QPoint delta = event->angleDelta();
			int hDelta = delta.y(); // Use vertical wheel for horizontal scroll
			if (delta.isNull())
				return;

			// Adjust speed factor as needed
			horizontalScrollBar()->setValue(horizontalScrollBar()->value() - hDelta);
			event->accept();
		} else {
			QGraphicsView::wheelEvent(event);
		}
	}

	void mousePressEvent(QMouseEvent *event) override
	{
		if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && m_isSpacePressed)) {
			m_isPanning = true;
			m_lastPanPos = event->pos();
			viewport()->setCursor(Qt::ClosedHandCursor);
			event->accept();
			return;
		}
		QGraphicsView::mousePressEvent(event);
	}

	void mouseMoveEvent(QMouseEvent *event) override
	{
		if (m_isPanning) {
			QPoint delta = event->pos() - m_lastPanPos;
			m_lastPanPos = event->pos();
			horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
			verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
			event->accept();
			return;
		}
		QGraphicsView::mouseMoveEvent(event);
	}

	void mouseReleaseEvent(QMouseEvent *event) override
	{
		if (m_isPanning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
			m_isPanning = false;
			// Restore cursor based on space key state
			if (m_isSpacePressed)
				viewport()->setCursor(Qt::OpenHandCursor);
			else
				viewport()->setCursor(Qt::ArrowCursor);
			event->accept();
			return;
		}
		QGraphicsView::mouseReleaseEvent(event);
	}

private:
	bool m_isPanning = false;
	bool m_isSpacePressed = false;
	QPoint m_lastPanPos;
};

// ----------------------------------------------------------------------------
// EncodingGraphWindow
// ----------------------------------------------------------------------------

EncodingGraphWindow::EncodingGraphWindow(QWidget *parent) : QMainWindow(parent)
{
	// Make it a normal window with min/max buttons
	// setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint); // QMainWindow handles this by default
	// setWindowState(Qt::WindowMaximized); // Keep this if desired

	setWindowTitle("Encoding Graph");
	resize(1000, 600);

	QWidget *central = new QWidget(this);
	setCentralWidget(central);
	QVBoxLayout *layout = new QVBoxLayout(central);

	m_view = new GraphView(this);
	m_view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
	m_scene = new GraphScene(this);

	// Set dark background
	m_scene->setBackgroundBrush(QColor(30, 30, 30));
	m_view->setScene(m_scene);

	// Context Menu
	m_view->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_view, &QWidget::customContextMenuRequested, [this](const QPoint &pos) {
		QMenu menu;
		menu.addAction("Refresh Graph", this, &EncodingGraphWindow::refresh);
		menu.addSeparator();
		menu.addAction("Reset Layout", this, &EncodingGraphWindow::layoutGraph);
		menu.addSeparator();
		QAction *toggleEdges = menu.addAction("Edge Selection", this,
			[this](bool checked) { setEdgesSelectable(checked); }
		);
		toggleEdges->setCheckable(true);
		toggleEdges->setChecked(m_edgesSelectable);

		menu.exec(m_view->mapToGlobal(pos));
	});

	layout->addWidget(m_view);

	// Refresh initially
	QTimer::singleShot(0, this, &EncodingGraphWindow::refresh);

	obs_frontend_add_event_callback(OBSFrontendEvent, this);
}

EncodingGraphWindow::~EncodingGraphWindow()
{
	obs_frontend_remove_event_callback(OBSFrontendEvent, this);
}

void EncodingGraphWindow::setEdgesSelectable(bool selectable)
{
	m_edgesSelectable = selectable;
	if (!m_scene)
		return;

	for (QGraphicsItem *item : m_scene->items()) {
		if (GraphEdge *edge = dynamic_cast<GraphEdge *>(item)) {
			edge->setFlag(QGraphicsItem::ItemIsSelectable, selectable);
		}
	}
}

void EncodingGraphWindow::OBSFrontendEvent(enum obs_frontend_event event, void *param)
{
	EncodingGraphWindow *dlg = static_cast<EncodingGraphWindow *>(param);
	if (!dlg)
		return;

	switch (event) {
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		// Use QTimer to debounce and ensure thread safety
		QTimer::singleShot(100, dlg, &EncodingGraphWindow::refresh);
		break;
	default:
		break;
	}
}

void EncodingGraphWindow::showEvent(QShowEvent *event)
{
	QMainWindow::showEvent(event);
	refresh();
}

void EncodingGraphWindow::keyPressEvent(QKeyEvent *event)
{
	// Zoom In
	if ((event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) &&
	    (event->modifiers() & Qt::ControlModifier || event->modifiers() == Qt::NoModifier)) {
		zoom(1.1);
		event->accept();
	}
	// Zoom Out
	else if (event->key() == Qt::Key_Minus &&
		 (event->modifiers() & Qt::ControlModifier || event->modifiers() == Qt::NoModifier)) {
		zoom(0.9);
		event->accept();
	}
	// Fit to View (Ctrl + 0)
	else if (event->key() == Qt::Key_0 && (event->modifiers() & Qt::ControlModifier)) {
		m_view->fitInView(m_scene->itemsBoundingRect(), Qt::KeepAspectRatio);
		event->accept();
	} else {
		QMainWindow::keyPressEvent(event);
	}
}

void EncodingGraphWindow::zoom(qreal factor)
{
	m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	m_view->scale(factor, factor);
}

GraphNode *EncodingGraphWindow::getOrCreateNode(const QString &id, const QString &title, NodeType type,
						const QString &subtext)
{
	if (m_nodes.contains(id)) {
		return m_nodes[id];
	}

	GraphNode *node = new GraphNode(title, type, subtext);
	m_scene->addItem(node);
	m_nodes[id] = node;
	return node;
}

void EncodingGraphWindow::addEdge(GraphNode *start, GraphNode *end, const QString &startPort, const QString &endPort)
{
	if (!start || !end)
		return;
	GraphEdge *edge = new GraphEdge(start, end, startPort, endPort);
	if (m_edgesSelectable) {
		edge->setFlag(QGraphicsItem::ItemIsSelectable, true);
	}
	m_scene->addItem(edge);
	start->addEdge(edge);
	end->addEdge(edge);
}

static QString get_encoder_bitrate_string(obs_encoder_t *encoder)
{
	if (!encoder)
		return "";
	obs_data_t *settings = obs_encoder_get_settings(encoder);
	if (!settings)
		return "";

	QString result;
	// Video/Audio encoders usually use 'bitrate' (kbps)
	int bitrate = (int)obs_data_get_int(settings, "bitrate");
	if (bitrate > 0) {
		result = QString("%1 Kbps").arg(bitrate);
	}
	obs_data_release(settings);
	return result;
}

void EncodingGraphWindow::refresh()
{
	// Save current positions
	QMap<QString, QPointF> savedPositions;
	for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
		savedPositions[it.key()] = it.value()->pos();
	}

	m_scene->clear();
	m_nodes.clear();

	// -------------------------------------------------------------
	// Create Global Audio Mixer Node
	// -------------------------------------------------------------
	GraphNode *audioMixerNode =
		getOrCreateNode("MIX:MainAudio", "Audio Mixer", NodeType::AudioMixer, "Global Audio Mixer");
	audioMixerNode->setNodeGroup("MAIN");

	int layoutIndex = 0; // Monotonically increasing index for stable sorting

	// Add 6 Tracks (Input & Output)
	for (int i = 1; i <= MAX_AUDIO_MIXES; i++) {
		audioMixerNode->addInputPort(QString("track%1").arg(i), QString("Track %1").arg(i));
		audioMixerNode->addOutputPort(QString("track%1").arg(i), QString("Track %1").arg(i));
	}

	// -------------------------------------------------------------
	// Enumerate Canvases (Main + Extra) and their Global Sources (channel bound)
	// -------------------------------------------------------------
	QList<obs_canvas_t *> canvases;
	auto canvas_cb = [](void *param, obs_canvas_t *canvas) -> bool {
		auto *list = static_cast<QList<obs_canvas_t *> *>(param);
		list->append(canvas);
		return true;
	};
	obs_enum_canvases(canvas_cb, &canvases);

	obs_canvas_t *mainCanvas = obs_get_main_canvas();

	// Map obs_canvas_t* to GraphNode* for quick lookup
	QHash<obs_canvas_t *, GraphNode *> canvasMap;

	// Track which sources are bound to canvases so we don't filter them out later
	QSet<obs_source_t *> canvasBoundSources;

	for (obs_canvas_t *canvas : canvases) {
		const char *name = obs_canvas_get_name(canvas);
		const char *canvasUUID = obs_canvas_get_uuid(canvas);
		const bool isMain = mainCanvas && canvas == mainCanvas;

		obs_log(LOG_INFO, "X:Canvas: %s (%s)", name, canvasUUID);

		obs_video_info ovi{};
		QString subText;
		if (obs_canvas_get_video_info(canvas, &ovi)) {
			subText = QString("Res: %1x%2\nFPS: %3/%4")
					  .arg(ovi.base_width)
					  .arg(ovi.base_height)
					  .arg(ovi.fps_num)
					  .arg(ovi.fps_den);
		} else {
			subText = "Video Info Unavailable";
		}

		if (isMain) {
			subText.prepend("MAIN OUTPUT\n");
		} else if (canvasUUID) {
			subText.prepend(QString("UUID: %1\n").arg(QString::fromUtf8(canvasUUID).left(8)));
		}

		QString nodeId = isMain ? "SRC:MainCanvas" : QString("CANVAS:%1").arg(canvasUUID);
		QString nodeTitle = isMain ? "Main Canvas" : (name ? name : "Untitled Canvas");

		GraphNode *canvasNode = getOrCreateNode(nodeId, nodeTitle, NodeType::Canvas, subText);
		// Assign group as UUID (or "MAIN" for main canvas)
		canvasNode->setNodeGroup(isMain ? "MAIN" : (canvasUUID ? canvasUUID : ""));
		canvasNode->setSortOrder(layoutIndex++); // Order based on canvas enumeration

		// Add to Map
		canvasMap[canvas] = canvasNode;

		// Setup Ports

		// Video IO Ports
		// Default port for program scene or manually set video source
		canvasNode->addInputPort("video", "Program Input");
		canvasNode->addOutputPort("video_program", "Video Output (Program)");
		if (isMain) {
			canvasNode->addOutputPort("video_preview", "Video Output (Preview)");
		}

		// Audio Ports moved to Audio Mixer Node!

		canvasNode->setCanvas(canvas);

		// Get flags for Audio Mix check
		uint32_t c_flags = obs_canvas_get_flags(canvas);
		const bool canvasMixesAudio = (c_flags & static_cast<uint32_t>(obs_canvas_flags::MIX_AUDIO));

		// Check Canvas Channels (0-63) for bound sources (Global Source)
		for (size_t i = 0; i < MAX_CHANNELS; i++) {
			if (obs_source_t *source = obs_canvas_get_channel(canvas, static_cast<uint32_t>(i))) {
				const char *srcName = obs_source_get_name(source);
				const char *srcId = obs_source_get_id(source);
				const char *srcUuid = obs_source_get_uuid(source);
				obs_source_type srcType = obs_source_get_type(source);

				obs_log(LOG_INFO, "X:Canvas::Channel:Source: %s (%s)[%s]", srcName, srcId, srcUuid);

				// Determine type (Video or Audio)
				const uint32_t flags = obs_source_get_output_flags(source);

				auto nodeType =
					NodeType::VideoInput; // Default to VideoInput if unknown type but has video flag
				if (srcType == OBS_SOURCE_TYPE_SCENE) {
					nodeType = NodeType::Scene;
				} else if (srcType == OBS_SOURCE_TYPE_TRANSITION) {
					nodeType = NodeType::Transition;
				} else if (srcType == OBS_SOURCE_TYPE_INPUT) {
					if ((flags & OBS_SOURCE_VIDEO) && (flags & OBS_SOURCE_AUDIO)) {
						nodeType = NodeType::MediaInput;
					} else if (flags & OBS_SOURCE_VIDEO) {
						nodeType = NodeType::VideoInput;
					} else if (flags & OBS_SOURCE_AUDIO) {
						nodeType = NodeType::AudioInput;
					}
				} else if (srcType ==
					   OBS_SOURCE_TYPE_FILTER) { // NOTE: this unlikely to happen, just in case
					// For filters, we can check the flags to determine if it's video/audio/effect filter
					if (flags & OBS_SOURCE_VIDEO) {
						nodeType = NodeType::VideoInput; // Could be a video filter
					} else if (flags & OBS_SOURCE_AUDIO) {
						nodeType = NodeType::AudioInput; // Could be an audio filter
					}
				}

				GraphNode *srcNode = getOrCreateNode(
					QString("SRC:%1:%2").arg(srcName).arg(srcUuid), srcName, nodeType,
					QString("Type: %1\nChannel: %2").arg(srcId).arg(i + 1));

				// Group with this canvas
				srcNode->setNodeGroup(canvasNode->nodeGroup());
				srcNode->setSortOrder(layoutIndex++); // Order based on channel index
				srcNode->setShowGlobe(true);          // Bound to a channel -> Globe Icon
				srcNode->setSource(source);

				// Add Ports (Video is guaranteed by flag check above, but check audio)
				if (flags & OBS_SOURCE_VIDEO) {
					srcNode->addOutputPort("video", "Video");
				} else {
					// Audio-only source bound to channel gets a Link port
					srcNode->addOutputPort("link", "Link");
				}

				if ((flags & OBS_SOURCE_AUDIO) /*&& obs_source_get_audio_mixers(source) != 0*/) {
					srcNode->addOutputPort("audio", "Audio");
				}

				if (flags & OBS_SOURCE_VIDEO) {
					QString channelPortId = QString("video_channel_%1").arg(i);
					canvasNode->addInputPort(channelPortId, QString("Channel %1").arg(i));
					addEdge(srcNode, canvasNode, "video", channelPortId);
				} else {
					// Audio-only link edge (Gray)
					QString channelPortId = QString("link_channel_%1").arg(i);
					canvasNode->addInputPort(channelPortId, QString("Channel %1").arg(i));
					addEdge(srcNode, canvasNode, "link", channelPortId);
				}

				// Link to Audio Mixer ONLY if Canvas supports MIX_AUDIO
				if ((flags & OBS_SOURCE_AUDIO) && canvasMixesAudio) {
					uint32_t mixers = obs_source_get_audio_mixers(source);
					for (int m = 0; m < MAX_AUDIO_MIXES; m++) {
						if (mixers & (1 << m)) {
							QString trackPort = QString("track%1").arg(m + 1);
							addEdge(srcNode, audioMixerNode, "audio", trackPort);
						}
					}
				}

				canvasBoundSources.insert(source);
			}
		}
	}

	if (mainCanvas) {
		obs_canvas_release(mainCanvas);
	}

	// -------------------------------------------------------------
	// Enum Scenes by Canvases (Deep Inspection)
	// -------------------------------------------------------------
	QSet<obs_source_t *> processedSources;

	struct SceneItemContext {
		EncodingGraphWindow *dlg;
		GraphNode *sceneNode;
		GraphNode *canvasNode;
		GraphNode *audioMixerNode;
		QSet<obs_source_t *> *processed;
		int *layoutIndex;
	};

	auto sceneItemEnum = [](obs_scene_t *, obs_sceneitem_t *item, void *p) -> bool {
		auto *ctx = static_cast<SceneItemContext *>(p);
		obs_source_t *source = obs_sceneitem_get_source(item);
		if (!source)
			return true;

		// Mark as processed so obs_enum_sources doesn't attach it to Main Canvas blindly
		ctx->processed->insert(source);

		const char *name = obs_source_get_name(source);
		const char *uuid = obs_source_get_uuid(source);
		const char *id = obs_source_get_id(source);
		uint32_t flags = obs_source_get_output_flags(source);

		// Determine type and ID prefix
		bool isScene = obs_scene_from_source(source) != nullptr;
		NodeType type;
		if (isScene) {
			type = NodeType::Scene;
		} else if (obs_source_get_type(source) == OBS_SOURCE_TYPE_TRANSITION) {
			type = NodeType::Transition;
		} else if ((flags & OBS_SOURCE_VIDEO) && (flags & OBS_SOURCE_AUDIO)) {
			type = NodeType::MediaInput;
		} else if (flags & OBS_SOURCE_VIDEO) {
			type = NodeType::VideoInput;
		} else {
			type = NodeType::AudioInput;
		}

		QString nodeId;
		QString subText;

		if (isScene) {
			nodeId = QString("SCN:%1:%2").arg(name).arg(uuid);
			subText = "Type: Scene";
		} else {
			nodeId = QString("SRC:%1:%2").arg(name).arg(uuid);
			subText = QString("Type: %1").arg(id);
		}

		GraphNode *srcNode = ctx->dlg->getOrCreateNode(nodeId, name, type, subText);
		srcNode->setSource(source);
		srcNode->setNodeGroup(ctx->sceneNode->nodeGroup()); // Inherit group

		// If sortOrder is 0 (default), assign it. If already assigned (shared source), keep first encountered?
		// Or maybe update it if it's currently appearing in this scene context?
		// Let's assign it if 0.
		if (srcNode->sortOrder() == 0) {
			srcNode->setSortOrder((*ctx->layoutIndex)++);
		}

		if (flags & OBS_SOURCE_VIDEO) {
			srcNode->addOutputPort("video", "Video");
		}
		if (flags & OBS_SOURCE_AUDIO) {
			srcNode->addOutputPort("audio", "Audio");
		}

		// Link Video: Source -> Scene (Composition)
		if (flags & OBS_SOURCE_VIDEO) {
			ctx->dlg->addEdge(srcNode, ctx->sceneNode, "video", "video");
		}

		// Link Audio: Source -> Scene (Pass-through)
		if (flags & OBS_SOURCE_AUDIO) {
			uint32_t mixers = obs_source_get_audio_mixers(source);
			for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
				if (mixers & (1 << i)) {
					// Connect Source Output (Audio) -> Scene Input (Track N)
					QString sceneInputPort = QString("audio_track%1").arg(i + 1);
					ctx->dlg->addEdge(srcNode, ctx->sceneNode, "audio", sceneInputPort);
				}
			}
		}

		return true;
	};

	struct CanvasSceneContext {
		EncodingGraphWindow *dlg;
		GraphNode *canvasNode;
		GraphNode *audioMixerNode;
		decltype(sceneItemEnum) itemCallback;
		QSet<obs_source_t *> *processed;
		int *layoutIndex;
	};

	auto canvasSceneEnum = [](void *p, obs_source_t *sceneSource) -> bool {
		auto *ctx = static_cast<CanvasSceneContext *>(p);

		ctx->processed->insert(sceneSource);

		const char *name = obs_source_get_name(sceneSource);
		const char *uuid = obs_source_get_uuid(sceneSource);

		// Create Scene Node
		GraphNode *sceneNode = ctx->dlg->getOrCreateNode(QString("SCN:%1:%2").arg(name).arg(uuid), name,
								 NodeType::Scene, "Type: Scene");
		sceneNode->setSource(sceneSource);
		sceneNode->setNodeGroup(ctx->canvasNode->nodeGroup());
		sceneNode->setSortOrder((*ctx->layoutIndex)++); // Order based on appearance in canvas

		// Add Ports - Scene handles Video Composition & Audio Pass-through
		sceneNode->addInputPort("video", "Video Input");
		sceneNode->addOutputPort("video", "Video Output");

		// Scene Audio Ports (Pass-through for 6 tracks)
		for (int i = 1; i <= MAX_AUDIO_MIXES; i++) {
			QString trackId = QString("audio_track%1").arg(i);
			sceneNode->addInputPort(trackId, QString("Track %1").arg(i));
			sceneNode->addOutputPort(trackId, QString("Track %1").arg(i));

			// Link Scene -> Global Mixer (Default Flow)
			// Note: This implies the Scene outputs to the main mix.
			// In nested scenes, this might need adjustment, but for top-level scenes, this is correct.
			ctx->dlg->addEdge(sceneNode, ctx->audioMixerNode, trackId, QString("track%1").arg(i));
		}

		// Link Scene -> Canvas (Video)
		ctx->dlg->addEdge(sceneNode, ctx->canvasNode, "video", "video");

		const auto scene = obs_scene_from_source(sceneSource);

		// Enum Items
		SceneItemContext itemCtx = {ctx->dlg,       sceneNode,       ctx->canvasNode, ctx->audioMixerNode,
					    ctx->processed, ctx->layoutIndex};
		obs_scene_enum_items(scene, ctx->itemCallback, &itemCtx);

		return true;
	};

	for (obs_canvas_t *canvas : canvases) {
		if (canvasMap.contains(canvas)) {
			GraphNode *canvasNode = canvasMap[canvas];
			CanvasSceneContext ctx = {this,          canvasNode,        audioMixerNode,
						  sceneItemEnum, &processedSources, &layoutIndex};
			obs_canvas_enum_scenes(canvas, canvasSceneEnum, &ctx);
		}
	}

	// -------------------------------------------------------------
	// Add Inputs (Sources)
	// -------------------------------------------------------------
	struct EnumData {
		EncodingGraphWindow *dialog;
		GraphNode *audioMixerNode;
		QSet<obs_source_t *> *canvasChannelSource;
		QHash<obs_canvas_t *, GraphNode *> *canvasNodes;
		QSet<obs_source_t *> *processed;
	};
	EnumData enumData = {this, audioMixerNode, &canvasBoundSources, &canvasMap, &processedSources};

	obs_enum_sources(
		[](void *data, obs_source_t *source) {
			const auto *ed = static_cast<EnumData *>(data);
			if (ed->processed->contains(source))
				return true; // Already handled in scene hierarchy

			auto *dialog = ed->dialog;
			bool isGlobal = ed->canvasChannelSource->contains(source);

			uint32_t mixers = obs_source_get_audio_mixers(source);
			uint32_t flags = obs_source_get_output_flags(source);

			bool hasAudio = (flags & OBS_SOURCE_AUDIO);
			bool hasVideo = (flags & OBS_SOURCE_VIDEO);

			const char *uuid = obs_source_get_uuid(source);
			const char *name = obs_source_get_name(source);
			const char *id = obs_source_get_id(source);

			// Include if:
			// 1. Has Audio Mixers (active audio source)
			// 2. Is Video Source (potential video source)
			// 3. Is explicitly bound to a canvas (even if no mixers/video flags?)
			if (!hasAudio && !hasVideo && !isGlobal) {
				obs_log(LOG_INFO, "Skipping Source: %s (%s)[%s]", name, id, uuid);
				return true;
			}

			const bool isScene = obs_scene_from_source(source) != nullptr;
			NodeType type;
			if (isScene) {
				type = NodeType::Scene;
			} else if (obs_source_get_type(source) == OBS_SOURCE_TYPE_TRANSITION) {
				type = NodeType::Transition;
			} else if (hasVideo && hasAudio) {
				type = NodeType::MediaInput;
			} else if (hasVideo) {
				type = NodeType::VideoInput;
			} else {
				type = NodeType::AudioInput;
			}
			QString nodeId;
			QString subText;

			if (isScene) {
				nodeId = QString("SCN:%1:%2").arg(name).arg(uuid);
				subText = "Type: Scene";
			} else {
				nodeId = QString("SRC:%1:%2").arg(name).arg(uuid);
				subText = QString("Type: %1").arg(id);
			}

			// Create Source Node
			// Note: If it was already created in the canvas loop, getOrCreateNode returns existing.
			GraphNode *srcNode = dialog->getOrCreateNode(nodeId, name, type, subText);
			srcNode->setSource(source);

			// Add Ports
			if (hasVideo) {
				srcNode->addOutputPort("video", "Video");
			}
			if (hasAudio) {
				srcNode->addOutputPort("audio", "Audio");
			}

			// Determine which Canvas(es) this source belongs to.
			// Try to use obs_source_get_canvas first for accuracy
			// will only work if source has `OBS_SOURCE_REQUIRES_CANVAS`
			GraphNode *targetCanvasNode = nullptr;
			bool ownerMixesAudio = true; // Default to true if not bound (standard global audio)

			if (obs_canvas_t *ownerCanvas = obs_source_get_canvas(source)) {
				// Check MIX_AUDIO flag
				uint32_t cFlags = obs_canvas_get_flags(ownerCanvas);
				if (!(cFlags & (uint32_t)obs_canvas_flags::MIX_AUDIO)) {
					ownerMixesAudio = false;
				}

				if (ed->canvasNodes->contains(ownerCanvas)) {
					targetCanvasNode = ed->canvasNodes->value(ownerCanvas);
					srcNode->setNodeGroup(targetCanvasNode->nodeGroup());
				}
				obs_canvas_release(ownerCanvas);
			} else {
				obs_log(LOG_INFO, "Has No canvas bound:%s (%s)[%s]", name, id, uuid);
			}

			if (!targetCanvasNode && isGlobal) {
				// We already have edges from Step 1, so targetCanvas is just for audio logic?
				// Actually Step 1 added Video edges. We need targetCanvas for Audio edges here.
				// If isBound, nodeGroup is set. Let's find it.
				QString grp = srcNode->nodeGroup();
				if (grp == "MAIN") {
					targetCanvasNode = dialog->getOrCreateNode("SRC:MainCanvas", "Main Canvas",
										   NodeType::Canvas);
				} else {
					if (QString canvasId = QString("CANVAS:%1").arg(grp);
					    dialog->m_nodes.contains(canvasId)) {
						targetCanvasNode = dialog->m_nodes[canvasId];
					}
				}
			}

			// Fallback: If still no target and NOT bound, default to Main Canvas (Global Sources)
			if (!targetCanvasNode && !isGlobal) {
				targetCanvasNode =
					dialog->getOrCreateNode("SRC:MainCanvas", "Main Canvas", NodeType::Canvas);
			}

			if (targetCanvasNode) {
				// Link Video Source -> Canvas Video Input
				// (Only if not already bound via channel)
				if (hasVideo && !isGlobal) {
					dialog->addEdge(srcNode, targetCanvasNode, "video", "video");
				}

				// Link Audio Source -> Audio Mixer
				// Respect Canvas MIX_AUDIO flag if bound
				if (hasAudio && ownerMixesAudio) {
					for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
						if (mixers & (1 << i)) {
							QString trackPort = QString("track%1").arg(i + 1);
							dialog->addEdge(srcNode, ed->audioMixerNode, "audio",
									trackPort);
						}
					}
				}
			}
			return true;
		},
		&enumData);

	// -------------------------------------------------------------
	// Enumerate & Create Encoders
	// -------------------------------------------------------------
	QSet<obs_encoder_t *> videoEncoders;
	QSet<obs_encoder_t *> audioEncoders;
	QHash<obs_encoder_t *, GraphNode *> encoderNodes;

	struct EnumDataEncoders {
		EncodingGraphWindow *self;

		GraphNode *audioMixerNode;

		QSet<obs_encoder_t *> &videoEncoders;
		QSet<obs_encoder_t *> &audioEncoders;

		QHash<obs_canvas_t *, GraphNode *> &canvasNodes;
		QHash<obs_encoder_t *, GraphNode *> &encoderNodes;
	};
	EnumDataEncoders enumEncoderData = {this,          audioMixerNode, videoEncoders,
					    audioEncoders, canvasMap,      encoderNodes};

	obs_enum_encoders(
		[](void *data, obs_encoder_t *encoder) {
			auto *ed = static_cast<EnumDataEncoders *>(data);
			auto *dialog = ed->self;

			const char *name = obs_encoder_get_name(encoder);
			const char *codec = obs_encoder_get_codec(encoder);
			const char *id = obs_encoder_get_id(encoder);

			obs_encoder_type type = obs_encoder_get_type(encoder);

			QString bitrate = get_encoder_bitrate_string(encoder);
			QString subText = QString("Codec: %1").arg(codec);
			if (!bitrate.isEmpty())
				subText += QString("\n%1").arg(bitrate);

			// Create Node
			GraphNode *encNode = dialog->getOrCreateNode(
				QString("ENC:%1:%2").arg(name).arg(reinterpret_cast<qintptr>(encoder)), name,
				NodeType::Encoder, subText);
			encNode->setEncoder(encoder);

			if (type == OBS_ENCODER_VIDEO) {
				ed->videoEncoders.insert(encoder);
				ed->encoderNodes[encoder] = encNode;

				encNode->addInputPort("video", "Video Input");
				encNode->addOutputPort("video", "Video Output");

				// Canvas -> Encoder
				// const auto output_video = obs_encoder_video(encoder);
				if (const auto input_video = obs_encoder_parent_video(encoder)) {
					for (const auto canvas : ed->canvasNodes.keys()) {
						if (input_video == obs_canvas_get_video(canvas)) {
							const auto canvasNode = ed->canvasNodes.value(canvas);
							dialog->addEdge(canvasNode, encNode, "video_program", "video");
						}
					}
				}
			}

			if (type == OBS_ENCODER_AUDIO) {
				ed->audioEncoders.insert(encoder);
				ed->encoderNodes[encoder] = encNode;

				encNode->addInputPort("audio", "Audio Input");
				encNode->addOutputPort("audio", "Audio Output");

				// TODO: ???
			}

			return true;
		},
		&enumEncoderData);

	// -------------------------------------------------------------
	// Enumerate Outputs & Link to Encoders
	// -------------------------------------------------------------
	struct EnumDataOutputs {
		EncodingGraphWindow *self;
		GraphNode *audioMixerNode;
		QSet<obs_source_t *> *canvasChannelSource;
		QHash<obs_canvas_t *, GraphNode *> &canvasNodes;
		QHash<obs_encoder_t *, GraphNode *> &encoderNodes;
		QSet<obs_encoder_t *> &videoEncoders;
		QSet<obs_encoder_t *> &audioEncoders;
	};
	EnumDataOutputs enumOutputData = {this,         audioMixerNode, &canvasBoundSources, canvasMap,
					  encoderNodes, videoEncoders,  audioEncoders};

	obs_enum_outputs(
		[](void *data, obs_output_t *output) {
			const auto *ed = static_cast<EnumDataOutputs *>(data);
			auto *dialog = ed->self;

			const char *name = obs_output_get_name(output);
			const char *id = obs_output_get_id(output);
			const uint32_t flags = obs_output_get_flags(output);
			const bool active = obs_output_active(output);

			QString status = active ? "Active" : "Idle";
			if (obs_output_reconnecting(output))
				status = "Reconnecting";

			GraphNode *outNode = dialog->getOrCreateNode(QString("OUT:%1").arg(name), name,
								     NodeType::Output,
								     QString("Type: %1\nStatus: %2").arg(id, status));
			outNode->setOutput(output);

			const bool multitrack_video = (flags & OBS_OUTPUT_MULTI_TRACK_VIDEO);

			if (flags & OBS_OUTPUT_VIDEO) {
				// Ports
				for (size_t i = 0; i < MAX_OUTPUT_VIDEO_ENCODERS; i++) {
					outNode->addInputPort(QString("video_track%1").arg(i + 1),
							      QString("Video Track %1").arg(i + 1));
					if (!multitrack_video)
						break;
				}

				bool found = false;

				if (flags & OBS_OUTPUT_ENCODED) {
					// Encoder -> Output
					for (size_t enc_idx = 0; enc_idx < MAX_OUTPUT_VIDEO_ENCODERS; enc_idx++) {
						if (obs_encoder_t *video_encoder =
							    obs_output_get_video_encoder2(output, enc_idx)) {
							if (const auto encNode =
								    ed->encoderNodes.value(video_encoder)) {
								QString trackPortIn =
									QString("video_track%1").arg(enc_idx + 1);
								dialog->addEdge(encNode, outNode, "video", trackPortIn);
							}
							found = true;
						}
						if (!multitrack_video)
							break;
					}
				}

				if (!found) {
					if (const auto output_video = obs_output_video(output)) {
						// Canvas -> Output
						for (const auto canvas : ed->canvasNodes.keys()) {
							// assuming it's not passing through an encoder.
							// and gets video directly from canvas/sources.
							if (output_video == obs_canvas_get_video(canvas)) {
								const auto canvasNode = ed->canvasNodes.value(canvas);
								dialog->addEdge(canvasNode, outNode, "video_program",
										"video");
								found = true;
								break;
							}
						}
					}
				}

				// TODO: Source -> Output ??? (OBS NDI Preview/Source Record ?)
				if (!found) {
					obs_log(LOG_WARNING, "VIDEO SOURCE NOT FOUND: %s (%s)", name, id);

					// video_output_connect(output_video);
					// video_output_active(output_video);
					// video_output_get_info(output_video);
				}
			}

			const bool multitrack_audio = (flags & OBS_OUTPUT_MULTI_TRACK_AUDIO);

			if (flags & OBS_OUTPUT_AUDIO) {
				// Ports
				for (size_t i = 0; i < MAX_OUTPUT_AUDIO_ENCODERS; i++) {
					outNode->addInputPort(QString("audio_track%1").arg(i + 1),
							      QString("Audio Track %1").arg(i + 1));
					if (!multitrack_audio)
						break;
				}

				bool found = false;

				if (flags & OBS_OUTPUT_ENCODED) {
					for (size_t enc_idx = 0; enc_idx < MAX_OUTPUT_AUDIO_ENCODERS; enc_idx++) {
						if (obs_encoder_t *audio_encoder = obs_output_get_audio_encoder(
							    output,
							    enc_idx)) { // input index where encoder out is routed to
							if (const auto encNode =
								    ed->encoderNodes.value(audio_encoder)) {
								const auto mixer_index = obs_encoder_get_mixer_index(
									audio_encoder); // encoder mixer input

								obs_log(LOG_INFO,
									"[audio] mixer index: %d, output index: %d",
									mixer_index, enc_idx);

								// Encoder -> Output (encoded)
								QString trackPortIn =
									QString("audio_track%1").arg(enc_idx + 1);
								dialog->addEdge(encNode, outNode, "audio", trackPortIn);

								// Mixer -> Encoder (un-encoded)
								QString trackPortOut =
									QString("track%1").arg(mixer_index + 1);
								dialog->addEdge(ed->audioMixerNode, encNode,
										trackPortOut, "audio");
							}
							found = true;
						}
						if (!multitrack_audio)
							break;
					}
				}

				// // TODO: what are these? (maybe direct output fr mixer?)
				// {
				// 	uint32_t output_mixer = static_cast<uint32_t>(obs_output_get_mixer(output));
				// 	uint32_t output_mixers = static_cast<uint32_t>(obs_output_get_mixers(output));
				//
				// 	obs_log(LOG_INFO, "[audio] %s(%s) output mix, %d|%d", name, id, output_mixer,
				// 		output_mixers);
				// }

				if (!found) {
					// Mixer -> Output (un-encoded)
					const auto output_audio = obs_output_audio(output);
					if (output_audio == obs_get_audio()) {
						for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
							uint32_t output_mixers =
								static_cast<uint32_t>(obs_output_get_mixers(output));
							if (output_mixers & (1 << i)) {
								QString trackPort = QString("track%1").arg(i + 1);
								dialog->addEdge(ed->audioMixerNode, outNode, trackPort,
										"audio");
							}
						}
						found = true;
					}
				}

				// TODO: Source -> Output ???
				if (!found) {
					obs_log(LOG_WARNING, "AUDIO SOURCE NOT FOUND: %s (%s)", name, id);

					// audio_output_connect(output_audio);
					// audio_output_active(output_audio);
					// audio_output_get_info(output_audio);
				}
			}

			return true;
		},
		&enumOutputData);

	// Restore positions or initial layout
	bool anyRestored = false;
	QList<GraphNode *> newNodes;
	for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
		if (savedPositions.contains(it.key())) {
			it.value()->setPos(savedPositions[it.key()]);
			anyRestored = true;
		} else {
			newNodes.append(it.value());
		}
	}

	if (!anyRestored && !savedPositions.isEmpty()) {
		// If we had saved positions but none matched (e.g. completely new scene collection),
		// we probably want a fresh layout.
		layoutGraph();
	} else if (!anyRestored && savedPositions.isEmpty()) {
		// First run / empty state
		layoutGraph();
	} else {
		// Partial restore or full restore.
		// For nodes that were NOT restored (new sources), they default to (0,0).

		if (!newNodes.isEmpty()) {
			qreal startX = 50;
			qreal columnGap = 300;
			qreal paddingY = 20;

			// Determine max Y for each column from EXISTING nodes
			// Columns: 0=Sources, 1=Scenes, 2=CanvasIn, 3=Canvas, 4=Encoder, 5=Output
			QMap<int, qreal> columnBottomY;
			for (int i = 0; i < 6; i++)
				columnBottomY[i] = 50.0;

			auto getColIndex = [](GraphNode *node) -> int {
				if (node->isShowGlobe() || node->nodeType() == NodeType::Transition)
					return 2;
				switch (node->nodeType()) {
				case NodeType::VideoInput:
				case NodeType::AudioInput:
				case NodeType::MediaInput:
					return 0;
				case NodeType::Scene:
					return 1;
				case NodeType::Canvas:
				case NodeType::AudioMixer:
					return 3;
				case NodeType::Encoder:
					return 4;
				case NodeType::Output:
					return 5;
				default:
					return 0;
				}
			};

			// Scan existing nodes to find bottom of columns
			for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
				GraphNode *node = it.value();
				if (newNodes.contains(node))
					continue;

				int col = getColIndex(node);
				qreal bottom = node->y() + node->boundingRect().height() + paddingY;
				if (bottom > columnBottomY[col]) {
					columnBottomY[col] = bottom;
				}
			}

			// Sort new nodes to group them nicely when appending
			auto sorter = [](GraphNode *a, GraphNode *b) {
				if (a->nodeGroup() != b->nodeGroup()) {
					if (a->nodeGroup() == "MAIN")
						return true;
					if (b->nodeGroup() == "MAIN")
						return false;
					return a->nodeGroup() < b->nodeGroup();
				}
				return a->title().compare(b->title(), Qt::CaseInsensitive) < 0;
			};
			std::sort(newNodes.begin(), newNodes.end(), sorter);

			// Place new nodes at the bottom of their respective columns
			for (GraphNode *node : newNodes) {
				int col = getColIndex(node);
				qreal x = startX + (col * columnGap);
				qreal y = columnBottomY[col];

				node->setPos(x, y);
				columnBottomY[col] += node->boundingRect().height() + paddingY;
			}
		}

		// Update edges for restored positions
		for (QGraphicsItem *item : m_scene->items()) {
			if (GraphEdge *edge = dynamic_cast<GraphEdge *>(item)) {
				edge->updatePath();
			}
		}
	}
}

void EncodingGraphWindow::layoutGraph()
{
	// 6-column layout:
	// 1. Sources (Raw Inputs)
	// 2. Scenes (Compositions)
	// 3. Canvas Inputs (Transitions & Global Channel Sources)
	// 4. Canvas (Mixing/Rendering)
	// 5. Encoders
	// 6. Outputs

	QList<GraphNode *> rawSources;
	QList<GraphNode *> scenes;
	QList<GraphNode *> canvasInputs;
	QList<GraphNode *> canvases;
	QList<GraphNode *> encoders;
	QList<GraphNode *> outputs;

	// Classify nodes based on ID prefix and Type
	// We iterate the map to access IDs
	for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
		QString id = it.key();
		GraphNode *node = it.value();

		// Merge Transitions and Global Channel Sources
		if (node->isShowGlobe() || node->nodeType() == NodeType::Transition) {
			canvasInputs.append(node);
			continue;
		}

		switch (node->nodeType()) {
		case NodeType::VideoInput:
		case NodeType::AudioInput:
		case NodeType::MediaInput:
			rawSources.append(node);
			break;
		case NodeType::Scene:
			scenes.append(node);
			break;
		case NodeType::Canvas:
		case NodeType::AudioMixer:
			canvases.append(node);
			break;
		case NodeType::Encoder:
			encoders.append(node);
			break;
		case NodeType::Output:
			outputs.append(node);
			break;
		}
	}

	// Sort nodes by Group first, then Title for stable layout
	auto sorter = [](GraphNode *a, GraphNode *b) {
		// Primary: Sort by assigned order (Topology/Enumeration)
		if (a->sortOrder() != 0 && b->sortOrder() != 0) {
			return a->sortOrder() < b->sortOrder();
		}

		// Secondary: Grouping (for nodes without sort order)
		if (a->nodeGroup() != b->nodeGroup()) {
			// Sort "MAIN" first, then others
			if (a->nodeGroup() == "MAIN")
				return true;
			if (b->nodeGroup() == "MAIN")
				return false;
			return a->nodeGroup() < b->nodeGroup();
		}

		// Fallback: Title
		return a->title().compare(b->title(), Qt::CaseInsensitive) < 0;
	};

	std::sort(rawSources.begin(), rawSources.end(), sorter);
	std::sort(scenes.begin(), scenes.end(), sorter);
	std::sort(canvasInputs.begin(), canvasInputs.end(), sorter);
	std::sort(canvases.begin(), canvases.end(), sorter);
	std::sort(encoders.begin(), encoders.end(), sorter);
	std::sort(outputs.begin(), outputs.end(), sorter);

	qreal startX = 50;
	qreal columnGap = 300; // Increased gap for better readability
	qreal paddingY = 20;

	// Helper to place a column
	auto placeColumn = [&](QList<GraphNode *> &nodes, int colIndex) {
		qreal x = startX + (colIndex * columnGap);
		qreal y = 50;
		QString currentGroup = "";

		for (GraphNode *node : nodes) {
			// Add extra gap between groups (e.g. Main vs Preview)
			if (!currentGroup.isEmpty() && node->nodeGroup() != currentGroup) {
				y += 40;
			}
			currentGroup = node->nodeGroup();

			node->setPos(x, y);
			y += node->boundingRect().height() + paddingY;
		}
	};

	// Place Columns
	placeColumn(rawSources, 0);
	placeColumn(scenes, 1);
	placeColumn(canvasInputs, 2);
	placeColumn(canvases, 3);
	placeColumn(encoders, 4);
	placeColumn(outputs, 5);

	// Update scene rect
	m_scene->setSceneRect(m_scene->itemsBoundingRect().adjusted(-50, -50, 50, 50));

	// Update all edges
	for (QGraphicsItem *item : m_scene->items()) {
		if (GraphEdge *edge = dynamic_cast<GraphEdge *>(item)) {
			edge->updatePath();
		}
	}
}
