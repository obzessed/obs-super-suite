#pragma once

#include <QPushButton>

namespace super {

class SMixerSidebarToggle : public QPushButton {
	Q_OBJECT
public:
	explicit SMixerSidebarToggle(QWidget *parent = nullptr);
	void setExpanded(bool expanded);

protected:
	void paintEvent(QPaintEvent *) override;

private:
	bool m_expanded = true;
};

} // namespace super
