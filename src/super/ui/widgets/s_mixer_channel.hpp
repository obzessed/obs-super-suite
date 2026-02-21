#pragma once

// ============================================================================
// SMixerChannel — Complete DAW-style mixer channel strip widget
//
// Composes all individual mixer components into a single channel strip:
//
//   ┌──────────────────────────────────────┐
//   │ [Color Strip]                        │
//   │ [Channel Name Bar]                   │
//   │ [M] [S] [•]  (Control Bar)           │
//   │ [EFFECTS]  (label, in side panel)    │
//   │ [Bus: Master]  (Bus Selector)        │
//   │  [——●——]        (Pan Slider)         │
//   │                                      │
//   │  -6dB ─ ┌────┐ ║║                    │
//   │  -9dB ─ │    │ ║║  ← Fader + Meters  │
//   │ -12dB ─ │ ▓▓ │ ║║                    │
//   │ -24dB ─ │    │ ║║                    │
//   │ -48dB ─ │    │ ║║                    │
//   │ -60dB ─ └────┘ ║║                    │
//   │                                      │
//   │  [ -9.0 dB ]   (dB Label)            │
//   │     PAN                              │
//   │  [  >  ]        (Expand btn)         │
//   └──────────────────────────────────────┘
//
// When expanded, a side panel slides out with Effects Rack + Sends.
//
// Usage:
//   auto *channel = new SMixerChannel(parent);
//   channel->setSource(obs_source);
// ============================================================================

#include <QWidget>
#include <QLabel>
#include <QMutex>
#include <QPushButton>
#include <obs.hpp>
#include <QMenu>

#define MIXER_CHANNEL_SIDE_PANEL_WIDTH 220

namespace super {

// Forward declare components
class SMixerMeter;
class SMixerStereoMeter;
class SMixerFader;
class SMixerPanSlider;
class SMixerNameBar;
class SMixerControlBar;
class SMixerPropsSelector;
class SMixerDbLabel;
class SMixerSidePanel;
class SMixerSidebarToggle;

class SMixerChannel : public QWidget {
	Q_OBJECT

public:
	explicit SMixerChannel(QWidget *parent = nullptr);
	~SMixerChannel() override;

	// --- Source Binding ---
	void setSource(obs_source_t *source);
	OBSSource source() const { return getSource(); }
	QString sourceName() const;

	// --- Component Access ---
	SMixerStereoMeter *meter() const { return m_meter; }
	SMixerFader *fader() const { return m_fader; }
	SMixerPanSlider *panSlider() const { return m_pan_slider; }
	SMixerNameBar *nameBar() const { return m_name_bar; }
	SMixerControlBar *controlBar() const { return m_control_bar; }
	SMixerPropsSelector *busSelector() const { return m_props_selector; }
	SMixerDbLabel *dbLabel() const { return m_db_label; }
	SMixerSidePanel *sidePanel() const { return m_side_panel; }

	// --- Expand/Collapse ---
	bool isExpanded() const { return m_expanded; }
	void setExpanded(bool expanded);

signals:
	void volumeChanged(float linear_volume);
	void muteChanged(bool muted);
	void soloChanged(bool soloed);
	void panChanged(int pan);
	void channelExpanded(bool expanded);
	void sourceChanged(obs_source_t *source);

private slots:
	void toggleExpand();
	void onFaderChanged(float volume);
	void onMuteToggled(bool muted);
	void onPanChanged(int pan);
	void onDbResetRequested();

private:
	void setupUi();
	void connectSource(obs_source_t *source);
	void disconnectSource();
	void updateDbLabel();
	void startMeterTimer();

	// --- Context Menu ---
	void showChannelContextMenu(const QPoint &globalPos);
	void showColorPicker();

	// OBS callbacks (called from audio/signal threads)
	static void obsVolmeterCb(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
	                           const float peak[MAX_AUDIO_CHANNELS],
	                           const float input_peak[MAX_AUDIO_CHANNELS]);
	static void obsVolumeChangedCb(void *data, calldata_t *cd);
	static void obsMuteChangedCb(void *data, calldata_t *cd);
	static void obsRenamedCb(void *data, calldata_t *cd);
	static void obsFilterAddedCb(void *data, calldata_t *cd);
	static void obsFilterRemovedCb(void *data, calldata_t *cd);
	static void obsDestroyedCb(void *data, calldata_t *cd);

	OBSSource getSource() const {
		return OBSGetStrongRef(m_weak_source);
	}

	// OBS state
	OBSWeakSource m_weak_source;
	OBSVolMeter m_volmeter;

	// OBS Signals
	OBSSignal m_sig_volume;
	OBSSignal m_sig_mute;
	OBSSignal m_sig_rename;
	OBSSignal m_sig_filter_add;
	OBSSignal m_sig_filter_remove;
	OBSSignal m_sig_filter_reorder;
	OBSSignal m_sig_destroy;

	// Components
	SMixerNameBar *m_name_bar = nullptr;
	SMixerControlBar *m_control_bar = nullptr;
	SMixerPropsSelector *m_props_selector = nullptr;
	SMixerFader *m_fader = nullptr;
	SMixerStereoMeter *m_meter = nullptr;
	SMixerDbLabel *m_db_label = nullptr;
	QLabel *m_peak_label = nullptr;
	SMixerPanSlider *m_pan_slider = nullptr;
	SMixerSidePanel *m_side_panel = nullptr;
	QWidget *m_side_panel_sep = nullptr;
	SMixerSidebarToggle *m_expand_btn = nullptr;

	// Layout state
	bool m_expanded = false;
	bool m_updating_from_source = false;
	bool m_fader_locked = false;
	bool m_mono = false;

	// Meter data (written from audio thread, read from UI thread)
	QMutex m_meter_mutex;
	float m_peak_l = -60.0f, m_peak_r = -60.0f;
	float m_mag_l = -60.0f, m_mag_r = -60.0f;
	float m_disp_peak_l = -60.0f, m_disp_peak_r = -60.0f;
	float m_disp_mag_l = -60.0f, m_disp_mag_r = -60.0f;

	// Peak Hold
	float m_max_peak_hold = -60.0f;

	static constexpr int STRIP_WIDTH = 96;
	static constexpr int SIDE_PANEL_WIDTH = MIXER_CHANNEL_SIDE_PANEL_WIDTH;

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;
};

} // namespace super
