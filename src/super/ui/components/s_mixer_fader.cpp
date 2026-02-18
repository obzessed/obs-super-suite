#include "s_mixer_fader.hpp"

#include <QPainter>
#include <QPaintEvent>
#include <QStyleOptionSlider>
#include <cmath>
#include <algorithm>

namespace super {

// ============================================================================
// Constants
// ============================================================================

static constexpr float OBS_MAX_ALLOWED_DB = 26.0f; // Advanced Audio Properties let's us adjust this value upto 26dB (i.e: 2000%)
static constexpr float OBS_MAX_ALLOWED_GAIN = 19.9526f; // 10^(26/20)

// +6dB headroom (like DAWs)
static constexpr float MAX_DB = 6.0f;
static constexpr float MAX_GAIN = 1.9953f; // 10^(6/20)

// Unity gain (0dB) slider position
// norm_unity = cbrt(1.0 / MAX_GAIN) ≈ 0.7937
static constexpr int UNITY_VALUE = 794;

// ============================================================================
// SMixerFader
// ============================================================================

SMixerFader::SMixerFader(QWidget *parent) : QSlider(Qt::Vertical, parent)
{
	setRange(0, 1000);
	setValue(UNITY_VALUE); // Unity gain (0dB)
	setFixedWidth(50);
	
	connect(this, &QSlider::valueChanged, this, [this](int val) {
		if (!m_updating) {
			float norm = val / 1000.0f;
			float vol = norm * norm * norm * MAX_GAIN;
			emit volumeChanged(vol);
			emit faderMoved(val);
		}
	});
}

void SMixerFader::setVolume(float linear_volume)
{
	m_updating = true;
	float clamped = std::max(0.0f, std::min(linear_volume, MAX_GAIN));
	float norm = cbrtf(clamped / MAX_GAIN);
	int val = static_cast<int>(norm * 1000.0f);
	setValue(std::clamp(val, 0, 1000));
	m_updating = false;
}

float SMixerFader::volume() const
{
	float norm = value() / 1000.0f;
	return norm * norm * norm * MAX_GAIN;
}

float SMixerFader::volumeDb() const
{
	float vol = volume();
	if (vol < 0.0001f) return -INFINITY;
	return 20.0f * log10f(vol);
}

void SMixerFader::setNormalized(float norm)
{
	m_updating = true;
	int val = static_cast<int>(std::clamp(norm, 0.0f, 1.0f) * 1000.0f);
	setValue(val);
	m_updating = false;
}

float SMixerFader::normalized() const
{
	return value() / 1000.0f;
}

void SMixerFader::mouseDoubleClickEvent(QMouseEvent *event)
{
	if (event->modifiers() & Qt::ControlModifier) {
		// Go to -6dB
		// norm = cbrt(10^(-6/20) / MAX_GAIN) = cbrt(0.5012 / 1.9953) ≈ 0.6310
		setValue(631);
	} else {
		// Go to 0dB (Unity)
		setValue(UNITY_VALUE);
	}
}

// ----------------------------------------------------------------------------
// Painting
// ----------------------------------------------------------------------------

void SMixerFader::paintEvent(QPaintEvent *event)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	QStyleOptionSlider opt;
	initStyleOption(&opt);

	// Geometry constants
	int w = width();
	int h = height();
	int labelWidth = 22;
	
	// Layout: [Track Area] [Labels]
	int trackAreaW = w - labelWidth;
	int trackX = trackAreaW / 2; // Center of left area
	int labelX = trackAreaW;     // Start of label area

	// Draw Frame
	p.setPen(QColor(60, 60, 60));
	p.setBrush(QColor(20, 20, 20));
	p.drawRoundedRect(0, 0, trackAreaW - 2, h - 1, 4, 4);

	// Draw Groove
	int top = 20; 
	int bottom = h - 20;
	
	p.setPen(QPen(QColor(30, 30, 30), 4, Qt::SolidLine, Qt::RoundCap));
	p.drawLine(trackX, top, trackX, bottom);
	p.setPen(QPen(QColor(0, 0, 0), 1));
	p.drawLine(trackX, top, trackX, bottom);

	// Draw Ticks & Text
	QFont f = font();
	f.setPixelSize(9);
	f.setFamily("Segoe UI");
	p.setFont(f);
	int trackH = bottom - top;
	for (int i = 0; i < DB_MARKS_COUNT; i++) {
		int db = DB_MARKS[i];
		
		// Convert dB to slider norm position
		// linear = 10^(db/20), norm = cbrt(linear / MAX_GAIN)
		float linear = (db > -60) ? powf(10.0f, db / 20.0f) : 0.0f;
		float norm = (linear > 0.0f) ? cbrtf(linear / MAX_GAIN) : 0.0f;
		
		int y = bottom - static_cast<int>(norm * trackH);
		
		// Color: Red for positive dB, white for 0dB, gray for negative
		QColor textColor;
		if (db > 0)       textColor = QColor("#ff6666");
		else if (db == 0) textColor = QColor("#ffffff");
		else              textColor = QColor("#999999");
		
		p.setPen(textColor);
		
		// Text (Align Left, towards fader)
		QString text = (db > 0) ? QString("+%1").arg(db) : QString::number(db);
		p.drawText(QRect(labelX, y - 6, labelWidth, 12), Qt::AlignLeft | Qt::AlignVCenter, text);
		
		// Tick (Left of label area)
		p.drawLine(labelX - 2, y, labelX, y);
	}

	// Draw Handle
	float norm = value() / 1000.0f;
	int centerY = bottom - static_cast<int>(norm * trackH);
	
	int handleW = 28;
	int handleH = 36;
	QRect r(trackX - handleW/2, centerY - handleH/2, handleW, handleH);

	// Handle Visuals
	// Use SVG asset
	static const QIcon handleIcon(":/super/assets/icons/super/mixer/fader-handle.svg");
	handleIcon.paint(&p, r, Qt::AlignCenter);
}

} // namespace super
