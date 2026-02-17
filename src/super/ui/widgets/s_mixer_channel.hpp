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
#include <QMutex>
#include <QPushButton>
#include <obs.hpp>

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

class SMixerChannel : public QWidget {
	Q_OBJECT

public:
	explicit SMixerChannel(QWidget *parent = nullptr);
	~SMixerChannel() override;

	// --- Source Binding ---
	void setSource(obs_source_t *source);
	obs_source_t *source() const { return m_source; }
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
	void connectSource();
	void disconnectSource();
	void updateDbLabel();
	void startMeterTimer();

	// OBS callbacks (called from audio/signal threads)
	static void obsVolmeterCb(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
	                           const float peak[MAX_AUDIO_CHANNELS],
	                           const float input_peak[MAX_AUDIO_CHANNELS]);
	static void obsVolumeChangedCb(void *data, calldata_t *cd);
	static void obsMuteChangedCb(void *data, calldata_t *cd);
	static void obsRenamedCb(void *data, calldata_t *cd);
	static void obsFilterAddedCb(void *data, calldata_t *cd);
	static void obsFilterRemovedCb(void *data, calldata_t *cd);

	// OBS state
	obs_source_t *m_source = nullptr;
	obs_volmeter_t *m_volmeter = nullptr;

	// Components
	SMixerNameBar *m_name_bar = nullptr;
	SMixerControlBar *m_control_bar = nullptr;
	SMixerPropsSelector *m_props_selector = nullptr;
	SMixerFader *m_fader = nullptr;
	SMixerStereoMeter *m_meter = nullptr;
	SMixerDbLabel *m_db_label = nullptr;
	SMixerPanSlider *m_pan_slider = nullptr;
	SMixerSidePanel *m_side_panel = nullptr;
	QWidget *m_side_panel_sep = nullptr;
	QPushButton *m_expand_btn = nullptr;

	// Layout state
	bool m_expanded = false;
	bool m_updating_from_source = false;

	// Meter data (written from audio thread, read from UI thread)
	QMutex m_meter_mutex;
	float m_peak_l = -60.0f, m_peak_r = -60.0f;
	float m_mag_l = -60.0f, m_mag_r = -60.0f;
	float m_disp_peak_l = -60.0f, m_disp_peak_r = -60.0f;
	float m_disp_mag_l = -60.0f, m_disp_mag_r = -60.0f;

	static constexpr int STRIP_WIDTH = 96;
	static constexpr int SIDE_PANEL_WIDTH = 160;
};

} // namespace super
