#pragma once

// ============================================================================
// SMixerPanSlider — Horizontal pan/balance control
//
// A compact horizontal slider for stereo panning. Features:
//   - Range: -100 (full left) to +100 (full right), 0 = center
//   - Center detent (snaps to center near midpoint)
//   - "PAN" label below
//   - Double-click to reset to center
// ============================================================================

#include <QWidget>
#include <QSlider>
#include <QLabel>

namespace super {

class SMixerPanSlider : public QWidget {
	Q_OBJECT

public:
	explicit SMixerPanSlider(QWidget *parent = nullptr);

	// --- Pan Control ---
	void setPan(int value);       // -100 to +100
	int pan() const;

	void setBalance(float bal);   // 0.0 (left) to 1.0 (right)
	float balance() const;

	// --- Configuration ---
	void setShowLabel(bool show);
	void setCenterDetent(bool enable);

	QSlider *slider() const { return m_slider; }

signals:
	void panChanged(int value);
	void balanceChanged(float balance);

private slots:
	void onSliderChanged(int value);

private:
	void setupUi();

	QSlider *m_slider = nullptr;
	QLabel *m_label = nullptr;

	bool m_center_detent = true;
	bool m_updating = false;

	static constexpr int DETENT_RANGE = 5; // Snap to center within ±5
protected:
	bool eventFilter(QObject *obj, QEvent *event) override;
};

} // namespace super
