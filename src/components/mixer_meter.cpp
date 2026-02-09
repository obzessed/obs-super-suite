#include "mixer_meter.hpp"
#include <QPainterPath>

MixerMeter::MixerMeter(QWidget *parent) : QWidget(parent)
{
	setFixedWidth(12); // Narrow bar
	// Set minimum height to ensure it's visible, though layout will stretch it
	setMinimumHeight(150);
}

void MixerMeter::setLevel(float levelDb)
{
	m_levelDb = levelDb;
	update();
}

void MixerMeter::setPeak(float peakDb)
{
	m_peakDb = peakDb;
	update();
}

float MixerMeter::mapDbToPos(float db)
{
	// Simple linear mapping for now, or log?
	// OBS uses a specific log scale usually.
	// Let's assume -60dB is 0.0 height (bottom), 0dB is 1.0 height (top).
	// Clamped.
	
	const float minDb = -60.0f;
	const float maxDb = 0.0f;
	
	if (db < minDb) return 0.0f;
	if (db > maxDb) return 1.0f;
	
	return (db - minDb) / (maxDb - minDb);
}

void MixerMeter::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing); // Optional for rects, but good for smooth looks
	
	// Draw Background/Track
	painter.fillRect(rect(), QColor(20, 20, 20));

	int w = width();
	int h = height();
	
	// Colors
	QColor colGreen(76, 175, 80); // Material Green
	QColor colYellow(255, 235, 59); // Material Yellow
	QColor colRed(244, 67, 54);    // Material Red

	// Draw Level
	// Gradient strategy:
	// We can't just use one linear gradient because the colors are distinct zones usually.
	// But the user image shows distinct blocks or a gradient.
	// Let's use a segmented approach for precise coloring or a multi-stop gradient.
	
	float levelPos = mapDbToPos(m_levelDb);
	int levelH = (int)(levelPos * h);
	int levelY = h - levelH;

	// Fill active part
	if (levelH > 0) {
		// We need to fill from bottom (h) to levelY
		// But the color is determined by the specific Y position (dB value)
		
		for (int y = h - 1; y >= levelY; --y) {
			float valPos = (float)(h - 1 - y) / (float)h; // 0.0 bottom, 1.0 top
			// Map pos back to dB to find color?
			// pos = (db - min) / range -> db = pos * range + min
			float currentDb = valPos * 60.0f - 60.0f;
			
			QColor c = colGreen;
			if (currentDb > -9.0f) c = colRed;
			else if (currentDb > -20.0f) c = colYellow;
			
			painter.setPen(c);
			painter.drawLine(0, y, w, y);
		}
	}

	// Draw Ticks
	painter.setPen(QColor(60, 60, 60));
	for (int db = -60; db <= 0; db += 5) {
		float pos = mapDbToPos((float)db);
		int y = h - 1 - (int)(pos * (h - 1));
		if (db == 0) y = 0; // Ensure top
		
		// Major ticks at 0, -10, -20...
		if (db % 10 == 0) {
			painter.setPen(QColor(120, 120, 120));
			painter.drawLine(0, y, w, y);
		} else {
			// Minor ticks
			painter.setPen(QColor(80, 80, 80));
			painter.drawLine(w/2, y, w, y); // Half width
		}
	}
}
