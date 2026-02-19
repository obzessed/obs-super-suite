#include "s_mixer_db_label.hpp"

#include <QVBoxLayout>
#include <QMouseEvent>
#include <cmath>

namespace super {

SMixerDbLabel::SMixerDbLabel(QWidget *parent) : QWidget(parent)
{
	setupUi();
}

void SMixerDbLabel::setupUi()
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	m_label = new QLabel("-\xe2\x88\x9e", this);
	m_label->setAlignment(Qt::AlignCenter);
	m_label->setFixedHeight(18);
	m_label->setCursor(Qt::PointingHandCursor);
	m_label->setToolTip("Click to reset the fader");
	m_label->setStyleSheet(
		"color: #aaa; font-size: 10px; font-weight: bold;"
		"background: #2b2b2b; border-radius: 2px;"
		"font-family: 'Segoe UI', sans-serif;"
		"border: 1px solid #333;"
	);

	layout->addWidget(m_label);
}

void SMixerDbLabel::setDb(float db)
{
	m_db = db;
	updateText();
}

void SMixerDbLabel::updateText()
{
	if (m_db <= -99.0f || std::isinf(m_db)) {
		m_label->setText("-\xe2\x88\x9e");
		m_label->setStyleSheet(
			"color: #666; font-size: 10px; font-weight: bold;"
			"background: #2b2b2b; border-radius: 2px;"
			"font-family: 'Segoe UI', sans-serif;"
			"border: 1px solid #333;"
		);
	} else {
		float display_db = m_db;
		if (display_db > -0.05f && display_db < 0.0f) display_db = 0.0f;
		m_label->setText(QString::asprintf("%.1f", display_db));

		// Green tint for normal, orange for hot, red for clipping
		QString color = "#00ff00"; // Default Green
		if (m_db >= -0.05f) // Near zero or positive
			color = "#ff4444";
		else if (m_db >= -5.0f)
			color = "#ffaa00";

		m_label->setStyleSheet(QString(
			"color: %1; font-size: 10px; font-weight: bold;"
			"background: #2b2b2b; border-radius: 2px;"
			"font-family: 'Segoe UI', sans-serif;"
			"border: 1px solid #333;"
		).arg(color));
	}
}

void SMixerDbLabel::setInteractive(bool interactive)
{
	m_interactive = interactive;
	setCursor(interactive ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void SMixerDbLabel::mousePressEvent(QMouseEvent *event)
{
	if (m_interactive && event->button() == Qt::LeftButton) {
		emit resetRequested();
	}
	QWidget::mousePressEvent(event);
}

} // namespace super
