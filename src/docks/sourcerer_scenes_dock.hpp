#pragma once

#include <QWidget>
#include <QScrollArea>
#include <vector>
#include <obs.hpp>
#include "sourcerer_item.hpp"
#include "../utils/widgets/flow_layout.hpp"

#include <QSlider>

class SourcererScenesDock : public QWidget {
	Q_OBJECT

public:
	explicit SourcererScenesDock(QWidget *parent = nullptr);
	~SourcererScenesDock() override;

	void Refresh();
	void UpdateZoom(int delta_steps);
	void ResetZoom();
	void SetZoom(int width);
	QJsonObject Save() const;
	void Load(const QJsonObject &obj);

protected:
	void showEvent(QShowEvent *event) override;
	bool eventFilter(QObject *obj, QEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;

private:
	QScrollArea *scrollArea;
	QWidget *containerWidget;
	FlowLayout *flowLayout;
	QWidget *statusBar;
	QSlider *zoomSlider;
	std::vector<SourcererItem *> items;
	int itemWidth = 160;

	void Clear();

	bool syncWithMain = true;
	bool isReadOnly = false;
	bool doubleClickToProgram = true;
	void HighlightCurrentScene() const;
	static void FrontendEvent(enum obs_frontend_event event, void *data);

	void OnItemClicked(SourcererItem *item);
	void OnItemDoubleClicked(SourcererItem *item);
};
