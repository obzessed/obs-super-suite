#include "sourcerer_scenes_dock.hpp"

#include "QListWidget"
#include "plugin-support.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QDockWidget>
#include <QContextMenuEvent>
#include <QJsonObject>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QTimer>
#include <QGuiApplication>
#include <QMainWindow>
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QPainter>
#include <QStyle>
#include <QStyleOptionSlider>

#define MIN_ITEM_WIDTH 60
#define MAX_ITEM_WIDTH 500
#define ZOOM_STEP 20

#define T_BAR_PRECISION 1024
#define T_BAR_PRECISION_F ((float)T_BAR_PRECISION)
#define T_BAR_CLAMP (T_BAR_PRECISION / 10)

// Custom Slider for T-Bar with visual markers
class TBarSlider : public QSlider {
public:
	TBarSlider(Qt::Orientation orientation, QWidget *parent = nullptr) : QSlider(orientation, parent) {}

protected:
	void paintEvent(QPaintEvent *ev) override
	{
		QSlider::paintEvent(ev);

		QPainter p(this);
		QStyleOptionSlider opt;
		initStyleOption(&opt);

		QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);

		// Draw clamp markers (lines) at 10% and 90%
		// T_BAR_PRECISION is 1024. Clamp is T_BAR_PRECISION / 10 = 10%.

		constexpr float pct1 = 0.1f;
		constexpr float pct2 = 0.9f;

		p.setPen(QColor(255, 80, 80, 180)); // Semi-transparent Red

		if (orientation() == Qt::Horizontal) {
			int x1 = groove.left() + (int)(groove.width() * pct1);
			int x2 = groove.left() + (int)(groove.width() * pct2);
			int top = groove.top() - 2;
			int bottom = groove.bottom() + 2;

			p.drawLine(x1, top, x1, bottom);
			p.drawLine(x2, top, x2, bottom);
		} else {
			int y1 = groove.top() + (int)(groove.height() * pct1);
			int y2 = groove.top() + (int)(groove.height() * pct2);
			int left = groove.left() - 2;
			int right = groove.right() + 2;

			p.drawLine(left, y1, right, y1);
			p.drawLine(left, y2, right, y2);
		}
	}
};

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

	refreshTimer = new QTimer(this);
	refreshTimer->setSingleShot(true);
	connect(refreshTimer, &QTimer::timeout, this, &SourcererScenesDock::PerformRefresh);

	mainLayout->addWidget(statusBar);

	contentContainer = new QWidget(this);
	QHBoxLayout *contentLayout = new QHBoxLayout(contentContainer);
	contentLayout->setContentsMargins(0, 0, 0, 0);
	contentLayout->setSpacing(0);
	contentLayout->addWidget(scrollArea);
	mainLayout->insertWidget(0, contentContainer); // Insert at top (index 0)

	obs_frontend_add_event_callback(FrontendEvent, this);
}

static bool IsValidTBarTransition(const obs_source_t *transition)
{
	if (!transition)
		return false;

	if (const std::string id = obs_source_get_id(transition);
	    (id == "cut_transition" || id == "obs_stinger_transition")) {
		return false;
	}

	return true;
}

static int obs_frontend_get_tbar_position_safe()
{
	// workaround for https://github.com/obsproject/obs-studio/pull/13128
	if (obs_frontend_preview_program_mode_active()) {
		return obs_frontend_get_tbar_position();
	}

	return 0;
}

static void obs_frontend_set_tbar_position_safe(int value)
{
	// workaround for https://github.com/obsproject/obs-studio/pull/13128
	if (obs_frontend_preview_program_mode_active()) {
		obs_frontend_set_tbar_position(value);
	}
}

static void obs_frontend_release_tbar_safe()
{
	// workaround for https://github.com/obsproject/obs-studio/pull/13128
	if (obs_frontend_preview_program_mode_active()) {
		obs_frontend_release_tbar();
	}
}

void SourcererScenesDock::SetupTBar()
{
	tBarScrollingWithCtrl = false;

	// workaround for https://github.com/obsproject/obs-studio/pull/13116
	// FIXME: obs frontend won't have the `slider-tbar` init until we enable studio mode.
	// accessing t-bar when obs started with studio-mode disabled causes obs to crash
	static QSlider *buggyOBSTBarSlider = nullptr;
	static bool checkedBuggyOBSTBar = false;

	auto setOBSBasicTBar = [](const int value) {
		if (buggyOBSTBarSlider) {
			buggyOBSTBarSlider->setValue(value);
		} else {
			obs_frontend_set_tbar_position_safe(value);
		}
	};

	if (!checkedBuggyOBSTBar) {
		constexpr int test_value = T_BAR_PRECISION / 2;

		auto orig_val = obs_frontend_get_tbar_position_safe();
		obs_frontend_set_tbar_position_safe(test_value);
		auto isBuggyOBSTBar = test_value != obs_frontend_get_tbar_position_safe();
		obs_frontend_set_tbar_position_safe(orig_val);
		obs_frontend_release_tbar_safe();

		if (isBuggyOBSTBar) {
			// OBSBasic < OBSMainWindow < QMainWindow
			const auto *mainWin = static_cast<QMainWindow *>(obs_frontend_get_main_window());

			// find QSlider in main window children
			for (QList<QSlider *> sliders = mainWin->findChildren<QSlider *>(); QSlider *slider : sliders) {
				if (slider->property("class").toString() == "slider-tbar") {
					slider->setProperty("tbar-slider-fix-applied", true);
					obs_log(LOG_INFO, "Applied OBSBasic T-Bar fix to QSlider(%s)", "slider-tbar");
					buggyOBSTBarSlider = slider;
					break;
				}
			}
			if (!buggyOBSTBarSlider) {
				obs_log(LOG_WARNING, "Failed to find buggy OBSBasic T-Bar slider. T-Bar may not work correctly.");
			}
		}

		checkedBuggyOBSTBar = true;
	}

	if (tbarSlider) {
		delete tbarSlider;
		tbarSlider = nullptr;
	}

	if (tbarContainer) {
		delete tbarContainer;
		tbarContainer = nullptr;
	}

	if (tBarPos == TBarPosition::Hidden)
		return;

	const Qt::Orientation orientation =
		(tBarPos == TBarPosition::Bottom || tBarPos == TBarPosition::Top) ? Qt::Horizontal : Qt::Vertical;
	tbarSlider = new TBarSlider(orientation, this);
	tbarSlider->setRange(0, T_BAR_PRECISION - 1);
	tbarSlider->setToolTip("Transition T-Bar");
	tbarSlider->installEventFilter(this);

	if (obs_frontend_preview_program_mode_active()) {
		obs_source_t *transition = obs_frontend_get_current_transition();
		if (IsValidTBarTransition(transition)) {
			tbarSlider->setEnabled(true);
		} else {
			tbarSlider->setEnabled(false);
			tbarSlider->setToolTip("Transition T-Bar (Disabled - Unsupported Transition)");
		}
		if (transition)
			obs_source_release(transition);
	} else {
		tbarSlider->setEnabled(false);
		tbarSlider->setToolTip("Transition T-Bar (Disabled - Not in Studio Mode)");
	}

	// Initial value
	tbarSlider->setValue(obs_frontend_get_tbar_position_safe());

	// Stylesheet for better visibility
	tbarSlider->setStyleSheet(
		"QSlider:horizontal { height: 36px; }"
		"QSlider::groove:horizontal { "
		"    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #353535, stop:0.1 #353535, stop:0.101 #903030, stop:0.899 #903030, stop:0.9 #353535, stop:1 #353535); "
		"    height: 8px; border-radius: 4px; "
		"}"
		"QSlider::sub-page:horizontal { background: #4D79E6; border-radius: 4px; }"
		"QSlider::add-page:horizontal { background: transparent; border-radius: 4px; }"
		"QSlider::handle:horizontal { background: #FFFFFF; width: 18px; height: 36px; margin: -18px 0; border-radius: 4px; }"
		"QSlider::handle:horizontal:hover { background: #F9FAFB; }"
		"QSlider::handle:horizontal:pressed { background: #F3F4F6; }"
		"QSlider[inActiveZone=\"true\"]::handle:horizontal { background: #FF5555; }"

		"QSlider:vertical { width: 36px; }"
		"QSlider::groove:vertical { "
		"    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #353535, stop:0.1 #353535, stop:0.101 #903030, stop:0.899 #903030, stop:0.9 #353535, stop:1 #353535); "
		"    width: 8px; border-radius: 4px; "
		"}"
		"QSlider::sub-page:vertical { background: transparent; border-radius: 4px; }"
		"QSlider::add-page:vertical { background: #4D79E6; border-radius: 4px; }"
		"QSlider::handle:vertical { background: #FFFFFF; height: 18px; width: 36px; margin: 0 -18px; border-radius: 4px; }"
		"QSlider::handle:vertical:hover { background: #F9FAFB; }"
		"QSlider::handle:vertical:pressed { background: #F3F4F6; }"
		"QSlider[inActiveZone=\"true\"]::handle:vertical { background: #FF5555; }");

	connect(tbarSlider, &QSlider::valueChanged, [this, setOBSBasicTBar](const int value) {
		// Handle Color Update based on Clamp
		bool inClamp = (value <= T_BAR_CLAMP) || (value >= (T_BAR_PRECISION - T_BAR_CLAMP));
		bool inActive = !inClamp;
		bool wasInActive = tbarSlider->property("inActiveZone").toBool();
		if (inActive != wasInActive) {
			tbarSlider->setProperty("inActiveZone", inActive);
			tbarSlider->style()->unpolish(tbarSlider);
			tbarSlider->style()->polish(tbarSlider);
		}

		// Only set if triggered by user interaction (we'll block signals when updating from event)
		setOBSBasicTBar(value);
	});

	connect(tbarSlider, &QSlider::sliderReleased, [this] { HandleTBarRelease(); });

	constexpr auto layout_margin = 8;

	if (tBarPos == TBarPosition::Bottom) {
		// Add to main layout (vertical)
		tbarContainer = new QWidget(this);
		QVBoxLayout *tbarLayout = new QVBoxLayout(tbarContainer);
		tbarLayout->setContentsMargins(layout_margin, layout_margin, layout_margin, layout_margin);
		tbarLayout->addWidget(tbarSlider);
		layout()->addWidget(tbarContainer);
	} else if (tBarPos == TBarPosition::Top) {
		// Add to main layout (vertical, at top)
		tbarContainer = new QWidget(this);
		QVBoxLayout *tbarLayout = new QVBoxLayout(tbarContainer);
		tbarLayout->setContentsMargins(layout_margin, layout_margin, layout_margin, layout_margin);
		tbarLayout->addWidget(tbarSlider);
		// Assuming layout() is the main QVBoxLayout
		static_cast<QVBoxLayout *>(layout())->insertWidget(0, tbarContainer);
	} else if (tBarPos == TBarPosition::Right) {
		// Add to content layout (horizontal)
		tbarContainer = new QWidget(this);
		QHBoxLayout *tbarLayout = new QHBoxLayout(tbarContainer);
		tbarLayout->setContentsMargins(layout_margin, layout_margin, layout_margin, layout_margin);
		tbarLayout->addWidget(tbarSlider);
		contentContainer->layout()->addWidget(tbarContainer);
	} else if (tBarPos == TBarPosition::Left) {
		// Add to content layout (horizontal, at left)
		tbarContainer = new QWidget(this);
		QHBoxLayout *tbarLayout = new QHBoxLayout(tbarContainer);
		tbarLayout->setContentsMargins(layout_margin, layout_margin, layout_margin, layout_margin);
		tbarLayout->addWidget(tbarSlider);
		// Assuming contentContainer->layout() is QHBoxLayout
		static_cast<QHBoxLayout *>(contentContainer->layout())->insertWidget(0, tbarContainer);
	}
}

void SourcererScenesDock::SetTBarPosition(TBarPosition pos)
{
	if (tBarPos == pos)
		return;

	tBarPos = pos;

	if (!frontendReady) return;

	SetupTBar();
}

void SourcererScenesDock::UpdateTBarValue()
{
	if (tbarSlider) {
		// Prevent feedback loop: If user is dragging OUR slider, ignore updates from OBS
		if (tbarSlider->isSliderDown())
			return;

		QSignalBlocker blocker(tbarSlider);
		tbarSlider->setValue(obs_frontend_get_tbar_position_safe());
	}
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
	if (obj == tbarSlider && event->type() == QEvent::KeyRelease) {
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		if (keyEvent->key() == Qt::Key_Control) {
			if (tBarScrollingWithCtrl) {
				tBarScrollingWithCtrl = false;
				HandleTBarRelease();
				return true;
			}
		}
	}

	if (event->type() == QEvent::Wheel) {

		QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);

		if (obj == tbarSlider && tbarSlider) {
			if (wheelEvent->modifiers() & Qt::ControlModifier) {
				int delta = wheelEvent->angleDelta().y();
				int step = T_BAR_PRECISION / 20; // 5% per scroll tick
				if (delta < 0)
					step = -step;

				int newVal = tbarSlider->value() + step;
				if (newVal < 0)
					newVal = 0;
				if (newVal >= T_BAR_PRECISION)
					newVal = T_BAR_PRECISION - 1;

				tbarSlider->setValue(newVal);
				tBarScrollingWithCtrl = true;

				return true;
			}
		}

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

	QAction *refreshAction = menu.addAction(tr("Refresh"));
	connect(refreshAction, &QAction::triggered, [this]() { Refresh(); });

	QAction *toggleLiveMode = menu.addAction(tr("Live Mode"));
	toggleLiveMode->setCheckable(true);
	toggleLiveMode->setChecked(liveMode);
	connect(toggleLiveMode, &QAction::toggled, [this](bool checked) {
		liveMode = checked;
		if (checked) {
			HighlightCurrentScene();
		} else {
			// Clear program and FTB highlighting
			for (SourcererItem *item : items) {
				item->SetProgram(false);
				item->SetFTB(false);
			}
		}
	});

	QAction *toggleSyncSelection = menu.addAction(tr("Sync Selection"));
	toggleSyncSelection->setCheckable(true);
	toggleSyncSelection->setChecked(syncSelection);
	connect(toggleSyncSelection, &QAction::toggled, [this](bool checked) {
		syncSelection = checked;
		if (checked) {
			HighlightCurrentScene();
		} else {
			// Clear selection highlighting
			for (SourcererItem *item : items) {
				item->SetSelected(false);
			}
		}
	});

	QAction *toggleScrollToProgram = menu.addAction(tr("Scroll to Program Scene"));
	toggleScrollToProgram->setCheckable(true);
	toggleScrollToProgram->setChecked(scrollToProgram);
	connect(toggleScrollToProgram, &QAction::toggled, [this](bool checked) { scrollToProgram = checked; });

	QAction *toggleReadOnly = menu.addAction(tr("Read Only"));

	toggleReadOnly->setCheckable(true);
	toggleReadOnly->setChecked(isReadOnly);
	connect(toggleReadOnly, &QAction::toggled, [this](bool checked) { isReadOnly = checked; });

	QAction *toggleDblClick = menu.addAction(tr("Double-Click to Program"));
	toggleDblClick->setCheckable(true);
	toggleDblClick->setChecked(doubleClickToProgram);
	connect(toggleDblClick, &QAction::toggled, [this](bool checked) { doubleClickToProgram = checked; });

	QAction *toggleHideEmpty = menu.addAction(tr("Hide Empty Scenes"));
	toggleHideEmpty->setCheckable(true);
	toggleHideEmpty->setChecked(hideEmptyScenes);
	connect(toggleHideEmpty, &QAction::toggled, [this](bool checked) {
		hideEmptyScenes = checked;
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

	menu.addSeparator();

	QMenu *tbarMenu = menu.addMenu(tr("T-Bar Position"));
	QActionGroup *tbarGroup = new QActionGroup(this);

	QAction *tbarHidden = tbarMenu->addAction(tr("Hidden"));
	tbarHidden->setCheckable(true);
	tbarHidden->setChecked(tBarPos == TBarPosition::Hidden);
	tbarGroup->addAction(tbarHidden);
	connect(tbarHidden, &QAction::triggered, [this]() { SetTBarPosition(TBarPosition::Hidden); });

	QAction *tbarLeft = tbarMenu->addAction(tr("Left"));
	tbarLeft->setCheckable(true);
	tbarLeft->setChecked(tBarPos == TBarPosition::Left);
	tbarGroup->addAction(tbarLeft);
	connect(tbarLeft, &QAction::triggered, [this]() { SetTBarPosition(TBarPosition::Left); });

	QAction *tbarRight = tbarMenu->addAction(tr("Right"));
	tbarRight->setCheckable(true);
	tbarRight->setChecked(tBarPos == TBarPosition::Right);
	tbarGroup->addAction(tbarRight);
	connect(tbarRight, &QAction::triggered, [this]() { SetTBarPosition(TBarPosition::Right); });

	QAction *tbarTop = tbarMenu->addAction(tr("Top"));
	tbarTop->setCheckable(true);
	tbarTop->setChecked(tBarPos == TBarPosition::Top);
	tbarGroup->addAction(tbarTop);
	connect(tbarTop, &QAction::triggered, [this]() { SetTBarPosition(TBarPosition::Top); });

	QAction *tbarBottom = tbarMenu->addAction(tr("Bottom"));
	tbarBottom->setCheckable(true);
	tbarBottom->setChecked(tBarPos == TBarPosition::Bottom);
	tbarGroup->addAction(tbarBottom);
	connect(tbarBottom, &QAction::triggered, [this]() { SetTBarPosition(TBarPosition::Bottom); });

	menu.exec(event->globalPos());
}

void SourcererScenesDock::OnItemClicked(SourcererItem *item)
{
	if (isReadOnly || !item)
		return;

	obs_source_t *source = item->GetSource();
	if (!source)
		return;

	// Check for Shift+click to open filters
	if (QGuiApplication::queryKeyboardModifiers() & Qt::ShiftModifier) {
		obs_frontend_open_source_filters(source);
		return;
	}

	if (obs_frontend_preview_program_mode_active()) {
		if (syncSelection) {
			obs_frontend_set_current_preview_scene(source);
		}
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
		// In studio mode
		if (syncSelection) {
			// Switch preview then transition
			obs_frontend_set_current_preview_scene(source);
			obs_frontend_preview_program_trigger_transition();
		} else {
			// Directly switch program scene
			obs_frontend_set_current_scene(source);
		}
	} else {
		// In standard mode, just switch scene (same as click)
		obs_frontend_set_current_scene(source);
	}
}

void SourcererScenesDock::keyPressEvent(QKeyEvent *event)
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

void SourcererScenesDock::keyReleaseEvent(QKeyEvent *event)
{
	UpdateKeyModifiers();

	if (event->key() == Qt::Key_Control && tBarScrollingWithCtrl) {
		tBarScrollingWithCtrl = false;
		HandleTBarRelease();
	}

	QWidget::keyReleaseEvent(event);
}

void SourcererScenesDock::UpdateKeyModifiers()
{
	bool altPressed = (QGuiApplication::queryKeyboardModifiers() & Qt::AltModifier);
	for (SourcererItem *item : items) {
		item->SetAltPressed(altPressed);
	}
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
	// Debounce
	if (refreshTimer)
		refreshTimer->start(100);
}

void SourcererScenesDock::PerformRefresh()
{
	Clear();

	struct obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);

	for (size_t i = 0; i < scenes.sources.num; i++) {
		obs_source_t *source = scenes.sources.array[i];
		SourcererItem *item = new SourcererItem(source);
		item->SetItemWidth(itemWidth);
		item->SetBadgesHidden(hideBadges);
		items.push_back(item);

		if (hideEmptyScenes) {
			obs_scene_t *scene = obs_scene_from_source(source);
			bool empty = true;
			obs_scene_enum_items(
				scene,
				[](obs_scene_t *, obs_sceneitem_t *, void *param) {
					bool *e = (bool *)param;
					*e = false;
					return false; // Found one, stop
				},
				&empty);
			if (empty) {
				item->hide();
			}
		}

		connect(item, &SourcererItem::Clicked, this, &SourcererScenesDock::OnItemClicked);
		connect(item, &SourcererItem::DoubleClicked, this, &SourcererScenesDock::OnItemDoubleClicked);
		connect(item, &SourcererItem::SceneItemCountChanged, [this](SourcererItem *item, int count) {
			if (hideEmptyScenes) {
				item->setVisible(count > 0);
			} else {
				if (!item->isVisible())
					item->show();
			}
		});

		flowLayout->addWidget(item);
	}

	obs_frontend_source_list_free(&scenes);

	if (liveMode || syncSelection)
		HighlightCurrentScene();
}

void SourcererScenesDock::FrontendEvent(obs_frontend_event event, void *data)
{
	const auto dock = static_cast<SourcererScenesDock *>(data);

	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
		if (obs_source_t *scene = obs_frontend_get_current_scene()) {
			const char *name = obs_source_get_name(scene);
			obs_log(LOG_INFO, "Scene Switched (Program): %s", name);
			obs_source_release(scene);
		}
	} else if (event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED) {
		if (obs_source_t *scene = obs_frontend_get_current_preview_scene()) {
			const char *name = obs_source_get_name(scene);
			obs_log(LOG_INFO, "Scene Switched (Preview): %s", name);
			obs_source_release(scene);
		}
	} else if (event == OBS_FRONTEND_EVENT_TRANSITION_STOPPED) {
		obs_log(LOG_INFO, "Transition Stopped");
	} else if (event == OBS_FRONTEND_EVENT_TRANSITION_CHANGED) {
		if (obs_source_t *transition = obs_frontend_get_current_transition()) {
			const char *name = obs_source_get_name(transition);
			obs_log(LOG_INFO, "Transition Changed to: %s", name);
			obs_source_release(transition);
		}
	} else if (event == OBS_FRONTEND_EVENT_TRANSITION_DURATION_CHANGED) {
		obs_log(LOG_INFO, "Transition Duration Changed");
	} else if (event == OBS_FRONTEND_EVENT_TBAR_VALUE_CHANGED) {
		dock->UpdateTBarValue();
	} else if (event == OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED) {
		obs_log(LOG_WARNING, "OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED");
		// Use queued connection to avoid crashes if triggered by source removal from within dock
		QMetaObject::invokeMethod(dock, &SourcererScenesDock::Refresh, Qt::QueuedConnection);
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
		obs_log(LOG_WARNING, "OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED");

		QMetaObject::invokeMethod(dock, &SourcererScenesDock::Refresh, Qt::QueuedConnection);
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED) {
		obs_log(LOG_WARNING, "OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED");
	}

	if (!dock->liveMode && !dock->syncSelection)
		return;

	if (event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED || event == OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED) {
		obs_log(LOG_INFO, "Studio Mode %s", (event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED) ? "Enabled" : "Disabled");

		// Show or Hide T-Bar based on Studio Mode
		dock->SetupTBar();
	}

	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED || event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED ||
	    event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED || event == OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED ||
	    event == OBS_FRONTEND_EVENT_TRANSITION_STOPPED || event == OBS_FRONTEND_EVENT_TRANSITION_CHANGED) {
		dock->HighlightCurrentScene();
	}

	// obs_log(LOG_ERROR, "Frontend Event: %d", event);
}

void SourcererScenesDock::HandleTBarRelease()
{
	if (!tbarSlider)
		return;

	// Always call the frontend release to ensure UI consistency and event firing
	obs_frontend_release_tbar_safe();

	// Force update shortly after release to catch any resets
	QTimer::singleShot(10, [this] { UpdateTBarValue(); });
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
		previewScene = nullptr; // Or same as program
	}

	// FTB Detection
	obs_source_t *transition = obs_frontend_get_current_transition();
	bool ftbActive = false;
	if (transition) {
		obs_source_t *activeSource = obs_transition_get_active_source(transition);
		if (activeSource == nullptr) {
			ftbActive = true;
			obs_log(LOG_INFO, "FTB Active (No active source in transition)");
		}
		obs_source_release(transition);
	}

	const char *programName = programScene ? obs_source_get_name(programScene) : "";
	const char *previewName = previewScene ? obs_source_get_name(previewScene) : "";

	for (SourcererItem *item : items) {
		obs_source_t *source = item->GetSource();
		const char *name = obs_source_get_name(source);

		bool isProg = programScene && (strcmp(programName, name) == 0);
		bool isPrev = previewScene && (strcmp(previewName, name) == 0);

		// If FTB is active, maybe we shouldn't show program selection?
		// Or maybe show it but differently?
		// For now, let's just log it as requested, but if FTB is active,
		// technically the program scene is still "current" in OBS logic usually,
		// but the audience sees black.
		// Let's mark it Program ONLY if not FTB?
		// The user instruction was "detect ftb... make it aware".
		// I'll leave the selection logic as is for now but log the FTB state.

		// Standard mode: Program is Red.
		// Studio mode: Program is Red, Preview is Blue.
		// If same scene is both, Program Red takes precedence (handled in paintEvent usually,
		// but let's set both flags).

		item->SetFTB(ftbActive && liveMode);
		item->SetProgram(isProg && liveMode);
		item->SetSelected(isPrev);

		if (isProg && scrollToProgram && !syncSelection) {
			scrollArea->ensureWidgetVisible(item);
		}

		if (isPrev && syncSelection) {
			scrollArea->ensureWidgetVisible(item);
		}
	}

	if (programScene)
		obs_source_release(programScene);
	if (previewScene)
		obs_source_release(previewScene);
}

QJsonObject SourcererScenesDock::Save() const
{
	QJsonObject obj;
	obj["itemWidth"] = itemWidth;
	obj["showZoomControls"] = statusBar->isVisible();
	obj["liveMode"] = liveMode;
	obj["syncSelection"] = syncSelection;
	obj["scrollToProgram"] = scrollToProgram;
	obj["hideEmptyScenes"] = hideEmptyScenes;
	obj["hideBadges"] = hideBadges;

	obj["isReadOnly"] = isReadOnly;
	obj["doubleClickToProgram"] = doubleClickToProgram;
	obj["tBarPosition"] = static_cast<int>(tBarPos);
	return obj;
}

void SourcererScenesDock::Load(const QJsonObject &obj)
{
	if (obj.contains("itemWidth")) {
		SetZoom(obj["itemWidth"].toInt(160));
	}
	if (obj.contains("showZoomControls")) {
		statusBar->setVisible(obj["showZoomControls"].toBool(true));
	}
	if (obj.contains("liveMode")) {
		liveMode = obj["liveMode"].toBool(true);
		if (liveMode || syncSelection)
			HighlightCurrentScene();
	}
	if (obj.contains("syncSelection")) {
		syncSelection = obj["syncSelection"].toBool(true);
		if (liveMode || syncSelection)
			HighlightCurrentScene();
	}
	if (obj.contains("scrollToProgram")) {
		scrollToProgram = obj["scrollToProgram"].toBool(true);
	}
	if (obj.contains("hideEmptyScenes")) {
		hideEmptyScenes = obj["hideEmptyScenes"].toBool(false);
	}
	if (obj.contains("hideBadges")) {
		hideBadges = obj["hideBadges"].toBool(false);
	}

	if (obj.contains("isReadOnly")) {
		isReadOnly = obj["isReadOnly"].toBool(false);
	}
	if (obj.contains("doubleClickToProgram")) {
		doubleClickToProgram = obj["doubleClickToProgram"].toBool(true);
	}
	if (obj.contains("tBarPosition")) {
		SetTBarPosition(static_cast<TBarPosition>(obj["tBarPosition"].toInt(0)));
	}
}

void SourcererScenesDock::FrontendReady()
{
	SetupTBar();

	bool attachedSceneListHandlers = false;
	// find the scenes dock and attach reorder handlers.
	{
		auto mainWin = static_cast<QMainWindow *>(obs_frontend_get_main_window());

		for (const auto *dock : mainWin->findChildren<QDockWidget *>("scenesDock")) {
			// obs_log(LOG_ERROR, "DOCK NAME: %s", dock->objectName().toStdString().c_str());

			if (dock->objectName() == "scenesDock") {
				// obs_log(LOG_ERROR, "Found scenesDock, attaching signal handlers");

				// SceneTree < QListWidget < QListView
				for (const auto *sceneList : dock->findChildren<QListWidget *>("scenes")) {
					// obs_log(LOG_ERROR, "Found scenes list widget, attaching item changed handler");

					// detect takeItem and insertItem to detect scene reorders.
					if (QAbstractItemModel *model = sceneList->model()) {
						connect(model, &QAbstractItemModel::rowsMoved, this,
							[this](const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent, int destinationRow) {
								// obs_log(LOG_ERROR, "rowsMoved: %p, %d-%d, %p, %d", &sourceParent, sourceStart, sourceEnd, &destinationParent, destinationRow);
								QMetaObject::invokeMethod(this, &SourcererScenesDock::Refresh, Qt::QueuedConnection);
							});

						connect(model, &QAbstractItemModel::rowsInserted, this,
							[this](const QModelIndex &parent, int first, int last) {
								// obs_log(LOG_ERROR, "rowsInserted: %p, %d, %d", &parent, first, last);
								QMetaObject::invokeMethod(this, &SourcererScenesDock::Refresh, Qt::QueuedConnection);
							});
						connect(model, &QAbstractItemModel::rowsRemoved, this,
							[this](const QModelIndex &parent, int first, int last) {
								// obs_log(LOG_ERROR, "rowsRemoved: %p, %d, %d", &parent, first, last);
								QMetaObject::invokeMethod(this, &SourcererScenesDock::Refresh, Qt::QueuedConnection);
							});

						attachedSceneListHandlers = true;
						goto loopEnd;
					}
				}
			}
		}

loopEnd:
		if (!attachedSceneListHandlers) {
			obs_log(LOG_ERROR, "Failed to find scenes list widget to attach handlers. Scene reordering may not update the dock correctly.");
		}
	}
}
