#pragma once

#include <QWidget>

namespace super {

class SMixerSwitch : public QWidget {
	Q_OBJECT
	Q_PROPERTY(float position READ position WRITE setPosition)

public:
	explicit SMixerSwitch(QWidget *parent = nullptr);

	bool isChecked() const { return m_checked; }
	// animate=true by default to support interactive toggling.
	// set to false for programmatic initialization.
	void setChecked(bool checked, bool animate = true);

	float position() const { return m_position; }
	void setPosition(float pos);

signals:
	void toggled(bool checked);

protected:
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *e) override;

private:
	bool m_checked = false;
	float m_position = 0.0f;
};

} // namespace super
