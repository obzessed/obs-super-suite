#include "midi_adapter.hpp"
#include "../core/control_port.hpp"
#include "../core/control_registry.hpp"
#include "../../utils/midi/midi_backend.hpp"

#include <algorithm>

namespace super {

// ===== FilterStage ========================================================

double FilterStage::process(double val) const
{
	if (!enabled) return val;

	switch (static_cast<Type>(type)) {
	case Delay: {
		// Defer output: always store latest input as rt_target,
		// only update output (rt_last) when delay has elapsed.
		if (!rt_init) {
			rt_last = val; rt_target = val;
			rt_init = true; rt_timer.start();
			return val;
		}
		rt_target = val;
		if (rt_timer.elapsed() >= static_cast<qint64>(param1)) {
			rt_last = rt_target;
			rt_timer.restart();
		}
		return rt_last;
	}
	case Debounce: {
		// Only pass value through once it's been stable (unchanged) for param1 ms.
		// Always track latest input; emit when timer expires without change.
		if (!rt_init) {
			rt_last = val; rt_target = val;
			rt_init = true; rt_timer.start();
			return val;
		}
		if (val != rt_target) {
			// Input changed — store new target, restart timer
			rt_target = val;
			rt_timer.restart();
			return rt_last; // Keep emitting old stable value
		}
		// Input unchanged — check if stable long enough
		if (rt_timer.elapsed() >= static_cast<qint64>(param1)) {
			rt_last = rt_target; // Accept as stable
		}
		return rt_last;
	}
	case RateLimit: {
		// Smoothly approach val at a maximum rate of param1 units/second.
		// Tracks target so convergence works even when called without new input.
		if (!rt_init) {
			rt_last = val; rt_target = val;
			rt_init = true; rt_timer.start();
			return val;
		}
		rt_target = val;
		double elapsed_s = qMax(0.001, rt_timer.elapsed() / 1000.0);
		rt_timer.restart();
		double delta = rt_target - rt_last;
		double max_delta = param1 * elapsed_s;
		if (max_delta > 0.0 && std::abs(delta) > max_delta)
			delta = (delta > 0 ? max_delta : -max_delta);
		rt_last += delta;
		return rt_last;
	}
	case Deadzone: {
		if (!rt_init) { rt_last = val; rt_init = true; return val; }
		if (std::abs(val - rt_last) < param1) return rt_last;
		rt_last = val;
		return val;
	}
	case Clamp:
		return qBound(param1, val, param2);

	case Scale:
		return val * param1 + param2;
	}
	return val;
}

bool FilterStage::needs_convergence() const
{
	if (!enabled) return false;
	switch (static_cast<Type>(type)) {
	case Delay:
		return rt_init && (rt_last != rt_target);
	case Debounce:
		return rt_init && (rt_last != rt_target);
	case RateLimit:
		return rt_init && std::abs(rt_last - rt_target) > 0.001;
	default:
		return false;
	}
}

QString FilterStage::type_name() const
{
	static const char *names[] = {
		"Delay", "Debounce", "Rate Limit", "Deadzone", "Clamp", "Scale"
	};
	return names[qBound(0, type, 5)];
}

QJsonObject FilterStage::to_json() const
{
	QJsonObject o;
	o["t"] = type;
	if (!enabled) o["e"] = false;
	if (param1 != 0.0) o["p1"] = param1;
	if (param2 != 0.0) o["p2"] = param2;
	return o;
}

FilterStage FilterStage::from_json(const QJsonObject &o)
{
	FilterStage s;
	s.type = o["t"].toInt(0);
	s.enabled = o["e"].toBool(true);
	s.param1 = o["p1"].toDouble(0.0);
	s.param2 = o["p2"].toDouble(0.0);
	return s;
}

// ===== InterpStage ========================================================

double InterpStage::process(double val) const
{
	if (!enabled) return val;

	switch (static_cast<Type>(type)) {
	case Linear: return val;

	case Quantize: {
		double step = param1 > 0.0 ? param1 : 0.1;
		return std::round(val / step) * step;
	}
	case Smooth: {
		double factor = qBound(0.01, param1, 1.0);
		rt_target = val;
		if (!rt_init) { rt_accum = val; rt_init = true; }
		else rt_accum += factor * (val - rt_accum);
		return rt_accum;
	}
	case SCurve:
		return val * val * (3.0 - 2.0 * val);

	case Easing: {
		QEasingCurve curve(static_cast<QEasingCurve::Type>(
			static_cast<int>(param1)));
		return curve.valueForProgress(qBound(0.0, val, 1.0));
	}
	}
	return val;
}

QString InterpStage::type_name() const
{
	static const char *names[] = {
		"Linear", "Quantize", "Smooth", "S-Curve",
		"Easing"
	};
	return names[qBound(0, type, 4)];
}

QJsonObject InterpStage::to_json() const
{
	QJsonObject o;
	o["t"] = type;
	if (!enabled) o["e"] = false;
	if (param1 != 0.0) o["p1"] = param1;
	if (param2 != 0.0) o["p2"] = param2;
	return o;
}

InterpStage InterpStage::from_json(const QJsonObject &o)
{
	InterpStage s;
	s.type = o["t"].toInt(0);
	s.enabled = o["e"].toBool(true);
	s.param1 = o["p1"].toDouble(0.0);
	s.param2 = o["p2"].toDouble(0.0);
	return s;
}

// ===== MidiOutputBinding ==================================================

int MidiOutputBinding::map_to_midi(double port_value) const
{
	double norm;
	if (input_max == input_min) norm = 0.0;
	else norm = (port_value - input_min) / (input_max - input_min);
	norm = qBound(0.0, norm, 1.0);
	return qBound(0, static_cast<int>(qRound(
		output_min + norm * (output_max - output_min))), 127);
}

QJsonObject MidiOutputBinding::to_json() const
{
	QJsonObject o;
	o["port_id"] = port_id;
	o["device"] = device_index;
	o["channel"] = channel;
	o["cc"] = cc;
	o["in_min"] = input_min; o["in_max"] = input_max;
	o["out_min"] = output_min; o["out_max"] = output_max;
	o["enabled"] = enabled;
	o["on_change"] = on_change;
	return o;
}

MidiOutputBinding MidiOutputBinding::from_json(const QJsonObject &o)
{
	MidiOutputBinding b;
	b.port_id = o["port_id"].toString();
	b.device_index = o["device"].toInt(-1);
	b.channel = o["channel"].toInt(0);
	b.cc = o["cc"].toInt(0);
	b.input_min = o["in_min"].toDouble(0.0);
	b.input_max = o["in_max"].toDouble(1.0);
	b.output_min = o["out_min"].toInt(0);
	b.output_max = o["out_max"].toInt(127);
	b.enabled = o["enabled"].toBool(true);
	b.on_change = o["on_change"].toBool(true);
	return b;
}

// ===== MidiPortBinding — Pipeline =========================================

static double eval_curve(const QVector<ValueMapPoint> &pts, int raw, bool invert)
{
	if (raw <= pts.first().input) return pts.first().output;
	if (raw >= pts.last().input) return pts.last().output;
	for (int i = 0; i < pts.size() - 1; i++) {
		const auto &a = pts[i]; const auto &b = pts[i + 1];
		if (raw >= a.input && raw <= b.input) {
			double t = (b.input == a.input) ? 0.0
				: static_cast<double>(raw - a.input) / (b.input - a.input);
			if (invert) t = 1.0 - t;
			return a.output + t * (b.output - a.output);
		}
	}
	return pts.last().output;
}

// Full pipeline: raw → pre_filters → normalize → interp → denorm → post_filters
double MidiPortBinding::map_value(int raw) const
{
	if (is_encoder && encoder_mode != EncoderMode::Absolute) {
		int delta = HardwareProfile::decode_encoder_delta(raw, encoder_mode);
		return static_cast<double>(delta) * encoder_sensitivity;
	}

	switch (map_mode) {
	case Toggle:
	case Trigger: {
		bool on = invert ? (raw < threshold) : (raw > threshold);
		return on ? 1.0 : 0.0;
	}
	case Select: {
		int clamped = std::clamp(raw, 0, 127);
		if (invert) clamped = 127 - clamped;
		if (!select_thresholds.isEmpty()) {
			for (int i = 0; i < select_thresholds.size(); i++)
				if (clamped <= select_thresholds[i])
					return static_cast<double>(i);
			return static_cast<double>(select_thresholds.size());
		}
		if (select_count > 1) {
			double norm = clamped / 127.0;
			int idx = static_cast<int>(qRound(norm * (select_count - 1)));
			return static_cast<double>(std::clamp(idx, 0, select_count - 1));
		}
		return 0.0;
	}
	case Range:
	default: {
		// 1. Pre-filters (raw domain, as double)
		double pre = static_cast<double>(raw);
		for (const auto &f : pre_filters)
			pre = f.process(pre);

		// 2. Normalize to 0-1
		double normalized;
		int pre_int = static_cast<int>(std::round(pre));
		if (!curve_points.isEmpty() && curve_points.size() >= 2) {
			double out = eval_curve(curve_points, pre_int, invert);
			double mn = curve_points.first().output;
			double mx = curve_points.last().output;
			normalized = (mx == mn) ? 0.0 : (out - mn) / (mx - mn);
		} else {
			int clamped = std::clamp(pre_int, input_min, input_max);
			normalized = (input_max == input_min) ? 0.0
				: static_cast<double>(clamped - input_min)
				  / (input_max - input_min);
			if (invert) normalized = 1.0 - normalized;
		}

		// 3. Interp chain (0-1 domain)
		for (const auto &s : interp_stages)
			normalized = s.process(normalized);

		// 4. Denormalize to output range
		double mapped = output_min + normalized * (output_max - output_min);

		// 5. Post-filters (output domain)
		for (const auto &f : post_filters)
			mapped = f.process(mapped);

		return mapped;
	}
	}
}

bool MidiPortBinding::needs_convergence() const
{
	for (const auto &f : pre_filters)
		if (f.needs_convergence()) return true;
	// InterpStage Smooth also converges
	for (const auto &s : interp_stages) {
		if (s.enabled && s.type == InterpStage::Smooth && s.rt_init)
			if (std::abs(s.rt_accum - s.rt_target) > 0.0001)
				return true;
	}
	for (const auto &f : post_filters)
		if (f.needs_convergence()) return true;
	return false;
}

PipelinePreview MidiPortBinding::preview_pipeline(int raw) const
{
	PipelinePreview p;
	p.raw_in = raw;
	p.input_min = input_min; p.input_max = input_max;
	p.output_min = output_min; p.output_max = output_max;

	// 1. Pre-filters
	double val = static_cast<double>(raw);
	for (const auto &f : pre_filters) {
		val = f.process(val);
		p.after_pre_filter.append(val);
		p.pre_filter_enabled.append(f.enabled);
		p.pre_filter_names.append(f.type_name());
	}
	p.pre_filtered = val;

	// 2. Normalize
	int pre_int = static_cast<int>(std::round(val));
	double normalized;
	if (!curve_points.isEmpty() && curve_points.size() >= 2) {
		double out = eval_curve(curve_points, pre_int, invert);
		double mn = curve_points.first().output;
		double mx = curve_points.last().output;
		normalized = (mx == mn) ? 0.0 : (out - mn) / (mx - mn);
	} else {
		int clamped = std::clamp(pre_int, input_min, input_max);
		normalized = (input_max == input_min) ? 0.0
			: static_cast<double>(clamped - input_min)
			  / (input_max - input_min);
		if (invert) normalized = 1.0 - normalized;
	}
	p.normalized = normalized;

	// 3. Interp chain
	val = normalized;
	for (const auto &s : interp_stages) {
		val = s.process(val);
		p.after_interp.append(val);
		p.interp_enabled.append(s.enabled);
		p.interp_names.append(s.type_name());
	}

	// 4. Denormalize
	p.mapped = output_min + val * (output_max - output_min);

	// 5. Post-filters
	val = p.mapped;
	for (const auto &f : post_filters) {
		val = f.process(val);
		p.after_post_filter.append(val);
		p.post_filter_enabled.append(f.enabled);
		p.post_filter_names.append(f.type_name());
	}
	p.final_value = val;

	// Action description
	switch (action_mode) {
	case ActionMode::SetValue:
		p.action_description = QString("Set → %1").arg(val, 0, 'f', 3);
		break;
	case ActionMode::Trigger:
		p.action_description = val > 0.5 ? "Trigger ⚡" : "—";
		break;
	}

	return p;
}

// ===== MidiPortBinding — Serialization ====================================

QJsonObject MidiPortBinding::to_json() const
{
	QJsonObject obj;
	obj["device"] = device_index;
	obj["channel"] = channel;
	obj["data1"] = data1;
	obj["msg_type"] = static_cast<int>(msg_type);
	obj["port_id"] = port_id;
	obj["map_mode"] = static_cast<int>(map_mode);
	obj["input_min"] = input_min; obj["input_max"] = input_max;
	obj["output_min"] = output_min; obj["output_max"] = output_max;
	obj["threshold"] = threshold;
	if (toggle_mode != 0) obj["toggle_mode"] = toggle_mode;
	obj["select_count"] = select_count;
	if (!select_thresholds.isEmpty()) {
		QJsonArray arr;
		for (int v : select_thresholds) arr.append(v);
		obj["select_thresholds"] = arr;
	}
	obj["invert"] = invert;
	obj["enabled"] = enabled;
	obj["continuous_fire"] = continuous_fire;
	obj["continuous_fire_interval"] = continuous_fire_interval_ms;
	obj["is_encoder"] = is_encoder;
	obj["encoder_mode"] = static_cast<int>(encoder_mode);
	obj["encoder_sensitivity"] = encoder_sensitivity;

	obj["action_mode"] = static_cast<int>(action_mode);
	if (action_param1 != 500.0) obj["action_p1"] = action_param1;
	if (action_param2 != 0.0) obj["action_p2"] = action_param2;

	if (!curve_points.isEmpty()) {
		QJsonArray pts;
		for (const auto &p : curve_points) pts.append(p.to_json());
		obj["curve"] = pts;
	}
	if (!pre_filters.isEmpty()) {
		QJsonArray arr;
		for (const auto &s : pre_filters) arr.append(s.to_json());
		obj["pre_filters"] = arr;
	}
	if (!interp_stages.isEmpty()) {
		QJsonArray arr;
		for (const auto &s : interp_stages) arr.append(s.to_json());
		obj["interps"] = arr;
	}
	if (!post_filters.isEmpty()) {
		QJsonArray arr;
		for (const auto &s : post_filters) arr.append(s.to_json());
		obj["post_filters"] = arr;
	}
	return obj;
}

MidiPortBinding MidiPortBinding::from_json(const QJsonObject &obj)
{
	MidiPortBinding b;
	b.device_index = obj["device"].toInt(-1);
	b.channel = obj["channel"].toInt(0);
	b.data1 = obj["data1"].toInt(0);
	b.msg_type = static_cast<MidiPortBinding::MsgType>(obj["msg_type"].toInt(0));
	b.port_id = obj["port_id"].toString();
	b.map_mode = static_cast<MidiPortBinding::MapMode>(obj["map_mode"].toInt(0));
	b.input_min = obj["input_min"].toInt(0);
	b.input_max = obj["input_max"].toInt(127);
	b.output_min = obj["output_min"].toDouble(0.0);
	b.output_max = obj["output_max"].toDouble(1.0);
	b.threshold = obj["threshold"].toInt(63);
	b.toggle_mode = obj["toggle_mode"].toInt(0);
	b.select_count = obj["select_count"].toInt(0);
	if (obj.contains("select_thresholds"))
		for (const auto &v : obj["select_thresholds"].toArray())
			b.select_thresholds.append(v.toInt());
	b.invert = obj["invert"].toBool(false);
	b.enabled = obj["enabled"].toBool(true);
	b.continuous_fire = obj["continuous_fire"].toBool(false);
	b.continuous_fire_interval_ms = obj["continuous_fire_interval"].toInt(100);
	b.is_encoder = obj["is_encoder"].toBool(false);
	b.encoder_mode = static_cast<EncoderMode>(obj["encoder_mode"].toInt(0));
	b.encoder_sensitivity = obj["encoder_sensitivity"].toDouble(1.0);

	b.action_mode = static_cast<ActionMode>(obj["action_mode"].toInt(0));
	b.action_param1 = obj["action_p1"].toDouble(500.0);
	b.action_param2 = obj["action_p2"].toDouble(0.0);

	if (obj.contains("curve"))
		for (const auto &v : obj["curve"].toArray())
			b.curve_points.append(ValueMapPoint::from_json(v.toObject()));
	if (obj.contains("pre_filters"))
		for (const auto &v : obj["pre_filters"].toArray())
			b.pre_filters.append(FilterStage::from_json(v.toObject()));
	if (obj.contains("interps"))
		for (const auto &v : obj["interps"].toArray())
			b.interp_stages.append(InterpStage::from_json(v.toObject()));
	if (obj.contains("post_filters"))
		for (const auto &v : obj["post_filters"].toArray())
			b.post_filters.append(FilterStage::from_json(v.toObject()));
	return b;
}

// ===== MidiAdapter ========================================================

MidiAdapter::MidiAdapter(QObject *parent) : QObject(parent)
{
	// Convergence timer: 16ms (~60fps) to keep time-based filters ticking
	m_convergence_timer = new QTimer(this);
	m_convergence_timer->setInterval(16);
	connect(m_convergence_timer, &QTimer::timeout,
			this, &MidiAdapter::on_convergence_tick);
	m_convergence_timer->start();
}

MidiAdapter::~MidiAdapter()
{
	if (m_convergence_timer) m_convergence_timer->stop();
	for (auto *t : m_continuous_timers) delete t;
	m_continuous_timers.clear();
	detach();
}

void MidiAdapter::attach(MidiBackend *backend)
{
	if (m_backend) detach();
	m_backend = backend;
	if (m_backend)
		connect(m_backend, &MidiBackend::midi_message,
				this, &MidiAdapter::on_midi_message);
}

void MidiAdapter::detach()
{
	if (m_backend) {
		disconnect(m_backend, &MidiBackend::midi_message,
				   this, &MidiAdapter::on_midi_message);
		m_backend = nullptr;
	}
}

bool MidiAdapter::is_attached() const { return m_backend != nullptr; }
MidiBackend *MidiAdapter::backend() const { return m_backend; }

// --- Input Binding Management ---

void MidiAdapter::add_binding(const MidiPortBinding &b) { m_bindings.append(b); }

void MidiAdapter::remove_binding(const QString &port_id)
{
	for (int i = m_bindings.size() - 1; i >= 0; --i)
		if (m_bindings[i].port_id == port_id) stop_continuous_fire(i);
	m_bindings.erase(std::remove_if(m_bindings.begin(), m_bindings.end(),
		[&](const MidiPortBinding &b) { return b.port_id == port_id; }),
		m_bindings.end());
}

void MidiAdapter::remove_all_bindings()
{
	for (auto *t : m_continuous_timers) delete t;
	m_continuous_timers.clear();
	m_bindings.clear();
}

QVector<MidiPortBinding> MidiAdapter::bindings_for(const QString &port_id) const
{
	QVector<MidiPortBinding> r;
	for (const auto &b : m_bindings)
		if (b.port_id == port_id) r.append(b);
	return r;
}

const QVector<MidiPortBinding> &MidiAdapter::all_bindings() const { return m_bindings; }

// --- Output Binding Management ---

void MidiAdapter::add_output(const MidiOutputBinding &o) { m_outputs.append(o); }

void MidiAdapter::remove_output(const QString &port_id)
{
	m_outputs.erase(std::remove_if(m_outputs.begin(), m_outputs.end(),
		[&](const MidiOutputBinding &o) { return o.port_id == port_id; }),
		m_outputs.end());
}

void MidiAdapter::remove_all_outputs() { m_outputs.clear(); }

QVector<MidiOutputBinding> MidiAdapter::outputs_for(const QString &port_id) const
{
	QVector<MidiOutputBinding> r;
	for (const auto &o : m_outputs)
		if (o.port_id == port_id) r.append(o);
	return r;
}

const QVector<MidiOutputBinding> &MidiAdapter::all_outputs() const { return m_outputs; }

// --- MIDI Learn ---

void MidiAdapter::start_learn(const QString &port_id)
{ m_learning = true; m_learn_port_id = port_id; }

void MidiAdapter::cancel_learn()
{
	if (m_learning) {
		m_learning = false; m_learn_port_id.clear();
		emit learn_cancelled();
	}
}

bool MidiAdapter::is_learning() const { return m_learning; }

// --- Hardware Profile ---

void MidiAdapter::load_profile(const HardwareProfile &profile) { m_profile = profile; }
const HardwareProfile &MidiAdapter::active_profile() const { return m_profile; }

// --- Continuous Fire ---

void MidiAdapter::start_continuous_fire(int bi)
{
	if (m_continuous_timers.contains(bi)) return;
	if (bi < 0 || bi >= m_bindings.size()) return;
	auto &b = m_bindings[bi];
	auto *timer = new QTimer(this);
	timer->setInterval(qMax(16, b.continuous_fire_interval_ms));
	connect(timer, &QTimer::timeout, this, [this, bi]() {
		if (bi >= m_bindings.size()) { stop_continuous_fire(bi); return; }
		auto &b = m_bindings[bi];
		if (!b.currently_above || !b.continuous_fire)
			{ stop_continuous_fire(bi); return; }
		auto *port = ControlRegistry::instance().find(b.port_id);
		if (port) { port->set_value(QVariant(1.0)); emit midi_dispatched(b.port_id, 1.0); }
	});
	timer->start();
	m_continuous_timers.insert(bi, timer);
}

void MidiAdapter::stop_continuous_fire(int bi)
{
	if (auto *t = m_continuous_timers.take(bi)) { t->stop(); delete t; }
}

// --- Feedback ---

void MidiAdapter::send_feedback(const QString &port_id, double value)
{
	if (!m_backend) return;
	for (auto &o : m_outputs) {
		if (o.port_id != port_id || !o.enabled) continue;
		int midi_val = o.map_to_midi(value);
		if (o.on_change && midi_val == static_cast<int>(o.last_sent)) continue;
		o.last_sent = midi_val;
		m_backend->send_cc(o.device_index, o.channel, o.cc, midi_val);
	}
}

// --- Action dispatch helper ---

static void dispatch_action(ControlPort *port, double value,
	ActionMode mode, double param1, double param2)
{
	switch (mode) {
	case ActionMode::SetValue:
		port->set_value(QVariant(value));
		break;
	case ActionMode::Trigger:
		port->set_value(QVariant(1.0));
		// Reset after one frame via singleshot
		QTimer::singleShot(50, port, [port]() {
			port->set_value(QVariant(0.0));
		});
		break;
	}
}

// --- Convergence tick (keeps time-based filters ticking) ---

void MidiAdapter::on_convergence_tick()
{
	for (auto &b : m_bindings) {
		if (!b.enabled || !b.needs_convergence()) continue;
		if (b.map_mode != MidiPortBinding::Range) continue;

		auto *port = ControlRegistry::instance().find(b.port_id);
		if (!port) continue;

		// Re-process with last known raw value
		double mapped = b.map_value(b.last_raw);
		dispatch_action(port, mapped, b.action_mode,
			b.action_param1, b.action_param2);
		emit midi_dispatched(b.port_id, mapped);
		send_feedback(b.port_id, mapped);
	}
}

// --- MIDI Dispatch ---


void MidiAdapter::on_midi_message(int device, int status, int data1, int data2)
{
	int msg_type = status & 0xF0;
	int channel = status & 0x0F;

	if (m_learning && msg_type == 0xB0) {
		MidiPortBinding binding;
		binding.device_index = device;
		binding.channel = channel;
		binding.data1 = data1;
		binding.msg_type = MidiPortBinding::CC;
		binding.port_id = m_learn_port_id;
		for (const auto &ctrl : m_profile.controls) {
			if (ctrl.midi_status == (msg_type | channel) &&
				ctrl.midi_data1 == data1 && ctrl.type == "encoder") {
				binding.is_encoder = true;
				binding.encoder_mode = ctrl.encoder_mode;
				break;
			}
		}
		m_learning = false; m_learn_port_id.clear();
		emit binding_learned(binding);
		return;
	}

	if (msg_type == 0xB0) {
		for (int bi = 0; bi < m_bindings.size(); bi++) {
			auto &b = m_bindings[bi];
			if (b.msg_type != MidiPortBinding::CC || !b.enabled) continue;
			if (b.data1 != data1 || b.channel != channel) continue;
			if (b.device_index != -1 && b.device_index != device) continue;

			auto *port = ControlRegistry::instance().find(b.port_id);
			if (!port) continue;

			if (b.map_mode == MidiPortBinding::Toggle ||
				b.map_mode == MidiPortBinding::Trigger) {
				bool was = b.invert ? (b.last_raw < b.threshold) : (b.last_raw > b.threshold);
				bool now = b.invert ? (data2 < b.threshold) : (data2 > b.threshold);
				b.last_raw = data2; b.currently_above = now;
				if (b.map_mode == MidiPortBinding::Toggle) {
					if (now && !was) {
						double new_val;
						switch (b.toggle_mode) {
						case 1:  new_val = 1.0; break;          // Check (set on)
						case 2:  new_val = 0.0; break;          // Uncheck (set off)
						default: // Toggle (flip)
							new_val = port->as_double() > 0.5 ? 0.0 : 1.0;
							break;
						}
						port->set_value(QVariant(new_val));
						emit midi_dispatched(b.port_id, new_val);
						send_feedback(b.port_id, new_val);
					}
				} else {
					if (now && !was) {
						dispatch_action(port, 1.0, b.action_mode,
							b.action_param1, b.action_param2);
						emit midi_dispatched(b.port_id, 1.0);
						send_feedback(b.port_id, 1.0);
						if (b.continuous_fire) start_continuous_fire(bi);
					} else if (!now && was) {
						stop_continuous_fire(bi);
					}
				}
			} else if (b.map_mode == MidiPortBinding::Select) {
				double idx = b.map_value(data2);
				dispatch_action(port, idx, b.action_mode,
					b.action_param1, b.action_param2);
				emit midi_dispatched(b.port_id, idx);
				send_feedback(b.port_id, idx);
				b.last_raw = data2;
			} else {
				if (b.is_encoder && b.encoder_mode != EncoderMode::Absolute) {
					double delta = b.map_value(data2);
					double next = qBound(b.output_min,
						port->as_double() + delta, b.output_max);
					dispatch_action(port, next, b.action_mode,
						b.action_param1, b.action_param2);
					emit midi_dispatched(b.port_id, next);
					send_feedback(b.port_id, next);
				} else {
					double mapped = b.map_value(data2);
					dispatch_action(port, mapped, b.action_mode,
						b.action_param1, b.action_param2);
					emit midi_dispatched(b.port_id, mapped);
					send_feedback(b.port_id, mapped);
				}
				b.last_raw = data2;
			}
		}
	} else if (msg_type == 0x90) {
		for (auto &b : m_bindings) {
			if (b.msg_type != MidiPortBinding::NoteOn || !b.enabled) continue;
			if (b.data1 != data1 || b.channel != channel) continue;
			if (b.device_index != -1 && b.device_index != device) continue;
			auto *port = ControlRegistry::instance().find(b.port_id);
			if (!port) continue;
			if (data2 > 0) {
				double val;
				if (b.map_mode == MidiPortBinding::Toggle) {
					val = port->as_double() > 0.5 ? 0.0 : 1.0;
				} else if (b.map_mode == MidiPortBinding::Trigger) {
					val = 1.0;
				} else {
					val = b.map_value(data2);
				}
				dispatch_action(port, val, b.action_mode,
					b.action_param1, b.action_param2);
				emit midi_dispatched(b.port_id, val);
				send_feedback(b.port_id, val);
			}
		}
	}
}

// --- Persistence ---

QJsonObject MidiAdapter::save() const
{
	QJsonObject obj;
	QJsonArray bindings_arr;
	for (const auto &b : m_bindings) bindings_arr.append(b.to_json());
	obj["bindings"] = bindings_arr;

	if (!m_outputs.isEmpty()) {
		QJsonArray outputs_arr;
		for (const auto &o : m_outputs) outputs_arr.append(o.to_json());
		obj["outputs"] = outputs_arr;
	}
	return obj;
}

void MidiAdapter::load(const QJsonObject &obj)
{
	m_bindings.clear();
	for (const auto &v : obj["bindings"].toArray())
		m_bindings.append(MidiPortBinding::from_json(v.toObject()));

	m_outputs.clear();
	if (obj.contains("outputs"))
		for (const auto &v : obj["outputs"].toArray())
			m_outputs.append(MidiOutputBinding::from_json(v.toObject()));
}

} // namespace super
