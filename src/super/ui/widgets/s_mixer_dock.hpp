#pragma once

// ============================================================================
// SMixerDock — DAW-style Mixer Dock (Example / Demo)
//
// A complete mixer dock that uses the modular SMixerChannel widget.
// Provides:
//   - Horizontal scrolling strip of mixer channels
//   - Source selector dropdown + Add button
//   - Auto-refresh when scene collection changes
//   - Group / sort controls (top toolbar)
//   - Master channel (always rightmost)
//
// Layout based on the DAW mixer image:
//   ┌─────────────────────────────────────────────────────────┐
//   │ [GROUPS] [All▼] [Source: ▼] [+ Add] [Refresh]          │
//   │                                                         │
//   │ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐│
//   │ │Drums │ │Piano │ │Vocal │ │Synth │ │Bass  │ │Master││
//   │ │  M S │ │  M S │ │  M S │ │  M S │ │  M S │ │  M S ││
//   │ │      │ │      │ │      │ │      │ │      │ │      ││
//   │ │ ▓▓▓▓ │ │ ▓▓▓▓ │ │ ▓▓▓▓ │ │ ▓▓▓▓ │ │ ▓▓▓▓ │ │ ▓▓▓▓ ││
//   │ │      │ │      │ │      │ │      │ │      │ │      ││
//   │ │-9.0dB│ │-12dB │ │-6.0dB│ │-inf  │ │-3.0dB│ │ 0.0dB││
//   │ │ PAN  │ │ PAN  │ │ PAN  │ │ PAN  │ │ PAN  │ │ PAN  ││
//   │ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘│
//   └─────────────────────────────────────────────────────────┘
// ============================================================================

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPointer>
#include <vector>
#include <obs.h>
#include <obs.hpp>
#include <obs-frontend-api.h>

namespace super {

class SMixerChannel;

class SMixerDock : public QWidget {
	Q_OBJECT

public:
	explicit SMixerDock(QWidget *parent = nullptr);
	~SMixerDock() override;

	// --- Channel Management ---
	SMixerChannel *addChannel(obs_source_t *source);
	void removeChannel(int index);
	void clearChannels();
	int channelCount() const;
	SMixerChannel *channelAt(int index) const;

	// --- Auto-populate ---
	void populateSources();
	void autoPopulateAudioSources();

signals:
	void channelAdded(SMixerChannel *channel);
	void channelRemoved(int index);

private slots:
	void onAddChannelClicked();
	void onRefreshClicked();
	void onAutoPopulateClicked();

private:
	void setupUi();

	// OBS event handler
	static void obsEventCallback(obs_frontend_event event, void *data);
	static bool enumAudioSourcesCb(void *param, obs_source_t *source);

	// UI
	QComboBox *m_source_combo = nullptr;
	std::vector<OBSWeakSource> m_combo_sources;

	QPushButton *m_add_btn = nullptr;
	QPushButton *m_refresh_btn = nullptr;
	QPushButton *m_auto_btn = nullptr;
	QScrollArea *m_scroll_area = nullptr;
	QHBoxLayout *m_channels_layout = nullptr;

	// Channels
	std::vector<QPointer<SMixerChannel>> m_channels;
};

} // namespace super
