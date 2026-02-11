#pragma once

#include <QWidget>
#include <QLabel>
#include <obs.hpp>
#include "../utils/widgets/qt-display.hpp"

class SourcererItem : public QWidget {
	Q_OBJECT

public:
	explicit SourcererItem(obs_source_t *source, QWidget *parent = nullptr);
	~SourcererItem();

	void UpdateName();
	void SetItemWidth(int width);
	obs_source_t *GetSource() const { return source; }
	void SetSelected(bool selected);
	void SetProgram(bool program);

signals:
	void Clicked(SourcererItem *item);
	void DoubleClicked(SourcererItem *item);

protected:
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
	obs_source_t *source = nullptr;
	OBSQTDisplay *display = nullptr;
	QLabel *label = nullptr;
	bool isSelected = false;
	bool isProgram = false;

	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);
};
