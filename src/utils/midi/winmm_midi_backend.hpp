#pragma once

#include "midi_backend.hpp"

#include <Windows.h>
#include <mmsystem.h>

#include <vector>

class WinMmMidiBackend : public MidiBackend {
	Q_OBJECT

public:
	explicit WinMmMidiBackend(QObject *parent = nullptr);
	~WinMmMidiBackend() override;

	// --- Input ---
	QStringList available_devices() const override;
	bool open_device(int index) override;
	void close_all() override;

	// --- Output ---
	QStringList available_output_devices() const override;
	bool open_output_device(int index) override;
	void close_all_outputs() override;
	void send_cc(int device, int channel, int cc, int value) override;

private:
	static void CALLBACK midi_in_proc(HMIDIIN hMidi, UINT wMsg,
		DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);

	struct OpenDevice {
		HMIDIIN handle;
		int index;
	};
	std::vector<OpenDevice> m_open_devices;

	struct OpenOutputDevice {
		HMIDIOUT handle;
		int index;
	};
	std::vector<OpenOutputDevice> m_open_outputs;
};
