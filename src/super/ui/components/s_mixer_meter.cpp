#include "s_mixer_meter.hpp"

#include <QPainter>
#include <QPaintEvent>
#include <cmath>
#include <algorithm>

namespace super {

SMixerStereoMeter::SMixerStereoMeter(QWidget *parent) : QWidget(parent)
{
	setFixedWidth(44); // 22(Labels) + 4 + 6(L) + 2 + 6(R) + 4(Pad)
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void SMixerStereoMeter::setLevels(float pl, float ml, float pr, float mr)
{
	bool was_clipping = m_left.clipping || m_right.clipping;

	updateState(m_left, pl, ml);
	updateState(m_right, pr, mr);

	if (!was_clipping && (m_left.clipping || m_right.clipping)) {
		emit clipped();
	}
	update();
}

void SMixerStereoMeter::updateState(MeterChannelState &s, float peak, float mag)
{
	s.peak_db = peak;
	s.mag_db = mag;

	// Peak Hold
	if (peak > s.hold_db) {
		s.hold_db = peak;
		s.hold_frames = 0;
	} else {
		s.hold_frames++;
		if (s.hold_frames > PEAK_HOLD_DURATION) {
			s.hold_db -= 0.5f;
			if (s.hold_db < m_min_db) s.hold_db = m_min_db;
		}
	}

	// Clip
	if (peak >= m_clip_threshold) s.clipping = true;
}

void SMixerStereoMeter::resetPeak()
{
	m_left.hold_db = m_min_db;
	m_left.clipping = false;
	m_right.hold_db = m_min_db;
	m_right.clipping = false;
	update();
}

void SMixerStereoMeter::setMuted(bool muted)
{
	if (m_muted != muted) {
		m_muted = muted;
		update();
	}
}

void SMixerStereoMeter::setMono(bool mono)
{
	if (m_mono != mono) {
		m_mono = mono;
		update();
	}
}

void SMixerStereoMeter::setDbRange(float min_db, float max_db)
{
	m_min_db = min_db;
	m_max_db = max_db;
	update();
}

void SMixerStereoMeter::setClipThreshold(float db)
{
	m_clip_threshold = db;
}

float SMixerStereoMeter::mapDb(float db) const
{
	// Cubic mapping to match fader
	if (db <= -60.0f) return 0.0f;
	if (db >= 0.0f) return 1.0f;
	return powf(10.0f, db / 60.0f);
}

QColor SMixerStereoMeter::segmentColor(float ratio) const
{
	// Gradient: Cyan (#00FFFF) -> Yellow (#FFFF00) -> Orange (#FF4400) -> Red (#FF0000)
	QColor c1, c2;
	float t;

	if (ratio < 0.6f) {
		c1 = QColor(0, 255, 255);
		c2 = QColor(255, 255, 0);
		t = ratio / 0.6f;
	} else if (ratio < 0.85f) {
		c1 = QColor(255, 255, 0);
		c2 = QColor(255, 68, 0);
		t = (ratio - 0.6f) / 0.25f;
	} else {
		c1 = QColor(255, 68, 0);
		c2 = QColor(255, 0, 0);
		t = (ratio - 0.85f) / 0.15f;
	}

	int r = c1.red() + static_cast<int>((c2.red() - c1.red()) * t);
	int g = c1.green() + static_cast<int>((c2.green() - c1.green()) * t);
	int b = c1.blue() + static_cast<int>((c2.blue() - c1.blue()) * t);

	QColor finalColor(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));

	if (m_muted) {
		// Convert to grayscale
		return finalColor.toHsv().value() < 30 ? finalColor : finalColor.toHsv().value() > 0 ? QColor::fromHsl(0, 0, finalColor.lightness()).darker(150) : finalColor;
		// Better grayscale: Luma
		int gray = qGray(finalColor.rgb());
		return QColor(gray, gray, gray).darker(120); 
	}
	return finalColor;
}

void SMixerStereoMeter::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	int w = width();
	int h = height();

	// Frame (Background)
	p.setPen(QColor(60, 60, 60));
	p.setBrush(QColor(20, 20, 20));
	p.drawRoundedRect(0, 0, w - 1, h - 1, 4, 4);

	// Geometry: Labels (Left), Meters (Right)
	int labelW = 22;
	int gap = 4;
	int barW = 6;
	int barGap = 2;

	int xLabels = 0;
	int xL = xLabels + labelW + gap;
	int xR = xL + barW + barGap;

	// Draw Channels (Top/Bottom padding)
	p.save();
	p.translate(0, 4);
	if (m_mono) {
		// Single bar spanning L+R width (14px)
		int monoX = xL;
		int monoW = barW * 2 + barGap;
		// Use Left channel data for mono (or max of both? OBS usually duplicates mono to L/R in callbacks)
		// We'll use L since that's where mono usually lives or duplicated.
		paintChannel(p, monoX, monoW, h - 18, m_left);
	} else {
		paintChannel(p, xL, barW, h - 18, m_left);
		paintChannel(p, xR, barW, h - 18, m_right);
	}
	p.restore();

	// Draw Labels (Left side)
	QFont f = font();
	f.setPixelSize(9);
	f.setFamily("Segoe UI");
	p.setFont(f);
	p.setPen(QColor(0xFF, 0xFF, 0xFF));

	int top = METER_TOP_MARGIN + 4;
	int bottom = h - 14;
	int trackH = bottom - top;

	for (int i = 0; i < DB_MARKS_COUNT; i++) {
		int db = DB_MARKS[i];
		// Skip marks above 0dB if max is 0
		if (db > 0 && m_max_db <= 0) continue;

		float ratio = 0.0f;
		if (db > -60) {
			float val = powf(10.0f, db / 60.0f);
			if (db == 0) val = 1.0f; // Exact
			ratio = val;
		}

		int y = bottom - static_cast<int>(ratio * trackH);
		
		// Align text right (next to meters)
		QString text = QString::number(db);
		p.drawText(QRect(xLabels, y - 6, labelW, 12), Qt::AlignRight | Qt::AlignVCenter, text);
		
		// Tick
		p.drawLine(xLabels + labelW, y, xLabels + labelW + 2, y);
	}
}

void SMixerStereoMeter::paintChannel(QPainter &p, int x, int w, int h, const MeterChannelState &state)
{
	// Clip LED at top
	QRect clipRect(x, 0, w, CLIP_LED_HEIGHT);
	p.fillRect(clipRect, state.clipping ? COLOR_CLIP : COLOR_INACTIVE.darker());

	// Bar area
	int barTop = METER_TOP_MARGIN;
	int barH = h - barTop;
	
	// Map dB (cube taper match? No, meters usually log or linear dB.
	// Fader is cubic volume.
	// But standard meters usually show linear dB scale.
	// If I use mapDb (linear dB mapping), -6dB is at 90% (linear)? No.
	// -6dB is 0.5 amplitude.
	// mapDb maps -60 to 0dB linearly?
	// `(db - min) / (max - min)`.
	// -6dB in range -60..0 is (-6 - -60) / 60 = 54/60 = 0.9.
	// 0.9 height.
	// This matches visual expectation (-6dB near top).
	
	float ratio = mapDb(state.peak_db);
	int activeH = static_cast<int>(ratio * barH);
	int filledY = h - activeH;
	
	// Draw full bar (gradient or segments)
	// We'll draw segments
	int segH = 2;
	int segGap = 1;
	int step = segH + segGap;

	for (int yInv = 0; yInv < barH; yInv += step) {
		int y = h - yInv - segH;
		if (y < barTop) break;

		float segRatio = static_cast<float>(yInv) / static_cast<float>(barH);
		bool active = (yInv < activeH);
		
		QColor c = active ? segmentColor(segRatio) : COLOR_INACTIVE;
		p.fillRect(x, y, w, segH, c);
	}

	// Peak Hold
	float holdRatio = mapDb(state.hold_db);
	if (holdRatio > 0.0f) {
		int holdY = h - static_cast<int>(holdRatio * barH) - 1;
		if (holdY >= barTop) {
			p.fillRect(x, holdY, w, 1, COLOR_PEAK_HOLD);
		}
	}
}

} // namespace super
