#include "s_mixer_control_bar.hpp"

#include <QHBoxLayout>
#include <QPainter>
#include <QIcon>
#include <QPixmap>

namespace super {

class SMixerIconButton : public QPushButton {
public:
	SMixerIconButton(const QString &iconPath, const QColor &activeColor, QWidget *parent = nullptr)
		: QPushButton(parent), m_iconPath(iconPath), m_activeColor(activeColor)
	{
		setFixedSize(24, 24);
		setCheckable(true);
		
		setStyleSheet(QString(
			"QPushButton {"
			"  background: #2b2b2b; border: 1px solid #333;"
			"  border-radius: 4px;"
			"  min-width: 22px; max-width: 22px;"
			"  min-height: 22px; max-height: 22px;"
			"  margin: 0px; padding: 0px;"
			"}"
			"QPushButton:hover { background: #333; border: 1px solid #555; }"
			"QPushButton:checked { background: #2b2b2b; border: 1px solid %1; }"
			"QPushButton:checked:hover { border: 1px solid %1; }"
		).arg(m_activeColor.name()));
	}

protected:
	void paintEvent(QPaintEvent *e) override
	{
		// Draw background/border via stylesheet
		QPushButton::paintEvent(e);

		QPainter p(this);
		p.setRenderHint(QPainter::SmoothPixmapTransform);
		p.setRenderHint(QPainter::Antialiasing);

		// Determine Icon Color
		QColor iconColor = isChecked() ? m_activeColor : QColor("#888");
		if (!isChecked() && isDown()) iconColor = iconColor.lighter(120);
		if (!isEnabled()) iconColor = QColor("#555");
		
		// Icon geometry
		int dim = 14; 
		int x = (width() - dim) / 2;
		int y = (height() - dim) / 2;
		QRect iconRect(x, y, dim, dim);

		QIcon icon(m_iconPath);
		if (!icon.isNull()) {
			QPixmap pix(dim, dim);
			pix.fill(Qt::transparent);
			
			QPainter iconPainter(&pix);
			iconPainter.setRenderHint(QPainter::Antialiasing);
			iconPainter.setRenderHint(QPainter::SmoothPixmapTransform);
			icon.paint(&iconPainter, pix.rect(), Qt::AlignCenter);
			
			// Tint using SourceIn
			iconPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
			iconPainter.fillRect(pix.rect(), iconColor);
			iconPainter.end();

			p.drawPixmap(iconRect, pix);
		}
	}

private:
	QString m_iconPath;
	QColor m_activeColor;
};

// ============================================================================

SMixerControlBar::SMixerControlBar(QWidget *parent) : QWidget(parent)
{
	setupUi();
}

void SMixerControlBar::setupUi()
{
	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(2);

	// Mute — Red
	// Path: :/super/assets/icons/super/mixer/control-mute.svg
	m_mute_btn = new SMixerIconButton(":/super/assets/icons/super/mixer/control-mute.svg", QColor("#ff4c4c"), this);
	connect(m_mute_btn, &QPushButton::toggled, this, &SMixerControlBar::muteToggled);
	layout->addWidget(m_mute_btn);

	layout->addStretch();

	// Solo — Yellow
	// Path: :/super/assets/icons/super/mixer/control-monitor.svg
	m_solo_btn = new SMixerIconButton(":/super/assets/icons/super/mixer/control-monitor.svg", QColor("#ffcc00"), this);
	connect(m_solo_btn, &QPushButton::toggled, this, &SMixerControlBar::soloToggled);
	layout->addWidget(m_solo_btn);

	layout->addStretch();

	// Record — Red
	// Path: :/super/assets/icons/super/mixer/control-record.svg
	m_rec_btn = new SMixerIconButton(":/super/assets/icons/super/mixer/control-record.svg", QColor("#ff0000"), this);
	connect(m_rec_btn, &QPushButton::toggled, this, &SMixerControlBar::recordToggled);
	layout->addWidget(m_rec_btn);
}

void SMixerControlBar::setMuted(bool muted)
{
	m_mute_btn->setChecked(muted);
}

bool SMixerControlBar::isMuted() const
{
	return m_mute_btn->isChecked();
}

void SMixerControlBar::setSoloed(bool soloed)
{
	m_solo_btn->setChecked(soloed);
}

bool SMixerControlBar::isSoloed() const
{
	return m_solo_btn->isChecked();
}

void SMixerControlBar::setRecordArmed(bool armed)
{
	m_rec_btn->setChecked(armed);
}

bool SMixerControlBar::isRecordArmed() const
{
	return m_rec_btn->isChecked();
}

} // namespace super
