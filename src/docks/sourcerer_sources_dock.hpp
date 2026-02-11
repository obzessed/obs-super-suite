#pragma once

#include <QWidget>
#include <QScrollArea>
#include <vector>
#include <obs.hpp>
#include "sourcerer_item.hpp"
#include "../utils/widgets/flow_layout.hpp"

#include <QSlider>

class SourcererSourcesDock : public QWidget {
	Q_OBJECT

public:
	explicit SourcererSourcesDock(QWidget *parent = nullptr);
	~SourcererSourcesDock() override;

	void Refresh();
	void UpdateZoom(int delta_steps);
	void ResetZoom();
	void SetZoom(int width);

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
	static bool EnumSources(void *data, obs_source_t *source);
	static bool EnumSceneItems(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
	static void FrontendEvent(enum obs_frontend_event event, void *data);

	bool filterByCurrentScene = true;
};
