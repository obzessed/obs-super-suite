#pragma once

// ============================================================================
// Universal Control API — Core Types
// Part of the Super Suite "Super" platform (src/super/)
// ============================================================================

#include <QString>
#include <QVariant>
#include <QPointF>

namespace super {

// ---------------------------------------------------------------------------
// ControlType — The fundamental classification of a control port.
// ---------------------------------------------------------------------------
enum class ControlType {
	// Virtual / Organizational
	Command,	// Stateless trigger (fires, carries no persistent value)
	Folder,		// Grouping node (no value, organizational only)

	// Continuous Data
	Range,		// Normalized 0.0 – 1.0 (faders, knobs, sliders)
	Float,		// Unbounded double  (dB, Hz, seconds)
	Int,		// Discrete integer  (counter, index, step)

	// Temporal
	Time,		// Duration (ms) or absolute timestamp

	// Rich Data
	String,		// Text (display labels, OLED text)
	Color,		// RGBA  (scene tints, LED feedback)
	Blob,		// Raw bytes (SysEx, HID reports, custom structs)

	// Logic / State
	Toggle,		// Boolean on/off (mute, solo, arm)
	Select,		// Index into a named list (combo box, radio group)

	// Spatial
	XYPad		// 2D vector  (joystick, pan/tilt)
};

// ---------------------------------------------------------------------------
// FeedbackPolicy — How a port communicates value changes back to its source.
// ---------------------------------------------------------------------------
enum class FeedbackPolicy {
	None,			// Fire-and-forget (no echo)
	BiDirectional,	// Source ↔ Target stay in sync (motorized fader)
	Echo			// Target echoes received value back to source
};

// ---------------------------------------------------------------------------
// PersistencePolicy — How a variable's value is stored.
// ---------------------------------------------------------------------------
enum class PersistencePolicy {
	Session,	// Lost on OBS restart
	Persist		// Saved to disk (JSON config)
};

// ---------------------------------------------------------------------------
// ControlDescriptor — Metadata that fully describes a port before creation.
// Used by the Registry to instantiate and configure ports.
// ---------------------------------------------------------------------------
struct ControlDescriptor {
	QString id;				// Hierarchical ID: "audio.mic.vol"
	QString display_name;	// Human-readable: "Microphone Volume"
	QString group;			// Grouping hint: "audio.mic"
	ControlType type = ControlType::Range;
	FeedbackPolicy feedback = FeedbackPolicy::None;

	// Type-specific constraints
	double range_min = 0.0;
	double range_max = 1.0;
	double default_value = 0.0;
	QStringList select_options;	// For ControlType::Select
};

// ---------------------------------------------------------------------------
// Utility: Human-readable names for ControlType (for debug / UI).
// ---------------------------------------------------------------------------
inline QString control_type_name(ControlType t) {
	switch (t) {
	case ControlType::Command:	return QStringLiteral("Command");
	case ControlType::Folder:	return QStringLiteral("Folder");
	case ControlType::Range:	return QStringLiteral("Range");
	case ControlType::Float:	return QStringLiteral("Float");
	case ControlType::Int:		return QStringLiteral("Int");
	case ControlType::Time:		return QStringLiteral("Time");
	case ControlType::String:	return QStringLiteral("String");
	case ControlType::Color:	return QStringLiteral("Color");
	case ControlType::Blob:		return QStringLiteral("Blob");
	case ControlType::Toggle:	return QStringLiteral("Toggle");
	case ControlType::Select:	return QStringLiteral("Select");
	case ControlType::XYPad:	return QStringLiteral("XYPad");
	}
	return QStringLiteral("Unknown");
}

} // namespace super
