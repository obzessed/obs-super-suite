#pragma once

#include <QObject>
#include <QStringList>

// Abstract MIDI input backend.
// Subclass this to add support for different MIDI APIs (WinMM, RtMidi, CoreMIDI, etc.)
class MidiBackend : public QObject {
	Q_OBJECT

public:
	explicit MidiBackend(QObject *parent = nullptr) : QObject(parent) {}
	~MidiBackend() override = default;

	// Enumerate available MIDI input devices
	virtual QStringList available_devices() const = 0;

	// Open a device by index. Returns true on success.
	virtual bool open_device(int index) = 0;

	// Close all open devices
	virtual void close_all() = 0;

signals:
	// Raw MIDI message from device
	// status: full status byte (msg_type | channel)
	// data1:  first data byte  (CC number / note number)
	// data2:  second data byte (CC value / velocity)
	void midi_message(int device, int status, int data1, int data2);
};
