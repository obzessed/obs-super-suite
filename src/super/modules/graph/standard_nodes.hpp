#pragma once

// ============================================================================
// Standard Node Library — Built-in graph nodes.
// Math, Logic, Flow, and ControlPort bridge nodes.
// ============================================================================

#include "graph_node.hpp"
#include "../../core/control_registry.hpp"

#include <QtMath>

namespace super {

// ---------------------------------------------------------------------------
// MathNode — Performs a binary math operation.
// Inputs:  A (Number), B (Number)
// Output:  Result (Number)
// ---------------------------------------------------------------------------
class MathNode : public GraphNode {
	Q_OBJECT
public:
	enum Op { Add, Subtract, Multiply, Divide, Power, Modulo, Min, Max };

	explicit MathNode(Op op = Add, QObject *parent = nullptr)
		: GraphNode("math", parent), m_op(op)
	{
		add_input("a", "A", PinType::Number, 0.0);
		add_input("b", "B", PinType::Number, 0.0);
		add_output("result", "Result", PinType::Number);
		set_display_name(op_name(op));
	}

	void process() override {
		double a = input_value("a").toDouble();
		double b = input_value("b").toDouble();
		double r = 0.0;
		switch (m_op) {
		case Add:      r = a + b; break;
		case Subtract: r = a - b; break;
		case Multiply: r = a * b; break;
		case Divide:   r = qFuzzyIsNull(b) ? 0.0 : a / b; break;
		case Power:    r = qPow(a, b); break;
		case Modulo:   r = qFuzzyIsNull(b) ? 0.0 : std::fmod(a, b); break;
		case Min:      r = qMin(a, b); break;
		case Max:      r = qMax(a, b); break;
		}
		set_output("result", r);
	}

private:
	static QString op_name(Op op) {
		switch (op) {
		case Add:      return "Add";
		case Subtract: return "Subtract";
		case Multiply: return "Multiply";
		case Divide:   return "Divide";
		case Power:    return "Power";
		case Modulo:   return "Modulo";
		case Min:      return "Min";
		case Max:      return "Max";
		}
		return "Math";
	}
	Op m_op;
};

// ---------------------------------------------------------------------------
// CompareNode — Boolean comparison.
// ---------------------------------------------------------------------------
class CompareNode : public GraphNode {
	Q_OBJECT
public:
	enum Op { Equal, NotEqual, Less, Greater, LessEqual, GreaterEqual };

	explicit CompareNode(Op op = Equal, QObject *parent = nullptr)
		: GraphNode("compare", parent), m_op(op)
	{
		add_input("a", "A", PinType::Number, 0.0);
		add_input("b", "B", PinType::Number, 0.0);
		add_output("result", "Result", PinType::Bool);
	}

	void process() override {
		double a = input_value("a").toDouble();
		double b = input_value("b").toDouble();
		bool r = false;
		switch (m_op) {
		case Equal:        r = qFuzzyCompare(a, b); break;
		case NotEqual:     r = !qFuzzyCompare(a, b); break;
		case Less:         r = a < b; break;
		case Greater:      r = a > b; break;
		case LessEqual:    r = a <= b; break;
		case GreaterEqual: r = a >= b; break;
		}
		set_output("result", r);
	}

private:
	Op m_op;
};

// ---------------------------------------------------------------------------
// LogicGateNode — AND, OR, NOT, XOR.
// ---------------------------------------------------------------------------
class LogicGateNode : public GraphNode {
	Q_OBJECT
public:
	enum Op { And, Or, Not, Xor };

	explicit LogicGateNode(Op op = And, QObject *parent = nullptr)
		: GraphNode("logic_gate", parent), m_op(op)
	{
		add_input("a", "A", PinType::Bool, false);
		if (op != Not)
			add_input("b", "B", PinType::Bool, false);
		add_output("result", "Result", PinType::Bool);
	}

	void process() override {
		bool a = input_value("a").toBool();
		bool b = input_value("b").toBool();
		bool r = false;
		switch (m_op) {
		case And: r = a && b; break;
		case Or:  r = a || b; break;
		case Not: r = !a; break;
		case Xor: r = a != b; break;
		}
		set_output("result", r);
	}

private:
	Op m_op;
};

// ---------------------------------------------------------------------------
// SwitchNode — Routes input to output based on condition.
// ---------------------------------------------------------------------------
class SwitchNode : public GraphNode {
	Q_OBJECT
public:
	explicit SwitchNode(QObject *parent = nullptr)
		: GraphNode("switch", parent)
	{
		add_input("condition", "Condition", PinType::Bool, false);
		add_input("true_val", "If True", PinType::Number, 1.0);
		add_input("false_val", "If False", PinType::Number, 0.0);
		add_output("result", "Result", PinType::Number);
		set_display_name("Switch");
	}

	void process() override {
		bool cond = input_value("condition").toBool();
		set_output("result", cond ? input_value("true_val")
								  : input_value("false_val"));
	}
};

// ---------------------------------------------------------------------------
// ClampNode — Restricts a value to [min, max].
// ---------------------------------------------------------------------------
class ClampNode : public GraphNode {
	Q_OBJECT
public:
	explicit ClampNode(QObject *parent = nullptr)
		: GraphNode("clamp", parent)
	{
		add_input("value", "Value", PinType::Number,  0.0);
		add_input("min",   "Min",   PinType::Number,  0.0);
		add_input("max",   "Max",   PinType::Number,  1.0);
		add_output("result", "Result", PinType::Number);
		set_display_name("Clamp");
	}

	void process() override {
		double v = input_value("value").toDouble();
		double lo = input_value("min").toDouble();
		double hi = input_value("max").toDouble();
		set_output("result", qBound(lo, v, hi));
	}
};

// ---------------------------------------------------------------------------
// MapRangeNode — Remaps a value from one range to another.
// ---------------------------------------------------------------------------
class MapRangeNode : public GraphNode {
	Q_OBJECT
public:
	explicit MapRangeNode(QObject *parent = nullptr)
		: GraphNode("map_range", parent)
	{
		add_input("value",    "Value",    PinType::Number, 0.0);
		add_input("in_min",   "In Min",   PinType::Number, 0.0);
		add_input("in_max",   "In Max",   PinType::Number, 1.0);
		add_input("out_min",  "Out Min",  PinType::Number, 0.0);
		add_input("out_max",  "Out Max",  PinType::Number, 100.0);
		add_output("result",  "Result",   PinType::Number);
		set_display_name("Map Range");
	}

	void process() override {
		double v      = input_value("value").toDouble();
		double in_lo  = input_value("in_min").toDouble();
		double in_hi  = input_value("in_max").toDouble();
		double out_lo = input_value("out_min").toDouble();
		double out_hi = input_value("out_max").toDouble();

		double span = in_hi - in_lo;
		if (qFuzzyIsNull(span)) {
			set_output("result", out_lo);
			return;
		}
		double t = (v - in_lo) / span;
		set_output("result", out_lo + t * (out_hi - out_lo));
	}
};

// ---------------------------------------------------------------------------
// PortReadNode — Reads a ControlPort value from the Registry.
// ---------------------------------------------------------------------------
class PortReadNode : public GraphNode {
	Q_OBJECT
public:
	explicit PortReadNode(const QString &port_id = {},
						   QObject *parent = nullptr)
		: GraphNode("port_read", parent), m_port_id(port_id)
	{
		add_output("value", "Value", PinType::Number);
		set_display_name("Read: " + port_id);
	}

	void set_port_id(const QString &id) { m_port_id = id; }

	void process() override {
		auto *port = ControlRegistry::instance().find(m_port_id);
		set_output("value", port ? port->as_double() : 0.0);
	}

private:
	QString m_port_id;
};

// ---------------------------------------------------------------------------
// PortWriteNode — Writes a value to a ControlPort in the Registry.
// ---------------------------------------------------------------------------
class PortWriteNode : public GraphNode {
	Q_OBJECT
public:
	explicit PortWriteNode(const QString &port_id = {},
							QObject *parent = nullptr)
		: GraphNode("port_write", parent), m_port_id(port_id)
	{
		add_input("value", "Value", PinType::Number, 0.0);
		set_display_name("Write: " + port_id);
	}

	void set_port_id(const QString &id) { m_port_id = id; }

	void process() override {
		auto *port = ControlRegistry::instance().find(m_port_id);
		if (port)
			port->set_value(input_value("value"));
	}

private:
	QString m_port_id;
};

// ---------------------------------------------------------------------------
// ConstantNode — Outputs a constant value.
// ---------------------------------------------------------------------------
class ConstantNode : public GraphNode {
	Q_OBJECT
public:
	explicit ConstantNode(double value = 0.0, QObject *parent = nullptr)
		: GraphNode("constant", parent)
	{
		add_output("value", "Value", PinType::Number);
		set_display_name("Constant");
		set_output("value", value);
	}

	void process() override {
		// Output is already set; no-op unless overridden.
	}
};

// ---------------------------------------------------------------------------
// SmoothNode — Exponential moving average.
// ---------------------------------------------------------------------------
class SmoothNode : public GraphNode {
	Q_OBJECT
public:
	explicit SmoothNode(QObject *parent = nullptr)
		: GraphNode("smooth", parent)
	{
		add_input("input", "Input", PinType::Number, 0.0);
		add_input("alpha", "Smoothing", PinType::Number, 0.8);
		add_output("output", "Output", PinType::Number);
		set_display_name("Smooth");
	}

	void process() override {
		double in = input_value("input").toDouble();
		double a = qBound(0.0, input_value("alpha").toDouble(), 1.0);
		m_prev = a * m_prev + (1.0 - a) * in;
		set_output("output", m_prev);
	}

private:
	double m_prev = 0.0;
};

} // namespace super
