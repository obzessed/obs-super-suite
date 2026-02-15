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

	QStringList available_devices() const override;
	bool open_device(int index) override;
	void close_all() override;

private:
	static void CALLBACK midi_in_proc(HMIDIIN hMidi, UINT wMsg,
		DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);

	struct OpenDevice {
		HMIDIIN handle;
		int index;
	};

	std::vector<OpenDevice> m_open_devices;
};
