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
#include <QVariantAnimation>
#include <obs.hpp>
#include "s_mixer_switch.hpp"

namespace super {

// Chevron button that rotates 180 degrees when toggled, with smooth animation
class SMixerChevron : public QPushButton {
	Q_OBJECT
public:
	explicit SMixerChevron(QWidget *parent = nullptr);
	void setExpanded(bool expanded);

protected:
	void paintEvent(QPaintEvent *) override;

private:
	void animateTo(qreal endAngle);

	qreal m_angle = 0.0;
	QVariantAnimation *m_anim = nullptr;
};

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
	SMixerChevron *m_collapse_btn = nullptr;
	QVBoxLayout *m_items_layout = nullptr;

	obs_source_t *getSource() const;
	static void sourceDestroyedCb(void *data, calldata_t *cd);

	obs_weak_source_t *m_weak_source = nullptr;
	int m_track_count = 6;
	bool m_is_expanded = true;

	
	std::vector<SMixerSwitch*> m_switches;
};

} // namespace super
