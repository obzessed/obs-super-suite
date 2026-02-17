#pragma once

// ============================================================================
// SMixerStereoMeter — Dual-channel vertical volume meter with labels
//
// A composite widget displaying Left/Right levels and a dB scale.
// Features:
//   - L/R segmented bars
//   - Peak hold and clip indicators
//   - dB scale labels on the right side
// ============================================================================

#include <QWidget>
#include <QColor>

namespace super {

struct MeterChannelState {
	float peak_db = -60.0f;
	float mag_db = -60.0f;
	float hold_db = -60.0f;
	int hold_frames = 0;
	bool clipping = false;
};

class SMixerStereoMeter : public QWidget {
	Q_OBJECT

public:
	explicit SMixerStereoMeter(QWidget *parent = nullptr);

	// --- Level Control ---
	void setLevels(float peakL, float magL, float peakR, float magR);
	void resetPeak();

	// --- Configuration ---
	void setMuted(bool muted);
	bool isMuted() const { return m_muted; }
	void setMono(bool mono);
	bool isMono() const { return m_mono; }
	void setDbRange(float min_db, float max_db);
	void setClipThreshold(float db);

signals:
	void clipped();

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	void paintChannel(QPainter &p, int x, int w, int h, const MeterChannelState &state);
	void updateState(MeterChannelState &state, float peak, float mag);
	float mapDb(float db) const;
	QColor segmentColor(float ratio) const;

	MeterChannelState m_left;
	MeterChannelState m_right;

	// Config
	bool m_muted = false;
	bool m_mono = false;
	float m_min_db = -60.0f;
	float m_max_db = 0.0f; // 0dB top (matches fader max)
	float m_clip_threshold = -0.5f;

	// Constants
	static constexpr int PEAK_HOLD_DURATION = 30;
	static constexpr int CLIP_LED_HEIGHT = 4;
	static constexpr int METER_TOP_MARGIN = CLIP_LED_HEIGHT + 2;

	// Colors
	static constexpr QColor COLOR_INACTIVE{0x28, 0x28, 0x28};
	static constexpr QColor COLOR_LOW{0x00, 0xE5, 0xFF};      // Bright Cyan
	static constexpr QColor COLOR_MID{0xB2, 0xEB, 0xF2};      // Light Cyan
	static constexpr QColor COLOR_HIGH{0xFF, 0xFF, 0xFF};     // White
	static constexpr QColor COLOR_CLIP{0xFF, 0x44, 0x44};     // Red
	static constexpr QColor COLOR_PEAK_HOLD{0xFF, 0xFF, 0xFF};
	static constexpr QColor COLOR_GUTTER{0x18, 0x18, 0x18};

	// Match fader scale
	static constexpr int DB_MARKS[] = {6, 3, 0, -3, -6, -9, -12, -24, -48, -60};
	static constexpr int DB_MARKS_COUNT = 10;
};

} // namespace super
