#include "s_mixer_pan_slider.hpp"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QEvent>
#include <QLabel>
#include <QToolTip>

namespace super {

SMixerPanSlider::SMixerPanSlider(QWidget *parent) : QWidget(parent)
{
	setupUi();
}

void SMixerPanSlider::setupUi()
{
	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);

	// L Label
	auto *lblL = new QLabel("L", this);
	lblL->setStyleSheet("color: #666; font-weight: bold; font-size: 10px; font-family: 'Segoe UI', sans-serif;");
	layout->addWidget(lblL);

	m_slider = new QSlider(Qt::Horizontal, this);
	m_slider->setRange(-100, 100);
	m_slider->setValue(0);
	m_slider->setFixedHeight(16);
	m_slider->setCursor(Qt::PointingHandCursor);
	m_slider->setAttribute(Qt::WA_Hover);
	m_slider->installEventFilter(this);

	m_slider->setStyleSheet(
		"QSlider::groove:horizontal {"
		"  height: 4px; background: #111; border-radius: 2px;"
		"}"
		"QSlider::handle:horizontal {"
		"  background: #888; width: 10px; margin: -3px 0; border-radius: 2px;"
		"}"
		"QSlider::handle:horizontal:hover { background: #aaa; }"
		"QSlider::sub-page:horizontal { background: #111; border-radius: 2px; }"
		"QSlider::add-page:horizontal { background: #111; border-radius: 2px; }"
	);

	connect(m_slider, &QSlider::valueChanged, this, &SMixerPanSlider::onSliderChanged);
	layout->addWidget(m_slider);

	// R Label
	auto *lblR = new QLabel("R", this);
	lblR->setStyleSheet("color: #666; font-weight: bold; font-size: 10px; font-family: 'Segoe UI', sans-serif;");
	layout->addWidget(lblR);

	// Keep m_label for compatibility but hide it
	m_label = new QLabel(this);
	m_label->setVisible(false);
}

bool SMixerPanSlider::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == m_slider) {
		if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverMove ||
		    event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove) {
			
			int val = m_slider->value();
			QString text = (val == 0) ? "Center" : QString("%1%").arg(val);
			QToolTip::showText(QCursor::pos(), text, m_slider);
		}
		if (event->type() == QEvent::MouseButtonDblClick) {
			m_slider->setValue(0);
			return true;
		}
	}
	return QWidget::eventFilter(obj, event);
}

void SMixerPanSlider::setPan(int value)
{
	m_updating = true;
	m_slider->setValue(std::clamp(value, -100, 100));
	m_updating = false;
}

int SMixerPanSlider::pan() const
{
	return m_slider->value();
}

void SMixerPanSlider::setBalance(float bal)
{
	setPan(static_cast<int>((bal * 200.0f) - 100.0f));
}

float SMixerPanSlider::balance() const
{
	return (static_cast<float>(m_slider->value()) + 100.0f) / 200.0f;
}

void SMixerPanSlider::setShowLabel(bool show)
{
	m_label->setVisible(show);
}

void SMixerPanSlider::setCenterDetent(bool enable)
{
	m_center_detent = enable;
}

void SMixerPanSlider::onSliderChanged(int value)
{
	// Center detent: snap to 0 when close
	if (m_center_detent && !m_updating) {
		if (value >= -DETENT_RANGE && value <= DETENT_RANGE && value != 0) {
			m_updating = true;
			m_slider->setValue(0);
			m_updating = false;
			value = 0;
		}
	}

	if (!m_updating) {
		emit panChanged(value);
		emit balanceChanged(balance());
	}
}

} // namespace super
