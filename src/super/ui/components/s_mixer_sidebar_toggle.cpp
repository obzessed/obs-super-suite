#include "s_mixer_sidebar_toggle.hpp"
#include <QPainter>
#include <QIcon>

namespace super {

SMixerSidebarToggle::SMixerSidebarToggle(QWidget *parent) : QPushButton(parent) {
	setFixedSize(22, 22);
	setCursor(Qt::PointingHandCursor);
	setToolTip("Collapse");
	setStyleSheet("border: none; background: transparent; padding: 0px; margin: 0px; min-height: 0px;");
}

void SMixerSidebarToggle::setExpanded(bool expanded) {
	m_expanded = expanded;
	setToolTip(expanded ? "Collapse" : "Expand");
	update();
}

void SMixerSidebarToggle::paintEvent(QPaintEvent *) {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	p.setRenderHint(QPainter::SmoothPixmapTransform);

	QColor color = QColor("#888");
	if (underMouse()) color = QColor("#fff");
	if (isDown()) color = QColor("#00cccc");

	QString iconPath = m_expanded ? ":/super/assets/icons/super/mixer/sidebar-right-collapse.svg" 
	                              : ":/super/assets/icons/super/mixer/sidebar-right-expand.svg";

	QIcon icon(iconPath);
	int dim = 16;
	int x = (width() - dim) / 2;
	int y = (height() - dim) / 2;

	if (!icon.isNull()) {
		QPixmap pix(dim, dim);
		pix.fill(Qt::transparent);
		QPainter ip(&pix);
		ip.setRenderHint(QPainter::Antialiasing);
		icon.paint(&ip, pix.rect(), Qt::AlignCenter);
		ip.setCompositionMode(QPainter::CompositionMode_SourceIn);
		ip.fillRect(pix.rect(), color);
		ip.end();
		p.drawPixmap(x, y, pix);
	} else {
		p.setPen(color);
		p.drawText(rect(), Qt::AlignCenter, m_expanded ? ">" : "<");
	}
}

} // namespace super
