#pragma once

// ============================================================================
// Graph Workflow Engine — Node System
// The visual programming backbone: Nodes, Pins, and Connections.
// ============================================================================

#include <QObject>
#include <QString>
#include <QVariant>
#include <QList>
#include <QHash>
#include <QUuid>
#include <QPointF>
#include <QJsonObject>
#include <QJsonArray>

namespace super {

class GraphNode;
class GraphEngine;

// ---------------------------------------------------------------------------
// PinDirection — Whether a pin receives or sends data.
// ---------------------------------------------------------------------------
enum class PinDirection { Input, Output };

// ---------------------------------------------------------------------------
// PinType — The data type flowing through a pin.
// ---------------------------------------------------------------------------
enum class PinType {
	Any,		// Accepts anything (auto-convert)
	Number,		// double
	Bool,		// boolean
	String,		// QString
	Event		// Stateless trigger (like ControlType::Command)
};

// ---------------------------------------------------------------------------
// Pin — A single input or output slot on a node.
// ---------------------------------------------------------------------------
struct Pin {
	QString id;				// Unique within node: "in_a", "out_result"
	QString label;			// Display name: "Input A"
	PinDirection direction;
	PinType type = PinType::Any;
	QVariant default_value;
	QVariant current_value;

	bool is_input() const { return direction == PinDirection::Input; }
	bool is_output() const { return direction == PinDirection::Output; }
};

// ---------------------------------------------------------------------------
// Connection — A wire between two pins on different nodes.
// ---------------------------------------------------------------------------
struct Connection {
	QUuid id;
	QUuid source_node;
	QString source_pin;
	QUuid target_node;
	QString target_pin;

	QJsonObject to_json() const {
		QJsonObject obj;
		obj["id"] = id.toString();
		obj["source_node"] = source_node.toString();
		obj["source_pin"] = source_pin;
		obj["target_node"] = target_node.toString();
		obj["target_pin"] = target_pin;
		return obj;
	}

	static Connection from_json(const QJsonObject &obj) {
		Connection c;
		c.id = QUuid::fromString(obj["id"].toString());
		c.source_node = QUuid::fromString(obj["source_node"].toString());
		c.source_pin = obj["source_pin"].toString();
		c.target_node = QUuid::fromString(obj["target_node"].toString());
		c.target_pin = obj["target_pin"].toString();
		return c;
	}
};

// ---------------------------------------------------------------------------
// GraphNode — Abstract base class for all processing nodes.
//
// Subclasses implement process() to read inputs and write outputs.
// ---------------------------------------------------------------------------
class GraphNode : public QObject {
	Q_OBJECT

public:
	explicit GraphNode(const QString &type_id, QObject *parent = nullptr);
	~GraphNode() override = default;

	// -- Identity --
	QUuid node_id() const;
	const QString &type_id() const;
	const QString &display_name() const;
	void set_display_name(const QString &name);

	// -- Position (for visual editor) --
	QPointF position() const;
	void set_position(const QPointF &pos);

	// -- Pins --
	const QList<Pin> &pins() const;
	Pin *find_pin(const QString &pin_id);
	const Pin *find_pin(const QString &pin_id) const;

	QVariant input_value(const QString &pin_id) const;
	void set_output(const QString &pin_id, const QVariant &value);

	// -- Processing --
	// Called by the engine each tick. Read inputs, compute, write outputs.
	virtual void process() = 0;

	// -- Serialization --
	virtual QJsonObject save() const;
	virtual void load(const QJsonObject &obj);

signals:
	void output_changed(const QString &pin_id, const QVariant &value);

protected:
	// Subclasses call these in constructor to define their interface.
	void add_input(const QString &id, const QString &label,
				   PinType type = PinType::Any,
				   const QVariant &default_val = {});
	void add_output(const QString &id, const QString &label,
					PinType type = PinType::Any);

private:
	QUuid m_id;
	QString m_type_id;
	QString m_display_name;
	QPointF m_position;
	QList<Pin> m_pins;
};

// ---------------------------------------------------------------------------
// GraphEngine — Owns nodes, manages connections, drives evaluation.
// ---------------------------------------------------------------------------
class GraphEngine : public QObject {
	Q_OBJECT

public:
	explicit GraphEngine(QObject *parent = nullptr);
	~GraphEngine() override;

	// -- Node Management --
	GraphNode *add_node(GraphNode *node);    // Takes ownership
	void remove_node(const QUuid &id);
	GraphNode *find_node(const QUuid &id) const;
	QList<GraphNode *> all_nodes() const;

	// -- Connection Management --
	Connection *connect_pins(const QUuid &source_node,
							  const QString &source_pin,
							  const QUuid &target_node,
							  const QString &target_pin);
	void disconnect(const QUuid &connection_id);
	QList<Connection> connections() const;

	// -- Evaluation --
	// Process all nodes in topological order.
	void evaluate();

	// -- Serialization --
	QJsonObject save() const;
	void load(const QJsonObject &obj);

signals:
	void node_added(const QUuid &id);
	void node_removed(const QUuid &id);
	void connection_added(const QUuid &id);
	void connection_removed(const QUuid &id);
	void evaluation_complete();

private:
	void propagate_values();
	QList<GraphNode *> topological_sort() const;

	QHash<QUuid, GraphNode *> m_nodes;
	QList<Connection> m_connections;
};

} // namespace super
