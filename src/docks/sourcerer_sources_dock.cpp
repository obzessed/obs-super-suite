#include "sourcerer_sources_dock.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
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
		// signal is blocked to prevent recursion if connected that way,
		// though direct call loop is avoided by the itemWidth check above usually.
		if (zoomSlider->value() != width) {
			QSignalBlocker blocker(zoomSlider);
			zoomSlider->setValue(width);
		}
	}
}

void SourcererSourcesDock::Clear()
{
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
}

bool SourcererSourcesDock::EnumSources(void *data, obs_source_t *source)

{
	SourcererSourcesDock *dock = static_cast<SourcererSourcesDock *>(data);

	const char *id = obs_source_get_id(source);
	if (strcmp(id, "scene") == 0)
		return true; // Skip scenes
	if (strcmp(id, "group") == 0)
		return true; // Skip groups

	SourcererItem *item = new SourcererItem(source);
	item->SetItemWidth(dock->itemWidth);
	dock->items.push_back(item);

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

	// In this mode, we show whatever is in the scene, even groups or nested scenes?
	// User said "Show selected scene's source". Usually that means the inputs.
	// But let's apply same filter as global: no scenes, no groups, if we want "Sources"
	// However, if I am in a scene, I might want to see the nested scenes.
	// Let's stick to the behavior of "Sourcerer Sources" which seemed to filter out scenes/groups.

	const char *id = obs_source_get_id(source);
	if (strcmp(id, "scene") == 0)
		return true;
	if (strcmp(id, "group") == 0)
		return true;

	SourcererItem *widget = new SourcererItem(source);
	widget->SetItemWidth(dock->itemWidth);
	dock->items.push_back(widget);

	dock->flowLayout->addWidget(widget);

	return true;
}

void SourcererSourcesDock::FrontendEvent(enum obs_frontend_event event, void *data)
{
	SourcererSourcesDock *dock = static_cast<SourcererSourcesDock *>(data);
	if (!dock->filterByCurrentScene)
		return;

	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED || event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED ||
	    event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED || event == OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED) {
		dock->Refresh();
	}
}
