#pragma once

#include <QWidget>
#include <QScrollArea>
#include <vector>
#include <obs.hpp>
#include <QJsonObject>
#include "sourcerer_item.hpp"
#include "../utils/widgets/flow_layout.hpp"

#include <QSlider>
#include <QRubberBand>

class SourcererSourcesDock : public QWidget {
	Q_OBJECT

public:
	explicit SourcererSourcesDock(QWidget *parent = nullptr);
	~SourcererSourcesDock() override;

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
	void keyReleaseEvent(QKeyEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;

private:
	QScrollArea *scrollArea;
	QWidget *containerWidget;
	FlowLayout *flowLayout;
	QWidget *statusBar;
	QSlider *zoomSlider;
	QRubberBand *rubberBand;
	QPoint rubberBandOrigin;
	std::vector<SourcererItem *> items;
	int itemWidth = 160;

	void Clear();
	static bool EnumSources(void *data, obs_source_t *source);
	static bool EnumSceneItems(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
	static void FrontendEvent(enum obs_frontend_event event, void *data);

	void OnItemClicked(SourcererItem *item, Qt::KeyboardModifiers modifiers);
	void OnItemDoubleClicked(SourcererItem *item);
	void OnItemMenuRequested(SourcererItem *item, QMenu *menu);
	void OnToggleVisibilityRequested(SourcererItem *item);
	void OnToggleLockRequested(SourcererItem *item);

	bool filterByCurrentScene = true;
	obs_source_t *connectedScene = nullptr;
	std::vector<obs_source_t *> monitoredScenes;
	SourcererItem *selectedItem = nullptr;

	void UpdateSceneConnection();
	void ConnectSceneSignals(obs_source_t *source);
	void DisconnectAllScenes();
	void SyncSelection();
	void ApplySelectionToOBS();
	void UpdateKeyModifiers();
	void RemoveItemsInScene(obs_scene_t *scene);

	static void SceneItemSelect(void *data, calldata_t *cd);
	static void SceneItemDeselect(void *data, calldata_t *cd);
	static void SceneItemVisible(void *data, calldata_t *cd);
	static void SceneItemLocked(void *data, calldata_t *cd);
	static void SceneItemAdd(void *data, calldata_t *cd);
	static void SceneItemRemove(void *data, calldata_t *cd);
	static void SourceCreate(void *data, calldata_t *cd);
	static void SourceRemove(void *data, calldata_t *cd);
};
