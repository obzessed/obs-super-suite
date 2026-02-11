#include "sourcerer_scenes_dock.hpp"
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

SourcererScenesDock::SourcererScenesDock(QWidget *parent) : QWidget(parent)
{
	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);

	scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

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
	zoomSlider->setToolTip("Zoom Scene Previews");
	connect(zoomSlider, &QSlider::valueChanged, this, &SourcererScenesDock::SetZoom);

	statusLayout->addStretch(); // Push slider to the right
	statusLayout->addWidget(zoomSlider);

	mainLayout->addWidget(statusBar);

	obs_frontend_add_event_callback(FrontendEvent, this);
}

SourcererScenesDock::~SourcererScenesDock()
{
	obs_frontend_remove_event_callback(FrontendEvent, this);
	Clear();
}

void SourcererScenesDock::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	Refresh();
}

bool SourcererScenesDock::eventFilter(QObject *obj, QEvent *event)
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

void SourcererScenesDock::contextMenuEvent(QContextMenuEvent *event)
{
	QMenu menu(this);
	QAction *toggleStatus = menu.addAction(tr("Show Zoom Controls"));
	toggleStatus->setCheckable(true);
	toggleStatus->setChecked(statusBar->isVisible());

	connect(toggleStatus, &QAction::toggled, statusBar, &QWidget::setVisible);

	QAction *toggleSync = menu.addAction(tr("Sync with Main"));
	toggleSync->setCheckable(true);
	toggleSync->setChecked(syncWithMain);
	connect(toggleSync, &QAction::toggled, [this](bool checked) {
		syncWithMain = checked;
		if (checked) {
			HighlightCurrentScene();
		} else {
			// Optional: Clear selection when disabled?
			// For now, let's leave the last selection or clear it.
			// Let's clear it to indicate sync is off.
			for (SourcererItem *item : items) {
				item->SetSelected(false);
			}
		}
	});

	QAction *toggleReadOnly = menu.addAction(tr("Read Only"));
	toggleReadOnly->setCheckable(true);
	toggleReadOnly->setChecked(isReadOnly);
	connect(toggleReadOnly, &QAction::toggled, [this](bool checked) { isReadOnly = checked; });

	QAction *toggleDblClick = menu.addAction(tr("Double-Click to Program"));
	toggleDblClick->setCheckable(true);
	toggleDblClick->setChecked(doubleClickToProgram);
	connect(toggleDblClick, &QAction::toggled, [this](bool checked) { doubleClickToProgram = checked; });

	menu.exec(event->globalPos());
}

void SourcererScenesDock::OnItemClicked(SourcererItem *item)
{
	if (isReadOnly || !item)
		return;

	obs_source_t *source = item->GetSource();
	if (!source)
		return;

	if (obs_frontend_preview_program_mode_active()) {
		obs_frontend_set_current_preview_scene(source);
	} else {
		obs_frontend_set_current_scene(source);
	}
}

void SourcererScenesDock::OnItemDoubleClicked(SourcererItem *item)
{
	if (isReadOnly || !item || !doubleClickToProgram)
		return;

	obs_source_t *source = item->GetSource();
	if (!source)
		return;

	if (obs_frontend_preview_program_mode_active()) {
		// In studio mode, switch preview then transition
		obs_frontend_set_current_preview_scene(source);
		obs_frontend_preview_program_trigger_transition();
	} else {
		// In standard mode, just switch scene (same as click)
		obs_frontend_set_current_scene(source);
	}
}

void SourcererScenesDock::keyPressEvent(QKeyEvent *event)
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

void SourcererScenesDock::UpdateZoom(int delta_steps)
{
	int newWidth = itemWidth + (delta_steps * ZOOM_STEP);
	if (newWidth < MIN_ITEM_WIDTH)
		newWidth = MIN_ITEM_WIDTH;
	if (newWidth > MAX_ITEM_WIDTH)
		newWidth = MAX_ITEM_WIDTH;

	zoomSlider->setValue(newWidth);
}

void SourcererScenesDock::ResetZoom()
{
	zoomSlider->setValue(160);
}

void SourcererScenesDock::SetZoom(int width)
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

		if (zoomSlider->value() != width) {
			QSignalBlocker blocker(zoomSlider);
			zoomSlider->setValue(width);
		}
	}
}

void SourcererScenesDock::Clear()
{
	QLayoutItem *child;
	while ((child = flowLayout->takeAt(0)) != nullptr) {
		if (child->widget()) {
			delete child->widget();
		}
		delete child;
	}
	items.clear();
}

void SourcererScenesDock::Refresh()
{
	Clear();

	struct obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);

	for (size_t i = 0; i < scenes.sources.num; i++) {
		obs_source_t *source = scenes.sources.array[i];
		SourcererItem *item = new SourcererItem(source);
		item->SetItemWidth(itemWidth);
		items.push_back(item);

		connect(item, &SourcererItem::Clicked, this, &SourcererScenesDock::OnItemClicked);
		connect(item, &SourcererItem::DoubleClicked, this, &SourcererScenesDock::OnItemDoubleClicked);

		flowLayout->addWidget(item);
	}

	obs_frontend_source_list_free(&scenes);

	if (syncWithMain)
		HighlightCurrentScene();
}

void SourcererScenesDock::FrontendEvent(enum obs_frontend_event event, void *data)
{
	SourcererScenesDock *dock = static_cast<SourcererScenesDock *>(data);
	if (!dock->syncWithMain)
		return;

	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED || event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED ||
	    event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED || event == OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED) {
		dock->HighlightCurrentScene();
	}
}

void SourcererScenesDock::HighlightCurrentScene() const
{
	obs_source_t *programScene = nullptr;
	obs_source_t *previewScene = nullptr;

	programScene = obs_frontend_get_current_scene();
	if (obs_frontend_preview_program_mode_active()) {
		// Studio Mode
		previewScene = obs_frontend_get_current_preview_scene();
	} else {
		// Standard Mode
		programScene = obs_frontend_get_current_scene();
		previewScene = nullptr; // Or same as program
	}

	const char *programName = programScene ? obs_source_get_name(programScene) : "";
	const char *previewName = previewScene ? obs_source_get_name(previewScene) : "";

	for (SourcererItem *item : items) {
		obs_source_t *source = item->GetSource();
		const char *name = obs_source_get_name(source);

		bool isProg = programScene && (strcmp(programName, name) == 0);
		bool isPrev = previewScene && (strcmp(previewName, name) == 0);

		// Standard mode: Program is Red.
		// Studio mode: Program is Red, Preview is Blue.
		// If same scene is both, Program Red takes precedence (handled in paintEvent usually,
		// but let's set both flags).

		item->SetProgram(isProg);
		item->SetSelected(isPrev);

		if (isProg || isPrev) {
			scrollArea->ensureWidgetVisible(item);
		}
	}

	if (programScene)
		obs_source_release(programScene);
	if (previewScene)
		obs_source_release(previewScene);
}
