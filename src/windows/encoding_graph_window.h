#pragma once

#include <obs.h>

#include <QMainWindow>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>

enum class NodeType {
	AudioMixer,
	VideoInput,
	AudioInput,
	MediaInput,
	Scene,
	Transition,
	Canvas,
	Encoder,
	Output,
};

class GraphEdge;

class GraphNode : public QGraphicsItem {
public:
	GraphNode(const QString &title, NodeType type, const QString &subtext = "");
	~GraphNode() override;

	QRectF boundingRect() const override;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

	NodeType nodeType() const { return m_type; }
	QString title() const { return m_title; }

	void setSource(obs_source_t *source);
	void setEncoder(obs_encoder_t *encoder);
	void setOutput(obs_output_t *output);
	void setCanvas(obs_canvas_t *canvas);

	// These return a new strong reference that MUST be released by the caller!
	obs_source_t *getSourceRef() const;
	obs_encoder_t *getEncoderRef() const;
	obs_output_t *getOutputRef() const;
	obs_canvas_t *getCanvasRef() const; // Helper, adds ref

	void setNodeGroup(const QString &group) { m_group = group; }
	QString nodeGroup() const { return m_group; }

	void setShowGlobe(const bool show) { m_showGlobe = show; }
	bool isShowGlobe() const { return m_showGlobe; }

	void setSortOrder(int order) { m_sortOrder = order; }
	int sortOrder() const { return m_sortOrder; }

	// Port support
	void addInputPort(const QString &id, const QString &label);
	void addOutputPort(const QString &id, const QString &label);

	QPointF getInputPortPosition(const QString &id) const;
	QPointF getOutputPortPosition(const QString &id) const;

	[[nodiscard]] QPointF leftPort() const;
	[[nodiscard]] QPointF rightPort() const;

	void addEdge(GraphEdge *edge);

	void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

protected:
	QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
	void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
	struct NodePort {
		QString id;
		QString label;
		qreal yOffset;
	};

	QString m_title;
	QString m_subtext;
	QString m_group; // For layout grouping (e.g. Canvas UUID)
	NodeType m_type;
	qreal m_width = 200;
	qreal m_height = 80;
	qintptr m_sortOrder = 0;
	bool m_showGlobe = false;

	obs_weak_source_t *m_weakSource = nullptr;
	obs_weak_encoder_t *m_weakEncoder = nullptr;
	obs_weak_output_t *m_weakOutput = nullptr;
	obs_canvas_t *m_canvas = nullptr;

	QList<GraphEdge *> m_edges;

	QList<NodePort> m_inputPorts;
	QList<NodePort> m_outputPorts;

	QColor getHeaderColor() const;
	QColor getBodyColor() const;
	QColor getBorderColor() const;
};

class GraphEdge : public QGraphicsPathItem {
public:
	GraphEdge(GraphNode *start, GraphNode *end, const QString &startPortId = "", const QString &endPortId = "");
	void updatePath();

	GraphNode *startNode() const { return m_start; }
	GraphNode *endNode() const { return m_end; }
	QString startPortId() const { return m_startPortId; }
	QString endPortId() const { return m_endPortId; }

	QRectF boundingRect() const override;

protected:
	QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
	QPainterPath shape() const override;

private:
	GraphNode *m_start;
	GraphNode *m_end;
	QString m_startPortId;
	QString m_endPortId;
	QColor m_baseColor;
};

class GraphScene : public QGraphicsScene {
	Q_OBJECT

public:
	explicit GraphScene(QObject *parent = nullptr);
	~GraphScene() override;

private:
};

class EncodingGraphWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit EncodingGraphWindow(QWidget *parent = nullptr);
	~EncodingGraphWindow() override;

public slots:
	void refresh();

protected:
	void showEvent(QShowEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;

private:
	QGraphicsView *m_view;
	GraphScene *m_scene;

	void zoom(qreal factor);

	// Helper to deduplicate nodes
	QMap<QString, GraphNode *> m_nodes;

	GraphNode *getOrCreateNode(const QString &id, const QString &title, NodeType type, const QString &subtext = "");
	void addEdge(GraphNode *start, GraphNode *end, const QString &startPort = "", const QString &endPort = "");
	void layoutGraph();
	void setEdgesSelectable(bool selectable);

	bool m_edgesSelectable = false;

	static void OBSFrontendEvent(enum obs_frontend_event event, void *param);
};
