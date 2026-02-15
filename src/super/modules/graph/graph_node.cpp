// ============================================================================
// Graph Workflow Engine — Implementation
// ============================================================================

#include "graph_node.hpp"

#include <QQueue>
#include <QSet>
#include <algorithm>

namespace super {

// ============================================================================
// GraphNode
// ============================================================================

GraphNode::GraphNode(const QString &type_id, QObject *parent)
	: QObject(parent)
	, m_id(QUuid::createUuid())
	, m_type_id(type_id)
	, m_display_name(type_id)
{
}

QUuid GraphNode::node_id() const { return m_id; }
const QString &GraphNode::type_id() const { return m_type_id; }
const QString &GraphNode::display_name() const { return m_display_name; }
void GraphNode::set_display_name(const QString &name) { m_display_name = name; }

QPointF GraphNode::position() const { return m_position; }
void GraphNode::set_position(const QPointF &pos) { m_position = pos; }

const QList<Pin> &GraphNode::pins() const { return m_pins; }

Pin *GraphNode::find_pin(const QString &pin_id)
{
	for (auto &p : m_pins)
		if (p.id == pin_id)
			return &p;
	return nullptr;
}

const Pin *GraphNode::find_pin(const QString &pin_id) const
{
	for (const auto &p : m_pins)
		if (p.id == pin_id)
			return &p;
	return nullptr;
}

QVariant GraphNode::input_value(const QString &pin_id) const
{
	if (auto *p = find_pin(pin_id))
		return p->current_value.isValid() ? p->current_value : p->default_value;
	return {};
}

void GraphNode::set_output(const QString &pin_id, const QVariant &value)
{
	if (auto *p = find_pin(pin_id)) {
		p->current_value = value;
		emit output_changed(pin_id, value);
	}
}

void GraphNode::add_input(const QString &id, const QString &label,
						   PinType type, const QVariant &default_val)
{
	Pin p;
	p.id = id;
	p.label = label;
	p.direction = PinDirection::Input;
	p.type = type;
	p.default_value = default_val;
	p.current_value = default_val;
	m_pins.append(p);
}

void GraphNode::add_output(const QString &id, const QString &label,
							PinType type)
{
	Pin p;
	p.id = id;
	p.label = label;
	p.direction = PinDirection::Output;
	p.type = type;
	m_pins.append(p);
}

QJsonObject GraphNode::save() const
{
	QJsonObject obj;
	obj["id"] = m_id.toString();
	obj["type"] = m_type_id;
	obj["name"] = m_display_name;
	obj["x"] = m_position.x();
	obj["y"] = m_position.y();

	// Save pin defaults
	QJsonObject pins;
	for (const auto &p : m_pins) {
		if (p.is_input() && p.default_value.isValid())
			pins[p.id] = QJsonValue::fromVariant(p.default_value);
	}
	if (!pins.isEmpty())
		obj["pin_defaults"] = pins;

	return obj;
}

void GraphNode::load(const QJsonObject &obj)
{
	m_id = QUuid::fromString(obj["id"].toString());
	m_display_name = obj["name"].toString(m_type_id);
	m_position = QPointF(obj["x"].toDouble(), obj["y"].toDouble());

	auto pins = obj["pin_defaults"].toObject();
	for (auto it = pins.constBegin(); it != pins.constEnd(); ++it) {
		if (auto *p = find_pin(it.key()))
			p->default_value = it.value().toVariant();
	}
}

// ============================================================================
// GraphEngine
// ============================================================================

GraphEngine::GraphEngine(QObject *parent) : QObject(parent) {}

GraphEngine::~GraphEngine()
{
	qDeleteAll(m_nodes);
}

GraphNode *GraphEngine::add_node(GraphNode *node)
{
	node->setParent(this);
	m_nodes.insert(node->node_id(), node);
	emit node_added(node->node_id());
	return node;
}

void GraphEngine::remove_node(const QUuid &id)
{
	auto *node = m_nodes.take(id);
	if (!node)
		return;

	// Remove all connections involving this node
	m_connections.erase(
		std::remove_if(m_connections.begin(), m_connections.end(),
			[&id](const Connection &c) {
				return c.source_node == id || c.target_node == id;
			}),
		m_connections.end());

	emit node_removed(id);
	node->deleteLater();
}

GraphNode *GraphEngine::find_node(const QUuid &id) const
{
	return m_nodes.value(id, nullptr);
}

QList<GraphNode *> GraphEngine::all_nodes() const
{
	return m_nodes.values();
}

Connection *GraphEngine::connect_pins(const QUuid &source_node,
									   const QString &source_pin,
									   const QUuid &target_node,
									   const QString &target_pin)
{
	// Validate nodes exist
	auto *src = find_node(source_node);
	auto *tgt = find_node(target_node);
	if (!src || !tgt)
		return nullptr;

	// Validate pins exist and directions match
	auto *sp = src->find_pin(source_pin);
	auto *tp = tgt->find_pin(target_pin);
	if (!sp || !tp || !sp->is_output() || !tp->is_input())
		return nullptr;

	Connection c;
	c.id = QUuid::createUuid();
	c.source_node = source_node;
	c.source_pin = source_pin;
	c.target_node = target_node;
	c.target_pin = target_pin;

	m_connections.append(c);
	emit connection_added(c.id);
	return &m_connections.last();
}

void GraphEngine::disconnect(const QUuid &connection_id)
{
	auto it = std::find_if(m_connections.begin(), m_connections.end(),
		[&connection_id](const Connection &c) {
			return c.id == connection_id;
		});

	if (it != m_connections.end()) {
		m_connections.erase(it);
		emit connection_removed(connection_id);
	}
}

QList<Connection> GraphEngine::connections() const
{
	return m_connections;
}

// ---------------------------------------------------------------------------
// Topological sort (Kahn's algorithm)
// ---------------------------------------------------------------------------

QList<GraphNode *> GraphEngine::topological_sort() const
{
	// Build adjacency + in-degree
	QHash<QUuid, int> in_degree;
	QHash<QUuid, QList<QUuid>> adjacency;

	for (auto it = m_nodes.cbegin(); it != m_nodes.cend(); ++it) {
		in_degree[it.key()] = 0;
	}

	for (const auto &conn : m_connections) {
		adjacency[conn.source_node].append(conn.target_node);
		in_degree[conn.target_node]++;
	}

	// Seed with zero in-degree nodes
	QQueue<QUuid> queue;
	for (auto it = in_degree.cbegin(); it != in_degree.cend(); ++it) {
		if (it.value() == 0)
			queue.enqueue(it.key());
	}

	QList<GraphNode *> sorted;
	while (!queue.isEmpty()) {
		QUuid id = queue.dequeue();
		if (auto *node = m_nodes.value(id))
			sorted.append(node);

		for (const QUuid &neighbor : adjacency.value(id)) {
			in_degree[neighbor]--;
			if (in_degree[neighbor] == 0)
				queue.enqueue(neighbor);
		}
	}

	return sorted;
}

// ---------------------------------------------------------------------------
// Propagate output values to connected input pins
// ---------------------------------------------------------------------------

void GraphEngine::propagate_values()
{
	for (const auto &conn : m_connections) {
		auto *src = find_node(conn.source_node);
		auto *tgt = find_node(conn.target_node);
		if (!src || !tgt)
			continue;

		auto *out_pin = src->find_pin(conn.source_pin);
		auto *in_pin = tgt->find_pin(conn.target_pin);
		if (out_pin && in_pin)
			in_pin->current_value = out_pin->current_value;
	}
}

// ---------------------------------------------------------------------------
// Evaluate: topological sort → propagate → process each node
// ---------------------------------------------------------------------------

void GraphEngine::evaluate()
{
	auto sorted = topological_sort();

	for (auto *node : sorted) {
		propagate_values();  // Push upstream outputs to this node's inputs
		node->process();
	}

	emit evaluation_complete();
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

QJsonObject GraphEngine::save() const
{
	QJsonObject obj;

	QJsonArray nodes_arr;
	for (auto it = m_nodes.cbegin(); it != m_nodes.cend(); ++it)
		nodes_arr.append(it.value()->save());
	obj["nodes"] = nodes_arr;

	QJsonArray conn_arr;
	for (const auto &c : m_connections)
		conn_arr.append(c.to_json());
	obj["connections"] = conn_arr;

	return obj;
}

void GraphEngine::load(const QJsonObject &obj)
{
	// Note: Node deserialization requires a factory to create the right
	// subclass by type_id. This will be implemented when the node library
	// is built. For now, connections are loaded here.

	auto conn_arr = obj["connections"].toArray();
	m_connections.clear();
	for (const auto &v : conn_arr)
		m_connections.append(Connection::from_json(v.toObject()));
}

} // namespace super
