#pragma once

// ============================================================================
// SMixerSendsPanel — Track routing / sends / audio mixer assignment panel
//
// Shows which OBS audio mixer tracks (1-6) the source is routed to.
// Features:
//   - Switch toggle per track (Track 1..6)
//   - Reflects obs_source_get/set_audio_mixers bitmask
//   - Compact layout for side panel use
//   - Reacts to external mixer changes
// ============================================================================

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <vector>
#include <obs.hpp>
#include "s_mixer_switch.hpp"

namespace super {

class SMixerSendsPanel : public QWidget {
	Q_OBJECT

public:
	explicit SMixerSendsPanel(QWidget *parent = nullptr);
	~SMixerSendsPanel() override;

	// --- Source Binding ---
	void setSource(obs_source_t *source);
	void refresh();

	// --- Configuration ---
	void setTrackCount(int count);
	int trackCount() const { return m_track_count; }
	
	// --- Collapse ---
	void setExpanded(bool expanded);

signals:
	void trackChanged(int track_index, bool enabled);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;


private:
	void setupUi();
	void clearItems();
	void connectSource();
	void disconnectSource();
	void updateSwitches(uint32_t mixers);

	// Static callback for OBS signal
	static void audioMixersChangedCb(void *data, calldata_t *cd);

	QWidget *m_content_container = nullptr;
	QLabel *m_header_label = nullptr;
	QPushButton *m_collapse_btn = nullptr;
	QVBoxLayout *m_items_layout = nullptr;

	obs_source_t *m_source = nullptr;
	signal_handler_t *m_signal_handler = nullptr; // Cached handler
	int m_track_count = 6;
	bool m_is_expanded = true;

	
	std::vector<SMixerSwitch*> m_switches;
};

} // namespace super
