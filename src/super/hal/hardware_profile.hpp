#pragma once

// ============================================================================
// Hardware Abstraction Layer (HAL) — Hardware Profile
// Decouples physical devices from logical control ports.
// ============================================================================

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>

namespace super {

// ---------------------------------------------------------------------------
// Encoder Relative Mode — How encoder delta values are interpreted.
// ---------------------------------------------------------------------------
enum class EncoderMode {
	Absolute,					// Standard 0-127
	RelativeTwosComplement,		// 1 = +1, 127 = -1
	RelativeBinaryOffset,		// 64 = 0, 65 = +1, 63 = -1
	RelativeSignedBit			// 1 = +1, 65 = -1
};

// ---------------------------------------------------------------------------
// HardwareControl — One physical input on a device.
// ---------------------------------------------------------------------------
struct HardwareControl {
	QString name;		// "fader1", "encoder1", "btn_layer_a"
	QString type;		// "range", "encoder", "toggle", "trigger"

	// MIDI-specific
	int midi_status = 0;	// e.g. 0xB0 (CC), 0x90 (Note)
	int midi_data1 = 0;	// CC number or Note number
	EncoderMode encoder_mode = EncoderMode::Absolute;

	// Parse from JSON
	static HardwareControl from_json(const QJsonObject &obj) {
		HardwareControl c;
		c.name = obj["name"].toString();
		c.type = obj["type"].toString("range");

		auto midi = obj["midi"].toObject();
		c.midi_status = midi["status"].toInt();
		c.midi_data1 = midi["data1"].toInt();

		QString mode = midi["mode"].toString("absolute");
		if (mode == "relative_twos_complement")
			c.encoder_mode = EncoderMode::RelativeTwosComplement;
		else if (mode == "relative_binary_offset")
			c.encoder_mode = EncoderMode::RelativeBinaryOffset;
		else if (mode == "relative_signed_bit")
			c.encoder_mode = EncoderMode::RelativeSignedBit;
		else
			c.encoder_mode = EncoderMode::Absolute;

		return c;
	}
};

// ---------------------------------------------------------------------------
// HardwareProfile — A device descriptor loaded from JSON.
// ---------------------------------------------------------------------------
struct HardwareProfile {
	QString vendor;
	QString model;
	QList<HardwareControl> controls;

	// Full device ID: "vendor.model" (lowercased, spaces replaced)
	QString device_id() const {
		auto clean = [](QString s) {
			return s.toLower().replace(' ', '_');
		};
		return clean(vendor) + "." + clean(model);
	}

	// Parse from JSON
	static HardwareProfile from_json(const QJsonObject &obj) {
		HardwareProfile p;
		p.vendor = obj["vendor"].toString();
		p.model = obj["model"].toString();

		auto arr = obj["controls"].toArray();
		for (const auto &v : arr)
			p.controls.append(HardwareControl::from_json(v.toObject()));

		return p;
	}

	// Load from a JSON file
	static HardwareProfile load(const QString &path) {
		QFile f(path);
		if (!f.open(QIODevice::ReadOnly))
			return {};
		auto doc = QJsonDocument::fromJson(f.readAll());
		return from_json(doc.object());
	}

	// Convert delta byte to signed increment based on encoder mode
	static int decode_encoder_delta(int raw_value, EncoderMode mode) {
		switch (mode) {
		case EncoderMode::Absolute:
			return raw_value;  // Not a delta
		case EncoderMode::RelativeTwosComplement:
			return (raw_value < 64) ? raw_value : (raw_value - 128);
		case EncoderMode::RelativeBinaryOffset:
			return raw_value - 64;
		case EncoderMode::RelativeSignedBit:
			return (raw_value & 0x40) ? -(raw_value & 0x3F)
									  : (raw_value & 0x3F);
		}
		return 0;
	}
};

} // namespace super
