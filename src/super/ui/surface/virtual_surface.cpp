// ============================================================================
// Virtual Surface — Implementation
// ============================================================================

#include "virtual_surface.hpp"
#include "../../core/control_registry.hpp"
#include "../../core/control_types.hpp"

namespace super {

// ---------------------------------------------------------------------------
// SurfaceElement parsing
// ---------------------------------------------------------------------------

static SurfaceWidgetType parse_widget_type(const QString &s)
{
	if (s == "fader")    return SurfaceWidgetType::Fader;
	if (s == "hfader")   return SurfaceWidgetType::HFader;
	if (s == "knob")     return SurfaceWidgetType::Knob;
	if (s == "button")   return SurfaceWidgetType::Button;
	if (s == "toggle")   return SurfaceWidgetType::Toggle;
	if (s == "label")    return SurfaceWidgetType::Label;
	if (s == "encoder")  return SurfaceWidgetType::Encoder;
	if (s == "xypad")    return SurfaceWidgetType::XYPad;
	if (s == "group")    return SurfaceWidgetType::Group;
	return SurfaceWidgetType::Fader;
}

SurfaceElement SurfaceElement::from_json(const QJsonObject &obj)
{
	SurfaceElement el;
	el.type = parse_widget_type(obj["type"].toString("fader"));
	el.id = obj["id"].toString();
	el.label = obj["label"].toString(el.id);
	el.port_binding = obj["port"].toString();
	el.row = obj["row"].toInt(0);
	el.col = obj["col"].toInt(0);
	el.min_val = obj["min"].toDouble(0.0);
	el.max_val = obj["max"].toDouble(1.0);
	el.default_val = obj["default"].toDouble(0.0);
	el.checkable = obj["checkable"].toBool(false);

	auto children = obj["children"].toArray();
	for (const auto &v : children)
		el.children.append(SurfaceElement::from_json(v.toObject()));

	return el;
}

SurfaceSchema SurfaceSchema::from_json(const QJsonObject &obj)
{
	SurfaceSchema schema;
	schema.name = obj["name"].toString("Untitled Surface");
	schema.columns = obj["columns"].toInt(4);

	auto elements = obj["elements"].toArray();
	for (const auto &v : elements)
		schema.elements.append(SurfaceElement::from_json(v.toObject()));

	return schema;
}

// ---------------------------------------------------------------------------
// VirtualSurface
// ---------------------------------------------------------------------------

VirtualSurface::VirtualSurface(QWidget *parent)
	: QWidget(parent)
{
	m_grid = new QGridLayout(this);
	m_grid->setSpacing(4);
}

void VirtualSurface::load_schema(const QJsonObject &schema_json)
{
	load_schema(SurfaceSchema::from_json(schema_json));
}

void VirtualSurface::load_schema(const SurfaceSchema &schema)
{
	clear();
	m_schema_name = schema.name;

	for (const auto &el : schema.elements) {
		auto *widget = create_widget(el);
		if (widget) {
			m_grid->addWidget(widget, el.row, el.col);
			m_widgets.append(widget);
		}
	}
}

void VirtualSurface::clear()
{
	qDeleteAll(m_widgets);
	m_widgets.clear();
	m_schema_name.clear();
}

QString VirtualSurface::schema_name() const
{
	return m_schema_name;
}

// ---------------------------------------------------------------------------
// Widget Factory
// ---------------------------------------------------------------------------

QWidget *VirtualSurface::create_widget(const SurfaceElement &element)
{
	switch (element.type) {

	case SurfaceWidgetType::Fader: {
		auto *container = new QWidget(this);
		auto *layout = new QVBoxLayout(container);
		layout->setContentsMargins(2, 2, 2, 2);

		auto *label = new QLabel(element.label, container);
		label->setAlignment(Qt::AlignCenter);
		layout->addWidget(label);

		auto *slider = new QSlider(Qt::Vertical, container);
		slider->setMinimum(static_cast<int>(element.min_val * 100));
		slider->setMaximum(static_cast<int>(element.max_val * 100));
		slider->setValue(static_cast<int>(element.default_val * 100));
		slider->setObjectName(element.id);
		layout->addWidget(slider, 1);

		if (!element.port_binding.isEmpty())
			bind_to_port(slider, element.port_binding);

		connect(slider, &QSlider::valueChanged, this,
			[this, id = element.id](int v) {
				emit control_changed(id, v / 100.0);
			});

		return container;
	}

	case SurfaceWidgetType::HFader: {
		auto *container = new QWidget(this);
		auto *layout = new QHBoxLayout(container);
		layout->setContentsMargins(2, 2, 2, 2);

		auto *label = new QLabel(element.label, container);
		layout->addWidget(label);

		auto *slider = new QSlider(Qt::Horizontal, container);
		slider->setMinimum(static_cast<int>(element.min_val * 100));
		slider->setMaximum(static_cast<int>(element.max_val * 100));
		slider->setValue(static_cast<int>(element.default_val * 100));
		slider->setObjectName(element.id);
		layout->addWidget(slider, 1);

		if (!element.port_binding.isEmpty())
			bind_to_port(slider, element.port_binding);

		connect(slider, &QSlider::valueChanged, this,
			[this, id = element.id](int v) {
				emit control_changed(id, v / 100.0);
			});

		return container;
	}

	case SurfaceWidgetType::Knob: {
		auto *container = new QWidget(this);
		auto *layout = new QVBoxLayout(container);
		layout->setContentsMargins(2, 2, 2, 2);

		auto *label = new QLabel(element.label, container);
		label->setAlignment(Qt::AlignCenter);
		layout->addWidget(label);

		auto *dial = new QDial(container);
		dial->setMinimum(static_cast<int>(element.min_val * 100));
		dial->setMaximum(static_cast<int>(element.max_val * 100));
		dial->setValue(static_cast<int>(element.default_val * 100));
		dial->setObjectName(element.id);
		dial->setFixedSize(48, 48);
		layout->addWidget(dial, 0, Qt::AlignCenter);

		if (!element.port_binding.isEmpty())
			bind_to_port(dial, element.port_binding);

		connect(dial, &QDial::valueChanged, this,
			[this, id = element.id](int v) {
				emit control_changed(id, v / 100.0);
			});

		return container;
	}

	case SurfaceWidgetType::Button: {
		auto *btn = new QPushButton(element.label, this);
		btn->setObjectName(element.id);
		btn->setCheckable(element.checkable);

		if (!element.port_binding.isEmpty())
			bind_to_port(btn, element.port_binding);

		connect(btn, &QPushButton::clicked, this,
			[this, id = element.id](bool checked) {
				emit control_changed(id, checked ? 1.0 : 0.0);
			});

		return btn;
	}

	case SurfaceWidgetType::Toggle: {
		auto *check = new QCheckBox(element.label, this);
		check->setObjectName(element.id);

		if (!element.port_binding.isEmpty())
			bind_to_port(check, element.port_binding);

		connect(check, &QCheckBox::toggled, this,
			[this, id = element.id](bool on) {
				emit control_changed(id, on ? 1.0 : 0.0);
			});

		return check;
	}

	case SurfaceWidgetType::Label: {
		auto *label = new QLabel(element.label, this);
		label->setObjectName(element.id);
		label->setAlignment(Qt::AlignCenter);
		return label;
	}

	case SurfaceWidgetType::Encoder: {
		// Encoder rendered as a QDial with wrapping enabled
		auto *container = new QWidget(this);
		auto *layout = new QVBoxLayout(container);
		layout->setContentsMargins(2, 2, 2, 2);

		auto *label = new QLabel(element.label, container);
		label->setAlignment(Qt::AlignCenter);
		layout->addWidget(label);

		auto *dial = new QDial(container);
		dial->setMinimum(0);
		dial->setMaximum(127);
		dial->setValue(64);
		dial->setWrapping(true);
		dial->setObjectName(element.id);
		dial->setFixedSize(48, 48);
		layout->addWidget(dial, 0, Qt::AlignCenter);

		if (!element.port_binding.isEmpty())
			bind_to_port(dial, element.port_binding);

		return container;
	}

	case SurfaceWidgetType::Group: {
		auto *group = new QGroupBox(element.label, this);
		auto *layout = new QGridLayout(group);
		layout->setSpacing(4);

		for (const auto &child : element.children) {
			auto *w = create_widget(child);
			if (w)
				layout->addWidget(w, child.row, child.col);
		}

		return group;
	}

	case SurfaceWidgetType::XYPad: {
		// TODO: Custom XYPad widget. Placeholder label for now.
		auto *label = new QLabel(element.label + "\n(XY Pad)", this);
		label->setObjectName(element.id);
		label->setAlignment(Qt::AlignCenter);
		label->setMinimumSize(100, 100);
		label->setStyleSheet(
			"background: #1a1a2e; border: 1px solid #444; border-radius: 4px;");
		return label;
	}

	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Port Binding
// ---------------------------------------------------------------------------

void VirtualSurface::bind_to_port(QWidget *widget, const QString &port_id)
{
	auto &reg = ControlRegistry::instance();
	auto *port = reg.find(port_id);
	if (!port)
		return;

	// When port changes externally, update the widget
	connect(port, &ControlPort::value_changed, widget,
		[widget](const QVariant &val) {
			if (auto *slider = qobject_cast<QSlider *>(widget)) {
				slider->blockSignals(true);
				slider->setValue(static_cast<int>(val.toDouble() * 100));
				slider->blockSignals(false);
			} else if (auto *dial = qobject_cast<QDial *>(widget)) {
				dial->blockSignals(true);
				dial->setValue(static_cast<int>(val.toDouble() * 100));
				dial->blockSignals(false);
			} else if (auto *check = qobject_cast<QCheckBox *>(widget)) {
				check->blockSignals(true);
				check->setChecked(val.toBool());
				check->blockSignals(false);
			} else if (auto *btn = qobject_cast<QPushButton *>(widget)) {
				if (btn->isCheckable()) {
					btn->blockSignals(true);
					btn->setChecked(val.toBool());
					btn->blockSignals(false);
				}
			}
		});
}

} // namespace super
