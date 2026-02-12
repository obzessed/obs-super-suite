#include "sourcerer_sources_dock.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QGuiApplication>
#include <obs-module.h>
#include <obs-frontend-api.h>

#define MIN_ITEM_WIDTH 60
#define MAX_ITEM_WIDTH 500
#define ZOOM_STEP 20

SourcererSourcesDock::SourcererSourcesDock(QWidget *parent) : QWidget(parent)
{
	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);

	scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	// Install event filter on scroll area (and its viewport) to catch wheel events
	scrollArea->installEventFilter(this);

	containerWidget = new QWidget();
	// parent, margin, hSpacing, vSpacing
	flowLayout = new FlowLayout(containerWidget, 4, 4, 4);

	containerWidget->setLayout(flowLayout);
	scrollArea->setWidget(containerWidget);

	mainLayout->addWidget(scrollArea);

	// Status Bar & Zoom Slider
	statusBar = new QWidget(this);
	QHBoxLayout *statusLayout = new QHBoxLayout(statusBar);
	statusLayout->setContentsMargins(4, 0, 4, 0);

	zoomSlider = new QSlider(Qt::Horizontal, this);
	zoomSlider->setRange(MIN_ITEM_WIDTH, MAX_ITEM_WIDTH);
	zoomSlider->setValue(itemWidth);
	zoomSlider->setToolTip("Zoom Source Previews");

	connect(zoomSlider, &QSlider::valueChanged, this, &SourcererSourcesDock::SetZoom);

	statusLayout->addStretch(); // Push slider to the right
	statusLayout->addWidget(zoomSlider);

	mainLayout->addWidget(statusBar);

	obs_frontend_add_event_callback(FrontendEvent, this);
}

SourcererSourcesDock::~SourcererSourcesDock()
{
	if (connectedScene) {
		signal_handler_t *sh = obs_source_get_signal_handler(connectedScene);
		signal_handler_disconnect(sh, "item_select", SceneItemSelect, this);
		signal_handler_disconnect(sh, "item_deselect", SceneItemDeselect, this);
		signal_handler_disconnect(sh, "item_visible", SceneItemVisible, this);
		signal_handler_disconnect(sh, "item_locked", SceneItemLocked, this);
		obs_source_release(connectedScene);
	}
	obs_frontend_remove_event_callback(FrontendEvent, this);
	Clear();
}

void SourcererSourcesDock::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	Refresh();
}

bool SourcererSourcesDock::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::Wheel) {
		QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
		if (wheelEvent->modifiers() & Qt::ControlModifier) {
			int delta = wheelEvent->angleDelta().y();
			if (delta > 0)
				UpdateZoom(1);
			else if (delta < 0)
				UpdateZoom(-1);
			return true; // Consume event
		}
	}
	return QWidget::eventFilter(obj, event);
}

void SourcererSourcesDock::contextMenuEvent(QContextMenuEvent *event)
{
	QMenu menu(this);
	QAction *toggleStatus = menu.addAction(tr("Show Zoom Controls"));
	toggleStatus->setCheckable(true);
	toggleStatus->setChecked(statusBar->isVisible());

	connect(toggleStatus, &QAction::toggled, statusBar, &QWidget::setVisible);

	QAction *toggleAll = menu.addAction(tr("All Sources"));
	toggleAll->setCheckable(true);
	toggleAll->setChecked(!filterByCurrentScene);
	connect(toggleAll, &QAction::toggled, [this](bool checked) {
		filterByCurrentScene = !checked;
		Refresh();
	});

	menu.exec(event->globalPos());
}

void SourcererSourcesDock::keyPressEvent(QKeyEvent *event)
{
	UpdateKeyModifiers();

	if (event->modifiers() & Qt::ControlModifier) {
		if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
			UpdateZoom(1);
			event->accept();
			return;
		} else if (event->key() == Qt::Key_Minus) {
			UpdateZoom(-1);
			event->accept();
			return;
		} else if (event->key() == Qt::Key_0) {
			ResetZoom();
			event->accept();
			return;
		}
	}
	QWidget::keyPressEvent(event);
}

void SourcererSourcesDock::keyReleaseEvent(QKeyEvent *event)
{
	UpdateKeyModifiers();
	QWidget::keyReleaseEvent(event);
}

void SourcererSourcesDock::UpdateKeyModifiers()
{
	bool ctrlPressed = (QGuiApplication::queryKeyboardModifiers() & Qt::ControlModifier);
	for (SourcererItem *item : items) {
		item->SetCtrlPressed(ctrlPressed);
	}
}

void SourcererSourcesDock::OnItemClicked(SourcererItem *item)
{
	// Local UI update
	if (selectedItem) {
		selectedItem->SetSelected(false);
	}

	if (item && item != selectedItem) {
		selectedItem = item;
		selectedItem->SetSelected(true);
	} else {
		selectedItem = item;
		if (selectedItem)
			selectedItem->SetSelected(true);
	}

	// Sync back to OBS
	if (!connectedScene || !item)
		return;

	// If we have a scene item reference, select that directly (handles duplicates)
	obs_sceneitem_t *directSceneItem = item->GetSceneItem();
	if (directSceneItem) {
		obs_scene_t *scene = obs_scene_from_source(connectedScene);
		if (scene) {
			obs_scene_enum_items(
				scene,
				[](obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
					obs_sceneitem_select(item, false);
					return true;
				},
				nullptr);

			obs_sceneitem_select(directSceneItem, true);
		}
		return;
	}

	// Fallback to source lookup (Global mode or lost reference)
	obs_source_t *source = item->GetSource();
	if (!source)
		return;

	obs_scene_t *scene = obs_scene_from_source(connectedScene);
	if (!scene)
		return;

	const char *name = obs_source_get_name(source);

	// We need to find the scene item for this source in the current scene
	obs_sceneitem_t *sceneItem = obs_scene_find_source_recursive(scene, name);

	if (sceneItem) {
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
				obs_sceneitem_select(item, false);
				return true;
			},
			nullptr);

		obs_sceneitem_select(sceneItem, true);
	}
}

void SourcererSourcesDock::OnItemDoubleClicked(SourcererItem *item)
{
	if (!item)
		return;
	obs_source_t *source = item->GetSource();
	if (source) {
		obs_frontend_open_source_properties(source);
	}
}

void SourcererSourcesDock::OnItemMenuRequested(SourcererItem *item, QMenu *menu)
{
	if (!item || !menu)
		return;

	// Check if we can offer "Show/Hide in Current Scene"
	obs_source_t *sceneSource = nullptr;
	if (obs_frontend_preview_program_mode_active()) {
		sceneSource = obs_frontend_get_current_preview_scene();
	} else {
		sceneSource = obs_frontend_get_current_scene();
	}

	if (sceneSource) {
		obs_scene_t *scene = obs_scene_from_source(sceneSource);
		obs_source_t *itemSource = item->GetSource();
		if (scene && itemSource) {
			// If we have a direct scene item, use it
			obs_sceneitem_t *sceneItem = item->GetSceneItem();

			// If not, try to find it (Global mode fallback)
			if (!sceneItem) {
				const char *name = obs_source_get_name(itemSource);
				sceneItem = obs_scene_find_source_recursive(scene, name);
			}

			if (sceneItem) {
				bool visible = obs_sceneitem_visible(sceneItem);

				QAction *visAction = new QAction(tr("Visible"), menu);
				visAction->setCheckable(true);
				visAction->setChecked(visible);

				QAction *firstAction = menu->actions().value(0);
				if (firstAction) {
					menu->insertAction(firstAction, visAction);
					menu->insertSeparator(firstAction);
				} else {
					menu->addAction(visAction);
					menu->addSeparator();
				}

				connect(visAction, &QAction::toggled,
					[sceneItem](bool checked) { obs_sceneitem_set_visible(sceneItem, checked); });
			}
		}
		obs_source_release(sceneSource);
	}
}

void SourcererSourcesDock::OnToggleVisibilityRequested(SourcererItem *item)
{
	if (!connectedScene || !item)
		return;

	// Prefer direct scene item control
	obs_sceneitem_t *directSceneItem = item->GetSceneItem();
	if (directSceneItem) {
		bool visible = obs_sceneitem_visible(directSceneItem);
		obs_sceneitem_set_visible(directSceneItem, !visible);
		return;
	}

	// Fallback
	obs_source_t *source = item->GetSource();
	if (!source)
		return;

	obs_scene_t *scene = obs_scene_from_source(connectedScene);
	if (!scene)
		return;

	const char *name = obs_source_get_name(source);
	obs_sceneitem_t *sceneItem = obs_scene_find_source_recursive(scene, name);

	if (sceneItem) {
		bool visible = obs_sceneitem_visible(sceneItem);
		obs_sceneitem_set_visible(sceneItem, !visible);
	}
}

void SourcererSourcesDock::OnToggleLockRequested(SourcererItem *item)
{
	if (!connectedScene || !item)
		return;

	// Prefer direct scene item control
	obs_sceneitem_t *directSceneItem = item->GetSceneItem();
	if (directSceneItem) {
		bool locked = obs_sceneitem_locked(directSceneItem);
		obs_sceneitem_set_locked(directSceneItem, !locked);
		return;
	}

	// Fallback
	obs_source_t *source = item->GetSource();
	if (!source)
		return;

	obs_scene_t *scene = obs_scene_from_source(connectedScene);
	if (!scene)
		return;

	const char *name = obs_source_get_name(source);
	obs_sceneitem_t *sceneItem = obs_scene_find_source_recursive(scene, name);

	if (sceneItem) {
		bool locked = obs_sceneitem_locked(sceneItem);
		obs_sceneitem_set_locked(sceneItem, !locked);
	}
}

void SourcererSourcesDock::mousePressEvent(QMouseEvent *event)
{
	// Deselect if clicking on empty space
	if (event->button() == Qt::LeftButton) {
		if (selectedItem) {
			selectedItem->SetSelected(false);
			selectedItem = nullptr;
		}

		if (connectedScene) {
			obs_scene_t *scene = obs_scene_from_source(connectedScene);
			if (scene) {
				obs_scene_enum_items(
					scene,
					[](obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
						obs_sceneitem_select(item, false);
						return true;
					},
					nullptr);
			}
		}
	}
	QWidget::mousePressEvent(event);
}

void SourcererSourcesDock::UpdateZoom(int delta_steps)
{
	int newWidth = itemWidth + (delta_steps * ZOOM_STEP);
	if (newWidth < MIN_ITEM_WIDTH)
		newWidth = MIN_ITEM_WIDTH;
	if (newWidth > MAX_ITEM_WIDTH)
		newWidth = MAX_ITEM_WIDTH;

	zoomSlider->setValue(newWidth);
}

void SourcererSourcesDock::ResetZoom()
{
	zoomSlider->setValue(160);
}

void SourcererSourcesDock::SetZoom(int width)
{
	if (width < MIN_ITEM_WIDTH)
		width = MIN_ITEM_WIDTH;
	if (width > MAX_ITEM_WIDTH)
		width = MAX_ITEM_WIDTH;

	if (itemWidth != width) {
		itemWidth = width;
		for (SourcererItem *item : items) {
			item->SetItemWidth(itemWidth);
		}

		// If called programmatically (not by slider), ensure slider is in sync
		if (zoomSlider->value() != width) {
			QSignalBlocker blocker(zoomSlider);
			zoomSlider->setValue(width);
		}
	}
}

void SourcererSourcesDock::Clear()
{
	selectedItem = nullptr;

	// Remove all items from layout and delete them
	QLayoutItem *child;
	while ((child = flowLayout->takeAt(0)) != nullptr) {
		if (child->widget()) {
			delete child->widget();
		}
		delete child;
	}
	items.clear();
}

void SourcererSourcesDock::Refresh()
{
	Clear();
	if (filterByCurrentScene) {
		obs_source_t *scene_source = nullptr;
		if (obs_frontend_preview_program_mode_active()) {
			scene_source = obs_frontend_get_current_preview_scene();
		} else {
			scene_source = obs_frontend_get_current_scene();
		}

		if (scene_source) {
			obs_scene_t *scene = obs_scene_from_source(scene_source);
			if (scene) {
				obs_scene_enum_items(scene, EnumSceneItems, this);
			}
			obs_source_release(scene_source);
		}
	} else {
		obs_enum_sources(EnumSources, this);
	}
	UpdateSceneConnection();

	// Ensure modifier state is correct after refresh
	UpdateKeyModifiers();
}

bool SourcererSourcesDock::EnumSources(void *data, obs_source_t *source)
{
	SourcererSourcesDock *dock = static_cast<SourcererSourcesDock *>(data);

	const char *id = obs_source_get_id(source);
	if (strcmp(id, "scene") == 0)
		return true; // Skip scenes

	SourcererItem *item = new SourcererItem(source);

	item->SetItemWidth(dock->itemWidth);
	// item->SetSourceActive(obs_source_enabled(source)); // Handled internally
	item->SetSceneItemVisible(true);

	// No scene context in global mode by default
	item->SetHasSceneContext(false);
	item->SetSceneItem(nullptr);

	dock->items.push_back(item);

	connect(item, &SourcererItem::Clicked, dock, &SourcererSourcesDock::OnItemClicked);
	connect(item, &SourcererItem::DoubleClicked, dock, &SourcererSourcesDock::OnItemDoubleClicked);
	connect(item, &SourcererItem::MenuRequested, dock, &SourcererSourcesDock::OnItemMenuRequested);
	connect(item, &SourcererItem::ToggleVisibilityRequested, dock,
		&SourcererSourcesDock::OnToggleVisibilityRequested);
	connect(item, &SourcererItem::ToggleLockRequested, dock, &SourcererSourcesDock::OnToggleLockRequested);

	dock->flowLayout->addWidget(item);

	return true;
}

bool SourcererSourcesDock::EnumSceneItems(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	Q_UNUSED(scene);
	SourcererSourcesDock *dock = static_cast<SourcererSourcesDock *>(param);

	obs_source_t *source = obs_sceneitem_get_source(item);
	if (!source)
		return true;

	const char *id = obs_source_get_id(source);
	if (strcmp(id, "scene") == 0)
		return true;

	SourcererItem *widget = new SourcererItem(source);

	widget->SetItemWidth(dock->itemWidth);
	widget->SetSceneItemVisible(obs_sceneitem_visible(item));
	widget->SetSceneItemLocked(obs_sceneitem_locked(item));
	widget->SetHasSceneContext(true);
	widget->SetSceneItem(item); // Crucial for distinguishing copies

	dock->items.push_back(widget);

	connect(widget, &SourcererItem::Clicked, dock, &SourcererSourcesDock::OnItemClicked);
	connect(widget, &SourcererItem::DoubleClicked, dock, &SourcererSourcesDock::OnItemDoubleClicked);
	connect(widget, &SourcererItem::MenuRequested, dock, &SourcererSourcesDock::OnItemMenuRequested);
	connect(widget, &SourcererItem::ToggleVisibilityRequested, dock,
		&SourcererSourcesDock::OnToggleVisibilityRequested);
	connect(widget, &SourcererItem::ToggleLockRequested, dock, &SourcererSourcesDock::OnToggleLockRequested);

	dock->flowLayout->addWidget(widget);

	return true;
}

void SourcererSourcesDock::FrontendEvent(enum obs_frontend_event event, void *data)
{
	SourcererSourcesDock *dock = static_cast<SourcererSourcesDock *>(data);
	// Refresh if filtering is on
	if (dock->filterByCurrentScene) {
		if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED || event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED ||
		    event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED ||
		    event == OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED) {
			dock->Refresh();
		}
	} else {
		// Even if not filtering, we might need to update the connected scene if it changed,
		// because we want to show highlights for the current scene's items.
		if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED || event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED ||
		    event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED ||
		    event == OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED) {
			dock->UpdateSceneConnection();
		}
	}
}

void SourcererSourcesDock::UpdateSceneConnection()
{
	obs_source_t *scene_source = nullptr;
	if (obs_frontend_preview_program_mode_active()) {
		scene_source = obs_frontend_get_current_preview_scene();
	} else {
		scene_source = obs_frontend_get_current_scene();
	}

	if (scene_source != connectedScene) {
		if (connectedScene) {
			signal_handler_t *sh = obs_source_get_signal_handler(connectedScene);
			signal_handler_disconnect(sh, "item_select", SceneItemSelect, this);
			signal_handler_disconnect(sh, "item_deselect", SceneItemDeselect, this);
			signal_handler_disconnect(sh, "item_visible", SceneItemVisible, this);
			signal_handler_disconnect(sh, "item_locked", SceneItemLocked, this);
			obs_source_release(connectedScene);
			connectedScene = nullptr;
		}

		if (scene_source) {
			connectedScene = scene_source;
			// We keep the reference (don't release yet, done in destructor or disconnect)

			signal_handler_t *sh = obs_source_get_signal_handler(connectedScene);
			signal_handler_connect(sh, "item_select", SceneItemSelect, this);
			signal_handler_connect(sh, "item_deselect", SceneItemDeselect, this);
			signal_handler_connect(sh, "item_visible", SceneItemVisible, this);
			signal_handler_connect(sh, "item_locked", SceneItemLocked, this);
		}
	} else {
		// Same scene, just release the temp reference we got
		if (scene_source)
			obs_source_release(scene_source);
	}

	// Always sync selection when checking connection
	SyncSelection();

	// Also sync visibility/locked state for items if we are in global mode but now have context
	// (Though normally we'd refresh if switching modes, this handles scene switch in global mode)
	// For now, SyncSelection is enough as Refresh handles the rest.
}

void SourcererSourcesDock::SyncSelection()
{
	if (!connectedScene)
		return;

	obs_scene_t *scene = obs_scene_from_source(connectedScene);
	if (!scene)
		return;

	obs_scene_enum_items(
		scene,
		[](obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
			SourcererSourcesDock *dock = static_cast<SourcererSourcesDock *>(param);
			bool selected = obs_sceneitem_selected(item);

			// Try to match by scene item pointer first
			for (SourcererItem *widget : dock->items) {
				if (widget->GetSceneItem() == item) {
					widget->SetSelected(selected);
					widget->SetSceneItemVisible(obs_sceneitem_visible(item));
					widget->SetSceneItemLocked(obs_sceneitem_locked(item));
					if (selected)
						dock->selectedItem = widget;
					return true; // Found match, done for this item
				}
			}

			// Fallback: match by source name (Global mode)
			obs_source_t *source = obs_sceneitem_get_source(item);
			if (!source)
				return true;
			const char *name = obs_source_get_name(source);

			for (SourcererItem *widget : dock->items) {
				// Only match if widget doesn't have a specific scene item attached
				// (avoid accidentally matching a specific copy to the wrong one via name)
				if (widget->GetSceneItem() == nullptr) {
					obs_source_t *wSource = widget->GetSource();
					if (wSource) {
						const char *wName = obs_source_get_name(wSource);
						if (strcmp(name, wName) == 0) {
							widget->SetSelected(selected);
							// For Global mode, we don't necessarily update visible/locked from scene
							// as it might be ambiguous. But for selection it's standard.
							if (selected)
								dock->selectedItem = widget;
						}
					}
				}
			}
			return true;
		},
		this);
}

void SourcererSourcesDock::SceneItemSelect(void *data, calldata_t *cd)
{
	SourcererSourcesDock *dock = static_cast<SourcererSourcesDock *>(data);
	obs_sceneitem_t *item = (obs_sceneitem_t *)calldata_ptr(cd, "item");
	if (!item)
		return;

	// Try direct match
	bool found = false;
	for (SourcererItem *widget : dock->items) {
		if (widget->GetSceneItem() == item) {
			widget->SetSelected(true);
			dock->selectedItem = widget;
			found = true;
		}
	}
	if (found)
		return;

	// Fallback to name match (Global mode)
	obs_source_t *source = obs_sceneitem_get_source(item);
	if (!source)
		return;

	const char *name = obs_source_get_name(source);

	// Find in dock items
	for (SourcererItem *widget : dock->items) {
		if (widget->GetSceneItem() == nullptr) {
			obs_source_t *wSource = widget->GetSource();
			if (wSource) {
				const char *wName = obs_source_get_name(wSource);
				if (strcmp(name, wName) == 0) {
					widget->SetSelected(true);
					dock->selectedItem = widget;
				}
			}
		}
	}
}

void SourcererSourcesDock::SceneItemDeselect(void *data, calldata_t *cd)
{
	SourcererSourcesDock *dock = static_cast<SourcererSourcesDock *>(data);
	obs_sceneitem_t *item = (obs_sceneitem_t *)calldata_ptr(cd, "item");
	if (!item)
		return;

	// Try direct match
	bool found = false;
	for (SourcererItem *widget : dock->items) {
		if (widget->GetSceneItem() == item) {
			widget->SetSelected(false);
			if (dock->selectedItem == widget)
				dock->selectedItem = nullptr;
			found = true;
		}
	}
	if (found)
		return;

	// Fallback
	obs_source_t *source = obs_sceneitem_get_source(item);
	if (!source)
		return;

	const char *name = obs_source_get_name(source);

	for (SourcererItem *widget : dock->items) {
		if (widget->GetSceneItem() == nullptr) {
			obs_source_t *wSource = widget->GetSource();
			if (wSource) {
				const char *wName = obs_source_get_name(wSource);
				if (strcmp(name, wName) == 0) {
					widget->SetSelected(false);
					if (dock->selectedItem == widget)
						dock->selectedItem = nullptr;
				}
			}
		}
	}
}

void SourcererSourcesDock::SceneItemVisible(void *data, calldata_t *cd)
{
	SourcererSourcesDock *dock = static_cast<SourcererSourcesDock *>(data);
	obs_sceneitem_t *item = (obs_sceneitem_t *)calldata_ptr(cd, "item");
	if (!item)
		return;

	bool visible = calldata_bool(cd, "visible");

	// Try direct match
	bool found = false;
	for (SourcererItem *widget : dock->items) {
		if (widget->GetSceneItem() == item) {
			QMetaObject::invokeMethod(
				widget, [widget, visible]() { widget->SetSceneItemVisible(visible); },
				Qt::QueuedConnection);
			found = true;
		}
	}
	// In Global mode, we generally do NOT update visibility based on scene item events
	// because it's ambiguous which item it refers to.
}

void SourcererSourcesDock::SceneItemLocked(void *data, calldata_t *cd)
{
	SourcererSourcesDock *dock = static_cast<SourcererSourcesDock *>(data);
	obs_sceneitem_t *item = (obs_sceneitem_t *)calldata_ptr(cd, "item");
	if (!item)
		return;

	bool locked = calldata_bool(cd, "locked");

	// Try direct match
	for (SourcererItem *widget : dock->items) {
		if (widget->GetSceneItem() == item) {
			QMetaObject::invokeMethod(
				widget, [widget, locked]() { widget->SetSceneItemLocked(locked); },
				Qt::QueuedConnection);
		}
	}
}

QJsonObject SourcererSourcesDock::Save() const
{
	QJsonObject obj;
	obj["itemWidth"] = itemWidth;
	obj["showZoomControls"] = statusBar->isVisible();
	obj["filterByCurrentScene"] = filterByCurrentScene;
	return obj;
}

void SourcererSourcesDock::Load(const QJsonObject &obj)
{
	if (obj.contains("itemWidth")) {
		SetZoom(obj["itemWidth"].toInt(160));
	}
	if (obj.contains("showZoomControls")) {
		statusBar->setVisible(obj["showZoomControls"].toBool(true));
	}
	if (obj.contains("filterByCurrentScene")) {
		filterByCurrentScene = obj["filterByCurrentScene"].toBool(false);
		Refresh();
	}
}
