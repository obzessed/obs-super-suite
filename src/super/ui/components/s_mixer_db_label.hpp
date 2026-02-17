#pragma once

// ============================================================================
// SMixerDbLabel — dB value readout display
//
// Shows the current fader dB value (or "-inf") in a compact styled label.
// Can be clicked to reset to 0 dB if interactive.
// ============================================================================

#include <QWidget>
#include <QLabel>

namespace super {

class SMixerDbLabel : public QWidget {
	Q_OBJECT

public:
	explicit SMixerDbLabel(QWidget *parent = nullptr);

	// --- Value ---
	void setDb(float db);
	float db() const { return m_db; }

	// --- Display ---
	void setInteractive(bool interactive);

signals:
	void resetRequested();  // Emitted when label is clicked (if interactive)

protected:
	void mousePressEvent(QMouseEvent *event) override;

private:
	void setupUi();
	void updateText();

	QLabel *m_label = nullptr;
	float m_db = -INFINITY;
	bool m_interactive = true;
};

} // namespace super
