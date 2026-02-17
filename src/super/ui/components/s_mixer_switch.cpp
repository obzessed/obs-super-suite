#include "s_mixer_switch.hpp"

#include <QPainter>
#include <QMouseEvent>
#include <QPropertyAnimation>

namespace super {

SMixerSwitch::SMixerSwitch(QWidget *parent) : QWidget(parent)
{
	setFixedSize(30, 16);
	setCursor(Qt::PointingHandCursor);
}

void SMixerSwitch::setChecked(bool checked, bool animate)
{
	if (m_checked == checked) return;
	m_checked = checked;

	// If hidden or explicit no-anim, skip
	if (!animate || !isVisible()) {
		m_position = m_checked ? 1.0f : 0.0f;
		emit toggled(m_checked);
		update();
		return;
	}

	auto *anim = new QPropertyAnimation(this, "position", this);
	anim->setDuration(150);
	anim->setStartValue(m_position);
	anim->setEndValue(m_checked ? 1.0f : 0.0f);
	anim->setEasingCurve(QEasingCurve::OutQuad);
	anim->start(QAbstractAnimation::DeleteWhenStopped);

	emit toggled(m_checked);
}

void SMixerSwitch::setPosition(float pos)
{
	m_position = pos;
	update();
}

void SMixerSwitch::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	int w = width();
	int h = height();
	int margin = 2;

	QColor activeColor("#00e5ff");
	QColor inactiveColor("#444");

	// Interpolate color
	QColor trackColor;
	if (m_position <= 0.0f) trackColor = inactiveColor;
	else if (m_position >= 1.0f) trackColor = activeColor.darker(150);
	else {
		// Blend
		int r = inactiveColor.red() + (activeColor.darker(150).red() - inactiveColor.red()) * m_position;
		int g = inactiveColor.green() + (activeColor.darker(150).green() - inactiveColor.green()) * m_position;
		int b = inactiveColor.blue() + (activeColor.darker(150).blue() - inactiveColor.blue()) * m_position;
		trackColor = QColor(r, g, b);
	}

	QRectF rect(0, 0, w, h);
	p.setPen(Qt::NoPen);
	p.setBrush(trackColor);
	p.drawRoundedRect(rect, h/2.0, h/2.0);

	// Handle
	float handleSize = h - 2.0*margin;
	float range = w - 2.0*margin - handleSize;
	float x = margin + range * m_position;

	QRectF handleRect(x, margin, handleSize, handleSize);
	p.setBrush(Qt::white);
	p.drawEllipse(handleRect);
}

void SMixerSwitch::mousePressEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton) {
		setChecked(!m_checked, true);
	}
}

} // namespace super
