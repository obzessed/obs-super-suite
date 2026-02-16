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

	signal_handler_t *sh = obs_get_signal_handler();
	signal_handler_connect(sh, "source_create", SourceCreate, this);
	signal_handler_connect(sh, "source_remove", SourceRemove, this);

	rubberBand = new QRubberBand(QRubberBand::Rectangle, containerWidget);
	containerWidget->installEventFilter(this);
}

SourcererSourcesDock::~SourcererSourcesDock()
{
	if (connectedScene) {
		signal_handler_t *sh = obs_source_get_signal_handler(connectedScene);
		signal_handler_disconnect(sh, "item_select", SceneItemSelect, this);
		signal_handler_disconnect(sh, "item_deselect", SceneItemDeselect, this);
		signal_handler_disconnect(sh, "item_visible", SceneItemVisible, this);
		signal_handler_disconnect(sh, "item_locked", SceneItemLocked, this);
		signal_handler_disconnect(sh, "item_add", SceneItemAdd, this);
		signal_handler_disconnect(sh, "item_remove", SceneItemRemove, this);
		signal_handler_disconnect(sh, "reorder", SceneItemReorder, this);
		obs_source_release(connectedScene);
	}
	DisconnectAllScenes();
	obs_frontend_remove_event_callback(FrontendEvent, this);

	signal_handler_t *sh = obs_get_signal_handler();
	signal_handler_disconnect(sh, "source_create", SourceCreate, this);
	signal_handler_disconnect(sh, "source_remove", SourceRemove, this);

	Clear();
}

void SourcererSourcesDock::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	Refresh();
}

bool SourcererSourcesDock::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == containerWidget) {
		if (event->type() == QEvent::MouseButtonPress) {
			QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
			if (mouseEvent->button() == Qt::LeftButton) {
				rubberBandOrigin = mouseEvent->pos();
				rubberBand->setGeometry(QRect(rubberBandOrigin, QSize()));
				rubberBand->show();

				// Clear existing selection if no modifiers
				if (mouseEvent->modifiers() == Qt::NoModifier) {
					for (SourcererItem *item : items) {
						item->SetSelected(false);
					}
					selectedItem = nullptr;
				}
				return true;
			}
		} else if (event->type() == QEvent::MouseMove) {
			if (rubberBand->isVisible()) {
				QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
				QRect selectionRect = QRect(rubberBandOrigin, mouseEvent->pos()).normalized();
				rubberBand->setGeometry(selectionRect);

				for (SourcererItem *item : items) {
					// Map item geometry to containerWidget coordinates (it is already child of containerWidget)
					if (item->geometry().intersects(selectionRect)) {
						item->SetSelected(true);
						if (mouseEvent->modifiers() == Qt::NoModifier) {
							selectedItem = item; // Update last selected
						}
					} else {
						if (mouseEvent->modifiers() == Qt::NoModifier) {
							item->SetSelected(false);
						}
					}
				}
				return true;
			}
		} else if (event->type() == QEvent::MouseButtonRelease) {
			if (rubberBand->isVisible()) {
				rubberBand->hide();
				ApplySelectionToOBS();
				return true;
			}
		}
	}

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
	
	QAction *refreshAction = menu.addAction(tr("Refresh"));
	connect(refreshAction, &QAction::triggered, [this]() { Refresh(); });

	QAction *toggleAll = menu.addAction(tr("All Sources"));
	toggleAll->setCheckable(true);
	toggleAll->setChecked(!filterByCurrentScene);
	connect(toggleAll, &QAction::toggled, [this](bool checked) {
		filterByCurrentScene = !checked;
		Refresh();
	});

	QAction *toggleHideBadges = menu.addAction(tr("Hide Badges"));
	toggleHideBadges->setCheckable(true);
	toggleHideBadges->setChecked(hideBadges);
	connect(toggleHideBadges, &QAction::toggled, [this](bool checked) {
		hideBadges = checked;
		for (SourcererItem *item : items) {
			item->SetBadgesHidden(hideBadges);
		}
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
	bool altPressed = (QGuiApplication::queryKeyboardModifiers() & Qt::AltModifier);
	for (SourcererItem *item : items) {
		item->SetAltPressed(altPressed);
	}
}

void SourcererSourcesDock::OnItemClicked(SourcererItem *item, Qt::KeyboardModifiers modifiers)
{
	// Local UI update
	if (!item)
		return;

	bool isCtrl = (modifiers & Qt::ControlModifier);
	bool isShift = (modifiers & Qt::ShiftModifier);

	if (!isCtrl && !isShift) {
		// Single click: Deselect others, select this
		for (SourcererItem *other : items) {
			if (other != item)
				other->SetSelected(false);
		}
		item->SetSelected(true);
		selectedItem = item;
	} else if (isCtrl) {
		// Toggle
		item->SetSelected(!item->IsSelected());
		selectedItem = item;
	} else if (isShift) {
		// Range
		if (selectedItem) {
			// Clear others first (Simple Shift behavior)
			for (SourcererItem *other : items) {
				other->SetSelected(false);
			}

			auto startIt = std::find(items.begin(), items.end(), selectedItem);
			auto endIt = std::find(items.begin(), items.end(), item);

			if (startIt != items.end() && endIt != items.end()) {
				// Determine range direction
				long dist = std::distance(startIt, endIt);
				auto current = startIt;
				if (dist >= 0) {
					while (current != endIt + 1) {
						(*current)->SetSelected(true);
						current++;
					}
				} else {
					// Backwards
					while (current != endIt - 1) {
						(*current)->SetSelected(true);
						current--;
					}
				}
				// Do NOT update selectedItem (Anchor)
			} else {
				// Should not happen, but fallback
				item->SetSelected(true);
				selectedItem = item;
			}
		} else {
			// No anchor
			for (SourcererItem *other : items) {
				other->SetSelected(false);
			}
			item->SetSelected(true);
			selectedItem = item;
		}
	}

	// Sync to OBS
	ApplySelectionToOBS();
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
			// Iterate ALL items (including nested ones) and deselect them
			for (SourcererItem *item : items) {
				// Update Local UI
				item->SetSelected(false);

				// Update OBS
				obs_sceneitem_t *si = item->GetSceneItem();
				if (si) {
					obs_sceneitem_select(si, false);
				} else {
					// Fallback for global sources (try to find in root scene)
					obs_source_t *source = item->GetSource();
					if (source) {
						obs_scene_t *scene = obs_scene_from_source(connectedScene);
						if (scene) {
							const char *name = obs_source_get_name(source);
							obs_sceneitem_t *found =
								obs_scene_find_source_recursive(scene, name);
							if (found) {
								obs_sceneitem_select(found, false);
							}
						}
					}
				}
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
	item->SetBadgesHidden(dock->hideBadges);
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

	// Recursively add items from groups
	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *groupScene = obs_sceneitem_group_get_scene(item);
		if (groupScene) {
			obs_scene_enum_items(groupScene, EnumSceneItems, dock);
		}
	}

	SourcererItem *widget = new SourcererItem(source);

	widget->SetItemWidth(dock->itemWidth);
	widget->SetBadgesHidden(dock->hideBadges);
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

	if (event == OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED) {
		// Use queued connection to avoid crashes
		QMetaObject::invokeMethod(dock, &SourcererSourcesDock::Refresh, Qt::QueuedConnection);
	}
}

void SourcererSourcesDock::SourceCreate(void *data, calldata_t *cd)
{
	SourcererSourcesDock *dock = static_cast<SourcererSourcesDock *>(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	if (!source)
		return;

	// If we are showing "All Sources", we need to add this source
	if (!dock->filterByCurrentScene) {
		QMetaObject::invokeMethod(dock, &SourcererSourcesDock::Refresh, Qt::QueuedConnection);
	}
}

void SourcererSourcesDock::SourceRemove(void *data, calldata_t *cd)
{
	SourcererSourcesDock *dock = static_cast<SourcererSourcesDock *>(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	if (!source)
		return;

	// If we are showing "All Sources", we need to remove this source
	if (!dock->filterByCurrentScene) {
		QMetaObject::invokeMethod(dock, &SourcererSourcesDock::Refresh, Qt::QueuedConnection);
	}
}

void SourcererSourcesDock::SceneItemReorder(void *data, calldata_t *cd)
{
	Q_UNUSED(cd); // param might contain scene but we refresh current view anyway
	SourcererSourcesDock *dock = static_cast<SourcererSourcesDock *>(data);
	// Reordering means we need to refresh to reflect new order
	QMetaObject::invokeMethod(dock, &SourcererSourcesDock::Refresh, Qt::QueuedConnection);
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
			// Instead of manually disconnecting just the root, use DisconnectAllScenes
			// to ensure we clean up all group connections that were added to monitoredScenes.
			// This prevents stale or duplicate connections when switching back and forth.
			DisconnectAllScenes();
			// DisconnectAllScenes releases the monitored references.
			// We still hold the 'connectedScene' reference which we must release.
			obs_source_release(connectedScene);
			connectedScene = nullptr;
		}

		if (scene_source) {
			connectedScene = scene_source;
			// We keep the reference (don't release yet, done in destructor or disconnect)
			ConnectSceneSignals(connectedScene);
		}
	} else {
		// Same scene, just release the temp reference we got
		if (scene_source)
			obs_source_release(scene_source);
	}

	// Always sync selection when checking connection
	SyncSelection();
}

void SourcererSourcesDock::ConnectSceneSignals(obs_source_t *source)
{
	if (!source)
		return;

	// Check if already connected
	for (obs_source_t *s : monitoredScenes) {
		if (s == source)
			return;
	}

	obs_source_get_ref(source);
	monitoredScenes.push_back(source);

	signal_handler_t *sh = obs_source_get_signal_handler(source);

	signal_handler_connect(sh, "item_select", SceneItemSelect, this);
	signal_handler_connect(sh, "item_deselect", SceneItemDeselect, this);
	signal_handler_connect(sh, "item_visible", SceneItemVisible, this);
	signal_handler_connect(sh, "item_locked", SceneItemLocked, this);
	signal_handler_connect(sh, "item_add", SceneItemAdd, this);
	signal_handler_connect(sh, "item_remove", SceneItemRemove, this);
	signal_handler_connect(sh, "reorder", SceneItemReorder, this);

	// Recurse into groups if this is a scene
	obs_scene_t *scene = obs_scene_from_source(source);
	if (scene) {
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
				if (obs_sceneitem_is_group(item)) {
					obs_source_t *groupSource = obs_sceneitem_get_source(item);
					// The group source itself has the scene
					SourcererSourcesDock *dock = static_cast<SourcererSourcesDock *>(param);
					dock->ConnectSceneSignals(groupSource);
				}
				return true;
			},
			this);
	}
}

void SourcererSourcesDock::DisconnectAllScenes()
{
	for (obs_source_t *source : monitoredScenes) {
		signal_handler_t *sh = obs_source_get_signal_handler(source);
		signal_handler_disconnect(sh, "item_select", SceneItemSelect, this);
		signal_handler_disconnect(sh, "item_deselect", SceneItemDeselect, this);
		signal_handler_disconnect(sh, "item_visible", SceneItemVisible, this);
		signal_handler_disconnect(sh, "item_locked", SceneItemLocked, this);
		signal_handler_disconnect(sh, "item_add", SceneItemAdd, this);
		signal_handler_disconnect(sh, "item_remove", SceneItemRemove, this);
		signal_handler_disconnect(sh, "reorder", SceneItemReorder, this);
		obs_source_release(source);
	}
	monitoredScenes.clear();
}

void SourcererSourcesDock::SyncSelection()
{
	if (!connectedScene)
		return;

	// Iterate our items and update their state based on actual OBS state
	// This handles nested items correctly because each item knows its scene item
	for (SourcererItem *widget : items) {
		obs_sceneitem_t *item = widget->GetSceneItem();
		if (item) {
			bool selected = obs_sceneitem_selected(item);
			bool visible = obs_sceneitem_visible(item);
			bool locked = obs_sceneitem_locked(item);

			if (widget->IsSelected() != selected)
				widget->SetSelected(selected);

			widget->SetSceneItemVisible(visible);
			widget->SetSceneItemLocked(locked);

			if (selected)
				selectedItem = widget;
		}
	}
}

void SourcererSourcesDock::ApplySelectionToOBS()
{
	if (!connectedScene)
		return;

	obs_scene_t *scene = obs_scene_from_source(connectedScene);
	if (!scene)
		return;

	for (SourcererItem *widget : items) {
		bool shouldBeSelected = widget->IsSelected();
		obs_sceneitem_t *si = widget->GetSceneItem();

		if (si) {
			if (obs_sceneitem_selected(si) != shouldBeSelected) {
				obs_sceneitem_select(si, shouldBeSelected);
			}
		} else {
			// Fallback for global sources or lost references
			obs_source_t *source = widget->GetSource();
			if (source) {
				const char *name = obs_source_get_name(source);
				obs_sceneitem_t *found = obs_scene_find_source_recursive(scene, name);
				if (found) {
					if (obs_sceneitem_selected(found) != shouldBeSelected) {
						obs_sceneitem_select(found, shouldBeSelected);
					}
				}
			}
		}
	}
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

void SourcererSourcesDock::SceneItemAdd(void *data, calldata_t *cd)
{
	SourcererSourcesDock *dock = static_cast<SourcererSourcesDock *>(data);
	obs_sceneitem_t *item = (obs_sceneitem_t *)calldata_ptr(cd, "item");
	if (!item)
		return;

	// Only needed if filtering by scene
	if (!dock->filterByCurrentScene)
		return;

	// Use invokeMethod to ensure UI operations happen on main thread
	QMetaObject::invokeMethod(
		dock,
		[dock, item]() {
			if (!dock->filterByCurrentScene)
				return; // Re-check in case it changed

			// Check if already exists (sanity check)
			for (SourcererItem *existing : dock->items) {
				if (existing->GetSceneItem() == item)
					return;
			}

			obs_source_t *source = obs_sceneitem_get_source(item);
			if (!source)
				return;

			const char *id = obs_source_get_id(source);
			if (strcmp(id, "scene") == 0)
				return;

			// Recursively add items from groups
			if (obs_sceneitem_is_group(item)) {
				obs_scene_t *groupScene = obs_sceneitem_group_get_scene(item);
				if (groupScene) {
					obs_scene_enum_items(groupScene, EnumSceneItems, dock);

					// Also connect signals for this new group
					obs_source_t *groupSource = obs_sceneitem_get_source(item);
					if (groupSource) {
						dock->ConnectSceneSignals(groupSource);
					}
				}
			}

			SourcererItem *widget = new SourcererItem(source);

			widget->SetItemWidth(dock->itemWidth);
			widget->SetBadgesHidden(dock->hideBadges);
			widget->SetSceneItemVisible(obs_sceneitem_visible(item));
			widget->SetSceneItemLocked(obs_sceneitem_locked(item));
			widget->SetHasSceneContext(true);
			widget->SetSceneItem(item);

			dock->items.push_back(widget);

			connect(widget, &SourcererItem::Clicked, dock, &SourcererSourcesDock::OnItemClicked);
			connect(widget, &SourcererItem::DoubleClicked, dock,
				&SourcererSourcesDock::OnItemDoubleClicked);
			connect(widget, &SourcererItem::MenuRequested, dock,
				&SourcererSourcesDock::OnItemMenuRequested);
			connect(widget, &SourcererItem::ToggleVisibilityRequested, dock,
				&SourcererSourcesDock::OnToggleVisibilityRequested);
			connect(widget, &SourcererItem::ToggleLockRequested, dock,
				&SourcererSourcesDock::OnToggleLockRequested);

			dock->flowLayout->addWidget(widget);

			// Sync selection state if the new item is selected
			if (obs_sceneitem_selected(item)) {
				widget->SetSelected(true);
				dock->selectedItem = widget;
			}
		},
		Qt::QueuedConnection);
}

void SourcererSourcesDock::SceneItemRemove(void *data, calldata_t *cd)
{
	SourcererSourcesDock *dock = static_cast<SourcererSourcesDock *>(data);
	obs_sceneitem_t *item = (obs_sceneitem_t *)calldata_ptr(cd, "item");
	if (!item)
		return;

	if (!dock->filterByCurrentScene)
		return;

	QMetaObject::invokeMethod(
		dock,
		[dock, item]() {
			auto it = std::find_if(dock->items.begin(), dock->items.end(), [item](SourcererItem *widget) {
				return widget->GetSceneItem() == item;
			});

			if (it != dock->items.end()) {
				SourcererItem *widget = *it;

				// Recurse cleanup if group
				if (obs_sceneitem_is_group(item)) {
					obs_scene_t *groupScene = obs_sceneitem_group_get_scene(item);
					if (groupScene) {
						dock->RemoveItemsInScene(groupScene);
					}
				}

				if (dock->selectedItem == widget) {
					dock->selectedItem = nullptr;
				}

				dock->items.erase(it);
				dock->flowLayout->removeWidget(widget);
				delete widget;
			}
		},
		Qt::QueuedConnection);
}

void SourcererSourcesDock::RemoveItemsInScene(obs_scene_t *scene)
{
	std::vector<SourcererItem *> toRemove;
	for (SourcererItem *item : items) {
		obs_sceneitem_t *si = item->GetSceneItem();
		if (si && obs_sceneitem_get_scene(si) == scene) {
			toRemove.push_back(item);
		}
	}

	for (SourcererItem *widget : toRemove) {
		obs_sceneitem_t *si = widget->GetSceneItem();
		if (si && obs_sceneitem_is_group(si)) {
			obs_scene_t *groupScene = obs_sceneitem_group_get_scene(si);
			if (groupScene) {
				RemoveItemsInScene(groupScene);
			}
		}

		if (selectedItem == widget)
			selectedItem = nullptr;

		flowLayout->removeWidget(widget);

		auto it = std::find(items.begin(), items.end(), widget);
		if (it != items.end())
			items.erase(it);

		delete widget;
	}
}

QJsonObject SourcererSourcesDock::Save() const

{
	QJsonObject obj;
	obj["itemWidth"] = itemWidth;
	obj["showZoomControls"] = statusBar->isVisible();
	obj["filterByCurrentScene"] = filterByCurrentScene;
	obj["hideBadges"] = hideBadges;
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
	if (obj.contains("hideBadges")) {
		hideBadges = obj["hideBadges"].toBool(false);
	}
}
