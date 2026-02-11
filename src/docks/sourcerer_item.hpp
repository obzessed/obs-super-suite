#pragma once

#include <QWidget>
#include <QLabel>
#include <obs.hpp>
#include "../utils/widgets/qt-display.hpp"

class QMenu;
class QContextMenuEvent;

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
	void SetSceneItemVisible(bool visible);
	void UpdateStatus();

signals:
	void Clicked(SourcererItem *item);
	void DoubleClicked(SourcererItem *item);
	void MenuRequested(SourcererItem *item, QMenu *menu);

protected:
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;

private:
	obs_source_t *source = nullptr;
	OBSQTDisplay *display = nullptr;
	QLabel *label = nullptr;
	bool isSelected = false;
	bool isProgram = false;
	bool isSceneItemVisible = true;
	bool isSourceEnabled = true;

	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);

	static void SourceRenamed(void *data, calldata_t *cd);
	static void SourceEnabled(void *data, calldata_t *cd);
	static void SourceDisabled(void *data, calldata_t *cd);
};
