#pragma once

// ============================================================================
// SMixerControlBar — Channel control buttons (Mute / Solo / Record)
//
// Horizontal row of toggle buttons for channel state. Features:
//   - M (Mute) — red when active
//   - S (Solo) — yellow when active
//   - R (Record arm) — dot, red when active
//   - Configurable button set and custom buttons
// ============================================================================

#include <QWidget>
#include <QPushButton>

namespace super {

class SMixerControlBar : public QWidget {
	Q_OBJECT

public:
	explicit SMixerControlBar(QWidget *parent = nullptr);

	// --- State ---
	void setMuted(bool muted);
	bool isMuted() const;

	void setSoloed(bool soloed);
	bool isSoloed() const;

	void setRecordArmed(bool armed);
	bool isRecordArmed() const;

	// --- Access ---
	QPushButton *muteButton() const { return m_mute_btn; }
	QPushButton *soloButton() const { return m_solo_btn; }
	QPushButton *recordButton() const { return m_rec_btn; }

signals:
	void muteToggled(bool muted);
	void soloToggled(bool soloed);
	void recordToggled(bool armed);

private:
	void setupUi();

	QPushButton *m_mute_btn = nullptr;
	QPushButton *m_solo_btn = nullptr;
	QPushButton *m_rec_btn = nullptr;
};

} // namespace super
