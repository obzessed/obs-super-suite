#include "s_mixer_name_bar.hpp"

#include <QVBoxLayout>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QStyleOption>

// Custom label handling elision
class SMixerNameLabel : public QLabel {
public:
	using QLabel::QLabel;
protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		QStyleOption opt;
		opt.initFrom(this);
		style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

		// Draw Elided Text
		if (!text().isEmpty()) {
			p.setPen(palette().color(QPalette::WindowText));
			QFontMetrics fm(font());
			// Subtract padding (approx 4px each side)
			int padding = 8;
			QString elided = fm.elidedText(text(), Qt::ElideRight, width() - padding);
			
			QRect textRect = rect().adjusted(4, 0, -4, 0); 
			p.drawText(textRect, alignment(), elided);
		}
	}
};

namespace super {

SMixerNameBar::SMixerNameBar(QWidget *parent) : QWidget(parent)
{
	setupUi();
}

void SMixerNameBar::setupUi()
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(6);

	// Color accent strip
	m_color_strip = new QWidget(this);
	m_color_strip->setFixedHeight(4);
	m_color_strip->setStyleSheet(
		QString("background: %1; border-radius: 2px;").arg(m_accent_color.name()));
	layout->addWidget(m_color_strip);

	// Channel name label (Custom elided label)
	m_name_label = new SMixerNameLabel("TRACK", this);
	m_name_label->setFixedHeight(24);
	m_name_label->setAlignment(Qt::AlignCenter);
	m_name_label->setStyleSheet(
		"background: #2b2b2b; color: #ddd;"
		"font-family: 'Segoe UI', sans-serif; font-size: 11px;"
		"border-radius: 3px; border: 1px solid #333;"
		"padding: 0 4px;"
	);
	layout->addWidget(m_name_label);

	// Hidden line edit for renaming
	m_name_edit = new QLineEdit(this);
	m_name_edit->setFixedHeight(24);
	m_name_edit->setAlignment(Qt::AlignCenter);
	m_name_edit->setVisible(false);
	m_name_edit->setStyleSheet(
		"background: #1a3a4a; color: #00e5ff;"
		"font-family: 'Segoe UI', sans-serif; font-size: 11px;"
		"border-radius: 3px; border: 1px solid #00e5ff;"
		"padding: 0 4px; selection-background-color: #00e5ff;"
		"selection-color: #1a1a1a;"
	);
	connect(m_name_edit, &QLineEdit::editingFinished, this, &SMixerNameBar::onEditFinished);
	layout->addWidget(m_name_edit);
}

void SMixerNameBar::setName(const QString &name)
{
	m_name_label->setText(name);
}

QString SMixerNameBar::name() const
{
	return m_name_label->text();
}

void SMixerNameBar::setAccentColor(const QColor &color)
{
	m_accent_color = color;
	m_color_strip->setStyleSheet(
		QString("background: %1; border-radius: 2px;").arg(color.name()));
}

void SMixerNameBar::setEditable(bool editable)
{
	m_editable = editable;
}

void SMixerNameBar::mouseDoubleClickEvent(QMouseEvent *event)
{
	if (m_editable) {
		startEditing();
	}
	emit doubleClicked();
	QWidget::mouseDoubleClickEvent(event);
}

void SMixerNameBar::startEditing()
{
	if (m_editing)
		return;

	m_editing = true;
	m_name_edit->setText(m_name_label->text());
	m_name_label->setVisible(false);
	m_name_edit->setVisible(true);
	m_name_edit->setFocus();
	m_name_edit->selectAll();
}

void SMixerNameBar::finishEditing()
{
	if (!m_editing)
		return;

	m_editing = false;
	QString new_name = m_name_edit->text().trimmed();

	if (!new_name.isEmpty() && new_name != m_name_label->text()) {
		m_name_label->setText(new_name);
		emit nameChanged(new_name);
	}

	m_name_edit->setVisible(false);
	m_name_label->setVisible(true);
}

void SMixerNameBar::onEditFinished()
{
	finishEditing();
}

} // namespace super
