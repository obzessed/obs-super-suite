#pragma once

#include <QDialog>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <obs.h>

enum class NodeType { Source, Encoder, Output };

class GraphNode : public QGraphicsItem {
public:
	GraphNode(const QString &title, NodeType type, const QString &subtext = "");

	QRectF boundingRect() const override;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

	NodeType nodeType() const { return m_type; }
	void setPos(qreal x, qreal y) { QGraphicsItem::setPos(x, y); }

	QPointF leftPort() const;
	QPointF rightPort() const;

private:
	QString m_title;
	QString m_subtext;
	NodeType m_type;
	qreal m_width = 180;
	qreal m_height = 60;
};

class GraphEdge : public QGraphicsPathItem {
public:
	GraphEdge(GraphNode *start, GraphNode *end);
	void updatePath();

private:
	GraphNode *m_start;
	GraphNode *m_end;
};

class EncodingGraphDialog : public QDialog {
	Q_OBJECT

public:
	explicit EncodingGraphDialog(QWidget *parent = nullptr);
	~EncodingGraphDialog();

public slots:
	void refresh();

protected:
	void showEvent(QShowEvent *event) override;

private:
	QGraphicsView *m_view;
	QGraphicsScene *m_scene;

	// Helper to deduplicate nodes
	QMap<QString, GraphNode *> m_nodes;

	GraphNode *getOrCreateNode(const QString &id, const QString &title, NodeType type, const QString &subtext = "");
	void addEdge(GraphNode *start, GraphNode *end);
	void layoutGraph();
};
