#pragma once

#include <QObject>
#include <QStringList>

// Abstract MIDI backend.
// Subclass this to add support for different MIDI APIs (WinMM, RtMidi, CoreMIDI, etc.)
class MidiBackend : public QObject {
	Q_OBJECT

public:
	explicit MidiBackend(QObject *parent = nullptr) : QObject(parent) {}
	~MidiBackend() override = default;

	// --- Input ---

	// Enumerate available MIDI input devices
	virtual QStringList available_devices() const = 0;

	// Open a device by index. Returns true on success.
	virtual bool open_device(int index) = 0;

	// Close all open input devices
	virtual void close_all() = 0;

	// --- Output ---

	// Enumerate available MIDI output devices (may differ from inputs)
	virtual QStringList available_output_devices() const = 0;

	// Open an output device by index. Returns true on success.
	virtual bool open_output_device(int index) = 0;

	// Close all open output devices
	virtual void close_all_outputs() = 0;

	// Send a CC message to an output device.
	// If device == -1, send to all open output devices.
	virtual void send_cc(int device, int channel, int cc, int value) = 0;

signals:
	// Raw MIDI message from device
	// status: full status byte (msg_type | channel)
	// data1:  first data byte  (CC number / note number)
	// data2:  second data byte (CC value / velocity)
	void midi_message(int device, int status, int data1, int data2);
};
