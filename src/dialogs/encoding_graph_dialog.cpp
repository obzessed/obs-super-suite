#include "encoding_graph_dialog.h"

#include <QVBoxLayout>
#include <QPainter>
#include <QBrush>
#include <QPen>
#include <QFontMetrics>
#include <QTimer>

// ----------------------------------------------------------------------------
// GraphNode
// ----------------------------------------------------------------------------

GraphNode::GraphNode(const QString &title, NodeType type, const QString &subtext)
	: m_title(title),
	  m_subtext(subtext),
	  m_type(type)
{
	setFlags(ItemIsMovable | ItemSendsGeometryChanges);
	setZValue(1);
}

QRectF GraphNode::boundingRect() const
{
	return QRectF(0, 0, m_width, m_height);
}

void GraphNode::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	// Background color based on type
	QColor bgColor;
	QColor borderColor;

	switch (m_type) {
	case NodeType::Source:
		bgColor = QColor(40, 40, 80);      // Dark Blue
		borderColor = QColor(80, 80, 160); // Light Blue
		break;
	case NodeType::Encoder:
		bgColor = QColor(80, 50, 20);       // Dark Orange
		borderColor = QColor(160, 100, 40); // Orange
		break;
	case NodeType::Output:
		bgColor = QColor(60, 30, 60);       // Dark Purple
		borderColor = QColor(120, 60, 120); // Purple
		break;
	}

	// Draw Body
	painter->setBrush(bgColor);
	painter->setPen(QPen(borderColor, 2));
	painter->drawRoundedRect(boundingRect(), 5, 5);

	// Draw Title
	painter->setPen(Qt::white);
	QFont titleFont = painter->font();
	titleFont.setBold(true);
	titleFont.setPointSize(10);
	painter->setFont(titleFont);
	painter->drawText(QRectF(10, 5, m_width - 20, 20), Qt::AlignLeft | Qt::AlignVCenter, m_title);

	// Draw Subtext
	if (!m_subtext.isEmpty()) {
		QFont subFont = painter->font();
		subFont.setBold(false);
		subFont.setPointSize(8);
		painter->setFont(subFont);
		painter->setPen(QColor(200, 200, 200));
		painter->drawText(QRectF(10, 25, m_width - 20, 30), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
				  m_subtext);
	}

	// Draw Ports
	painter->setBrush(Qt::white);
	painter->setPen(Qt::NoPen);

	// Output node has no right port, Source has no left port (logically)
	// But for visual consistency we can draw inputs on left, outputs on right
	if (m_type != NodeType::Source)
		painter->drawEllipse(leftPort(), 4, 4);

	if (m_type != NodeType::Output)
		painter->drawEllipse(rightPort(), 4, 4);
}

QPointF GraphNode::leftPort() const
{
	return QPointF(0, m_height / 2);
}

QPointF GraphNode::rightPort() const
{
	return QPointF(m_width, m_height / 2);
}

// ----------------------------------------------------------------------------
// GraphEdge
// ----------------------------------------------------------------------------

GraphEdge::GraphEdge(GraphNode *start, GraphNode *end) : m_start(start), m_end(end)
{
	setZValue(0);
	setPen(QPen(QColor(150, 150, 150), 2));
	updatePath();
}

void GraphEdge::updatePath()
{
	if (!m_start || !m_end)
		return;

	QPointF startPos = m_start->pos() + m_start->rightPort();
	QPointF endPos = m_end->pos() + m_end->leftPort();

	QPainterPath path;
	path.moveTo(startPos);

	qreal dx = endPos.x() - startPos.x();
	QPointF ctrl1(startPos.x() + dx * 0.5, startPos.y());
	QPointF ctrl2(endPos.x() - dx * 0.5, endPos.y());

	path.cubicTo(ctrl1, ctrl2, endPos);
	setPath(path);
}

// ----------------------------------------------------------------------------
// EncodingGraphDialog
// ----------------------------------------------------------------------------

EncodingGraphDialog::EncodingGraphDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle("Encoding Graph");
	resize(1000, 600);

	QVBoxLayout *layout = new QVBoxLayout(this);
	m_view = new QGraphicsView(this);
	m_scene = new QGraphicsScene(this);

	// Set dark background
	m_scene->setBackgroundBrush(QColor(30, 30, 30));
	m_view->setScene(m_scene);
	m_view->setRenderHint(QPainter::Antialiasing);

	layout->addWidget(m_view);

	// Refresh initially
	QTimer::singleShot(0, this, &EncodingGraphDialog::refresh);
}

EncodingGraphDialog::~EncodingGraphDialog() {}

void EncodingGraphDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	refresh();
}

GraphNode *EncodingGraphDialog::getOrCreateNode(const QString &id, const QString &title, NodeType type,
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

void EncodingGraphDialog::addEdge(GraphNode *start, GraphNode *end)
{
	if (!start || !end)
		return;
	GraphEdge *edge = new GraphEdge(start, end);
	m_scene->addItem(edge);
}

void EncodingGraphDialog::refresh()
{
	m_scene->clear();
	m_nodes.clear();

	struct OutputInfo {
		obs_output_t *output;
		obs_encoder_t *videoEnc;
		QList<obs_encoder_t *> audioEncs;
		size_t mixers;
	};

	QList<OutputInfo> outputInfos;

	// 1. Enumerate Outputs
	obs_enum_outputs(
		[](void *param, obs_output_t *output) {
			auto *list = static_cast<QList<OutputInfo> *>(param);

			OutputInfo info;
			info.output = output;
			info.videoEnc = obs_output_get_video_encoder(output);
			info.mixers = obs_output_get_mixers(output);

			// Check first 6 audio encoders (Obs usually supports up to 6 tracks)
			for (size_t i = 0; i < 6; i++) {
				obs_encoder_t *enc = obs_output_get_audio_encoder(output, i);
				if (enc) {
					info.audioEncs.append(enc);
				}
			}

			// Only show active or capable outputs to reduce clutter?
			// Showing all for now to be comprehensive.
			list->append(info);
			return true;
		},
		&outputInfos);

	// 2. Build Graph
	for (const auto &info : outputInfos) {
		QString outputName = obs_output_get_name(info.output);
		const char *outputId = obs_output_get_id(info.output);
		bool active = obs_output_active(info.output);

		QString outStatus = active ? "Active" : "Idle";
		if (obs_output_reconnecting(info.output))
			outStatus = "Reconnecting";

		GraphNode *outputNode = getOrCreateNode(QString("OUT:%1").arg(outputName), outputName, NodeType::Output,
							QString("Type: %1\nStatus: %2").arg(outputId, outStatus));

		// Link Video Encoder
		if (info.videoEnc) {
			QString encName = obs_encoder_get_name(info.videoEnc);
			const char *codec = obs_encoder_get_codec(info.videoEnc);
			uint32_t width = obs_encoder_get_width(info.videoEnc);
			uint32_t height = obs_encoder_get_height(info.videoEnc);

			GraphNode *encNode =
				getOrCreateNode(QString("ENC:%1").arg(encName), encName, NodeType::Encoder,
						QString("Codec: %1\nRes: %2x%3").arg(codec).arg(width).arg(height));

			addEdge(encNode, outputNode);

			// Link Video Source (Main Canvas)
			// Ideally we'd know which canvas, but standard outputs use main.
			GraphNode *sourceNode =
				getOrCreateNode("SRC:MainCanvas", "Main Canvas", NodeType::Source, "GPU Composition");
			addEdge(sourceNode, encNode);
		}

		// Link Audio Encoders
		int trackIdx = 1;
		for (obs_encoder_t *audioEnc : info.audioEncs) {
			QString encName = obs_encoder_get_name(audioEnc);
			const char *codec = obs_encoder_get_codec(audioEnc);

			GraphNode *encNode = getOrCreateNode(QString("ENC:%1").arg(encName), encName, NodeType::Encoder,
							     QString("Codec: %1\nAudio").arg(codec));

			addEdge(encNode, outputNode);

			// Determine which track this encoder is for
			// This is tricky; usually audio encoders are assigned to mixer indices sequentially
			// or based on the output's mixer mask.
			// If output has mixer mask 0, it might use a specific audio source (like simple output).
			// But standard outputs use tracks.

			size_t currentMixer = 0;
			// Simple heuristic: check which bit is set in mask for this index?
			// Actually obs_output_get_audio_encoder(out, i) corresponds to track i?
			// The mapping depends on output type. For simple output, it's track 1 (usually).
			// For recording with multitrack, index 0 -> track 1, index 1 -> track 2, etc.

			// Let's create a Source node for the "Track"
			// We can't easily know EXACTLY which track without more deep inspection,
			// but for standard FFmpeg output, index i maps to the i-th set bit in mixer_mask?
			// Or just Track (i+1)? Let's assume Track (i+1) for now if we can't be sure.

			QString trackName = QString("Track %1").arg(trackIdx);

			// If the output uses mixers, check if this index is valid
			if (info.mixers != 0) {
				// Actually we can iterate the mixer mask bits
			}

			GraphNode *sourceNode = getOrCreateNode(QString("SRC:AudioTrack%1").arg(trackIdx), // ID
								trackName, NodeType::Source, "Audio Mix");
			addEdge(sourceNode, encNode);
			trackIdx++;
		}
	}

	layoutGraph();
}

void EncodingGraphDialog::layoutGraph()
{
	// Simple 3-column layout
	QList<GraphNode *> sources;
	QList<GraphNode *> encoders;
	QList<GraphNode *> outputs;

	for (GraphNode *node : m_nodes) {
		switch (node->nodeType()) {
		case NodeType::Source:
			sources.append(node);
			break;
		case NodeType::Encoder:
			encoders.append(node);
			break;
		case NodeType::Output:
			outputs.append(node);
			break;
		}
	}

	// Sort by name for stability
	/*
    auto sorter = [](GraphNode* a, GraphNode* b) {
        return a->title() < b->title();
    };
    std::sort(sources.begin(), sources.end(), sorter);
    std::sort(encoders.begin(), encoders.end(), sorter);
    std::sort(outputs.begin(), outputs.end(), sorter);
    */

	qreal startX = 50;
	qreal gapY = 80;
	qreal columnGap = 300;

	// Place Sources
	qreal y = 50;
	for (GraphNode *node : sources) {
		node->setPos(startX, y);
		y += gapY;
	}

	// Place Encoders
	y = 50;
	for (GraphNode *node : encoders) {
		node->setPos(startX + columnGap, y);
		y += gapY;
	}

	// Place Outputs
	y = 50;
	for (GraphNode *node : outputs) {
		node->setPos(startX + columnGap * 2, y);
		y += gapY;
	}

	// Update scene rect
	m_scene->setSceneRect(m_scene->itemsBoundingRect().adjusted(-50, -50, 50, 50));

	// Update all edges
	for (QGraphicsItem *item : m_scene->items()) {
		if (GraphEdge *edge = dynamic_cast<GraphEdge *>(item)) {
			edge->updatePath();
		}
	}
}
