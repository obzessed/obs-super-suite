#include "winmm_midi_backend.hpp"

#include <obs.h>
#include <plugin-support.h>

#pragma comment(lib, "winmm.lib")

WinMmMidiBackend::WinMmMidiBackend(QObject *parent)
	: MidiBackend(parent)
{
}

WinMmMidiBackend::~WinMmMidiBackend()
{
	WinMmMidiBackend::close_all();
}

QStringList WinMmMidiBackend::available_devices() const
{
	QStringList devices;
	UINT count = midiInGetNumDevs();
	for (UINT i = 0; i < count; i++) {
		MIDIINCAPSW caps;
		if (midiInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
			devices.append(QString::fromWCharArray(caps.szPname));
		} else {
			devices.append(QString("MIDI Device %1").arg(i));
		}
	}
	return devices;
}

bool WinMmMidiBackend::open_device(int index)
{
	// Check if already open
	for (const auto &dev : m_open_devices) {
		if (dev.index == index)
			return true;
	}

	HMIDIIN handle = nullptr;
	MMRESULT result = midiInOpen(&handle, (UINT)index,
		(DWORD_PTR)midi_in_proc, (DWORD_PTR)this, CALLBACK_FUNCTION);

	if (result != MMSYSERR_NOERROR) {
		obs_log(LOG_WARNING, "WinMM: failed to open MIDI device %d (error %u)",
			index, result);
		return false;
	}

	midiInStart(handle);
	m_open_devices.push_back({handle, index});
	obs_log(LOG_INFO, "WinMM: opened MIDI device %d", index);
	return true;
}

void WinMmMidiBackend::close_all()
{
	for (auto &dev : m_open_devices) {
		midiInStop(dev.handle);
		midiInReset(dev.handle);
		midiInClose(dev.handle);
	}
	m_open_devices.clear();
}

void CALLBACK WinMmMidiBackend::midi_in_proc(HMIDIIN hMidi, UINT wMsg,
	DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	Q_UNUSED(dwParam2);

	if (wMsg != MIM_DATA)
		return;

	auto *self = reinterpret_cast<WinMmMidiBackend *>(dwInstance);

	int status = (int)(dwParam1 & 0xFF);
	int data1 = (int)((dwParam1 >> 8) & 0xFF);
	int data2 = (int)((dwParam1 >> 16) & 0xFF);

	// Resolve device index from handle
	int device_index = -1;
	for (const auto &dev : self->m_open_devices) {
		if (dev.handle == hMidi) {
			device_index = dev.index;
			break;
		}
	}

	// Post to Qt main thread (callback runs on a WinMM thread)
	QMetaObject::invokeMethod(self,
		[self, device_index, status, data1, data2]() {
			emit self->midi_message(device_index, status, data1, data2);
		},
		Qt::QueuedConnection);
}
