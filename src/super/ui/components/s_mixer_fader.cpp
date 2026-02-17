#include "s_mixer_fader.hpp"

#include <QPainter>
#include <QPaintEvent>
#include <QStyleOptionSlider>
#include <cmath>
#include <algorithm>

namespace super {

// ============================================================================
// SMixerFader
// ============================================================================

SMixerFader::SMixerFader(QWidget *parent) : QSlider(Qt::Vertical, parent)
{
	setRange(0, 1000);
	setValue(800); // Unity
	setFixedWidth(50);
	
	// Create space for dB labels on left via margins?
	// QSlider logic usually fills the rect.
	// We will manually constrain the slider rect in drawing and let QSlider logic work
	// by adjusting our drawing to where we want the interaction.
	// OR better: use layout margins if QSlider respects it.
	// But QSlider paints background.
	// We will override paintEvent completely.
	
	connect(this, &QSlider::valueChanged, this, [this](int val) {
		if (!m_updating) {
			float norm = val / 1000.0f;
			float vol = norm * norm * norm;
			emit volumeChanged(vol);
			emit faderMoved(val);
		}
	});
}

void SMixerFader::setVolume(float linear_volume)
{
	m_updating = true;
	float norm = cbrtf(std::max(0.0f, linear_volume));
	int val = static_cast<int>(norm * 1000.0f);
	setValue(std::clamp(val, 0, 1000));
	m_updating = false;
}

float SMixerFader::volume() const
{
	float norm = value() / 1000.0f;
	return norm * norm * norm;
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
		// norm = 10^(-6/60) = 0.7943
		setValue(794);
	} else {
		// Go to 0dB (Unity / Top)
		setValue(1000);
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
	p.setPen(QColor(0xFF, 0xFF, 0xFF));
	int trackH = bottom - top;
	for (int i = 0; i < DB_MARKS_COUNT; i++) {
		int db = DB_MARKS[i];
		int val = (db > -60) ? static_cast<int>(1000.0f * powf(10.0f, db / 60.0f)) : 0;
		float ratio = static_cast<float>(val) / 1000.0f;
		
		int y = bottom - static_cast<int>(ratio * trackH);
		
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
