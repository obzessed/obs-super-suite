#include "midi_router.hpp"
#include "winmm_midi_backend.hpp"

#include <obs.h>
#include <plugin-support.h>

#include <QJsonArray>

#include <algorithm>

static MidiRouter *s_instance = nullptr;

MidiRouter *MidiRouter::instance()
{
	if (!s_instance) {
		s_instance = new MidiRouter();
	}
	return s_instance;
}

void MidiRouter::cleanup()
{
	if (s_instance) {
		s_instance->close_all();
		delete s_instance;
		s_instance = nullptr;
	}
}

MidiRouter::MidiRouter()
{
	m_backend = std::make_unique<WinMmMidiBackend>(this);
	connect(m_backend.get(), &MidiBackend::midi_message,
		this, &MidiRouter::on_midi_message);
}

MidiRouter::~MidiRouter()
{
	close_all();
}

MidiBackend *MidiRouter::backend() const
{
	return m_backend.get();
}

QStringList MidiRouter::available_devices() const
{
	return m_backend ? m_backend->available_input_devices() : QStringList();
}

bool MidiRouter::open_device(int index)
{
	return m_backend ? m_backend->open_input_device(index) : false;
}

void MidiRouter::open_all_devices()
{
	if (!m_backend)
		return;
	QStringList devices = m_backend->available_input_devices();
	for (int i = 0; i < devices.size(); i++) {
		m_backend->open_input_device(i);
	}
}

void MidiRouter::close_all()
{
	if (m_backend) {
		m_backend->close_all_inputs();
	}
}

// ---------------------------------------------------------------------------
// Binding management
// ---------------------------------------------------------------------------

void MidiRouter::add_binding(const MidiBinding &b)
{
	m_bindings.append(b);
}

void MidiRouter::update_binding_at(int index, const MidiBinding &b)
{
	if (index >= 0 && index < m_bindings.size())
		m_bindings[index] = b;
}

void MidiRouter::remove_binding_at(int index)
{
	if (index >= 0 && index < m_bindings.size())
		m_bindings.removeAt(index);
}

void MidiRouter::remove_binding(const QString &widget_id, const QString &control_name)
{
	m_bindings.erase(
		std::remove_if(m_bindings.begin(), m_bindings.end(),
			[&](const MidiBinding &b) {
				return b.widget_id == widget_id &&
				       b.control_name == control_name;
			}),
		m_bindings.end());
}

void MidiRouter::remove_all_bindings(const QString &widget_id)
{
	m_bindings.erase(
		std::remove_if(m_bindings.begin(), m_bindings.end(),
			[&](const MidiBinding &b) {
				return b.widget_id == widget_id;
			}),
		m_bindings.end());
}

QVector<MidiBinding> MidiRouter::bindings_for(const QString &widget_id) const
{
	QVector<MidiBinding> result;
	for (const auto &b : m_bindings) {
		if (b.widget_id == widget_id)
			result.append(b);
	}
	return result;
}

QVector<int> MidiRouter::binding_indices_for(const QString &widget_id, const QString &control_name) const
{
	QVector<int> result;
	for (int i = 0; i < m_bindings.size(); i++) {
		if (m_bindings[i].widget_id == widget_id &&
		    m_bindings[i].control_name == control_name)
			result.append(i);
	}
	return result;
}

const QVector<MidiBinding> &MidiRouter::all_bindings() const
{
	return m_bindings;
}

// ---------------------------------------------------------------------------
// MIDI Learn
// ---------------------------------------------------------------------------

void MidiRouter::start_learn(const QString &widget_id, const QString &control_name)
{
	m_learning = true;
	m_learn_widget_id = widget_id;
	m_learn_control_name = control_name;
	obs_log(LOG_INFO, "MIDI Learn: waiting for input → %s / %s",
		widget_id.toUtf8().constData(),
		control_name.toUtf8().constData());
}

void MidiRouter::cancel_learn()
{
	if (m_learning) {
		m_learning = false;
		m_learn_widget_id.clear();
		m_learn_control_name.clear();
		emit learn_cancelled();
		obs_log(LOG_INFO, "MIDI Learn: cancelled");
	}
}

bool MidiRouter::is_learning() const
{
	return m_learning;
}

// ---------------------------------------------------------------------------
// Message dispatch
// ---------------------------------------------------------------------------

void MidiRouter::on_midi_message(int device, int status, int data1, int data2)
{
	int msg_type = status & 0xF0;
	int channel = status & 0x0F;

	// --- Learn mode: capture the first CC ---
	if (m_learning && msg_type == 0xB0) {
		MidiBinding binding;
		binding.device_index = device;
		binding.channel = channel;
		binding.cc = data1;
		binding.type = MidiBinding::CC;
		binding.widget_id = m_learn_widget_id;
		binding.control_name = m_learn_control_name;

		// Don't add here — the popup's on_binding_learned adds it
		// with the user's mapping preferences applied.

		m_learning = false;
		m_learn_widget_id.clear();
		m_learn_control_name.clear();

		emit binding_learned(binding);

		obs_log(LOG_INFO, "MIDI Learn: bound CC %d (Ch %d, Dev %d) → %s / %s",
			binding.cc, binding.channel, binding.device_index,
			binding.widget_id.toUtf8().constData(),
			binding.control_name.toUtf8().constData());
		return;
	}

	// --- Normal dispatch ---
	if (msg_type == 0xB0) {
		// Control Change
		for (auto &b : m_bindings) {
			if (b.type == MidiBinding::CC &&
			    b.enabled &&
			    b.cc == data1 &&
			    b.channel == channel &&
			    (b.device_index == -1 || b.device_index == device)) {

				if (b.map_mode == MidiBinding::Toggle ||
				    b.map_mode == MidiBinding::Trigger) {
					// Rising-edge only: emit once when crossing above threshold
					bool was_above = b.invert ? (b.last_raw < b.threshold)
					                          : (b.last_raw > b.threshold);
					bool now_above = b.invert ? (data2 < b.threshold)
					                          : (data2 > b.threshold);
					b.last_raw = data2;
					if (now_above && !was_above) {
						emit midi_cc_received(b.widget_id, b.control_name, 1.0);
					}
				} else {
					double mapped = b.map_value(data2);
					emit midi_cc_received(b.widget_id, b.control_name, mapped);
				}
			}
		}
	} else if (msg_type == 0x90) {
		// Note On
		for (const auto &b : m_bindings) {
			if (b.type == MidiBinding::NoteOn &&
			    b.enabled &&
			    b.cc == data1 &&
			    b.channel == channel &&
			    (b.device_index == -1 || b.device_index == device)) {
				emit midi_note_received(b.widget_id, b.control_name, data2);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

QJsonObject MidiRouter::save() const
{
	QJsonArray arr;
	for (const auto &b : m_bindings) {
		arr.append(b.to_json());
	}

	QJsonObject obj;
	obj["bindings"] = arr;
	return obj;
}

void MidiRouter::load(const QJsonObject &obj)
{
	m_bindings.clear();
	QJsonArray arr = obj["bindings"].toArray();
	for (const auto &val : arr) {
		m_bindings.append(MidiBinding::from_json(val.toObject()));
	}
	obs_log(LOG_INFO, "MidiRouter: loaded %d bindings", m_bindings.size());
}

// ---------------------------------------------------------------------------
// MidiBinding
// ---------------------------------------------------------------------------

double MidiBinding::map_value(int raw) const
{
	switch (map_mode) {
	case Toggle:
	case Trigger: {
		bool on = invert ? (raw < threshold) : (raw > threshold);
		return on ? 1.0 : 0.0;
	}
	case Select: {
		int clamped = std::clamp(raw, 0, 127);
		if (invert)
			clamped = 127 - clamped;

		// Use explicit thresholds if available
		if (!select_thresholds.isEmpty()) {
			for (int i = 0; i < select_thresholds.size(); i++) {
				if (clamped <= select_thresholds[i])
					return (double)i;
			}
			return (double)select_thresholds.size();
		}

		// Even distribution across select_count items
		if (select_count > 1) {
			double norm = clamped / 127.0;
			int idx = (int)qRound(norm * (select_count - 1));
			return (double)std::clamp(idx, 0, select_count - 1);
		}
		return 0.0;
	}
	case Range:
	default: {
		// Clamp to input range
		int clamped = std::clamp(raw, input_min, input_max);

		// Normalize to 0.0-1.0
		double normalized;
		if (input_max == input_min)
			normalized = 0.0;
		else
			normalized = (double)(clamped - input_min) / (double)(input_max - input_min);

		if (invert)
			normalized = 1.0 - normalized;

		// Map to output range
		return output_min + normalized * (output_max - output_min);
	}
	}
}

QJsonObject MidiBinding::to_json() const
{
	QJsonObject obj;
	obj["device"] = device_index;
	obj["channel"] = channel;
	obj["cc"] = cc;
	obj["type"] = (int)type;
	obj["widgetId"] = widget_id;
	obj["controlName"] = control_name;
	obj["mapMode"] = (int)map_mode;
	obj["inputMin"] = input_min;
	obj["inputMax"] = input_max;
	obj["outputMin"] = output_min;
	obj["outputMax"] = output_max;
	obj["threshold"] = threshold;
	obj["selectCount"] = select_count;
	if (!select_thresholds.isEmpty()) {
		QJsonArray arr;
		for (int v : select_thresholds)
			arr.append(v);
		obj["selectThresholds"] = arr;
	}
	obj["invert"] = invert;
	obj["enabled"] = enabled;
	return obj;
}

MidiBinding MidiBinding::from_json(const QJsonObject &obj)
{
	MidiBinding b;
	b.device_index = obj["device"].toInt(-1);
	b.channel = obj["channel"].toInt(0);
	b.cc = obj["cc"].toInt(0);
	b.type = (Type)obj["type"].toInt(0);
	b.widget_id = obj["widgetId"].toString();
	b.control_name = obj["controlName"].toString();
	b.map_mode = (MapMode)obj["mapMode"].toInt(0);
	b.input_min = obj["inputMin"].toInt(0);
	b.input_max = obj["inputMax"].toInt(127);
	b.output_min = obj["outputMin"].toDouble(0.0);
	b.output_max = obj["outputMax"].toDouble(127.0);
	b.threshold = obj["threshold"].toInt(63);
	b.select_count = obj["selectCount"].toInt(0);
	if (obj.contains("selectThresholds")) {
		QJsonArray arr = obj["selectThresholds"].toArray();
		for (const auto &v : arr)
			b.select_thresholds.append(v.toInt());
	}
	b.invert = obj["invert"].toBool(false);
	b.enabled = obj["enabled"].toBool(true);
	return b;
}

