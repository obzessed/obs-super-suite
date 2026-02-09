#pragma once

#include <QWidget>
#include <QPaintEvent>
#include <QPainter>
#include <QLinearGradient>

class MixerMeter : public QWidget {
	Q_OBJECT

public:
	explicit MixerMeter(QWidget *parent = nullptr);

	void setLevel(float levelDb); // -inf to 0
	void setPeak(float peakDb);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	float m_levelDb = -60.0f; // Current level
	float m_peakDb = -60.0f;  // Peak hold
	
	// Helper to map dB to Y position (0.0 - 1.0)
	// Range: -60dB (bottom) to 0dB (top)
	float mapDbToPos(float db);
};
