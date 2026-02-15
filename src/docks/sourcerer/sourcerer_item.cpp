#include "sourcerer_item.hpp"

#include "plugin-support.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QCursor>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QMessageBox>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <obs-module.h>
#include <obs-frontend-api.h>

class SourcererDisplay : public OBSQTDisplay {
public:
	double aspectRatio = 16.0 / 9.0;

	SourcererDisplay(QWidget *parent) : OBSQTDisplay(parent)
	{
		struct obs_video_info ovi;
		if (obs_get_video_info(&ovi)) {
			aspectRatio = (double)ovi.base_width / (double)ovi.base_height;
		}

		QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
		policy.setHeightForWidth(true);
		setSizePolicy(policy);
	}

	virtual bool hasHeightForWidth() const override { return true; }
	virtual int heightForWidth(int width) const override { return (int)(width / aspectRatio); }
};

// --- SourcererItemOverlay ---

SourcererItemOverlay::SourcererItemOverlay(QWidget *parent) : QWidget(parent)
{
	// Glassmorphic dark overlay background
	setAutoFillBackground(true);
	QPalette pal = palette();
	pal.setColor(QPalette::Window, QColor(10, 10, 14, 180));
	setPalette(pal);

	layout = new QGridLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->setSpacing(3);

	// Create buttons
	btnVisibility = new QPushButton(this);
	SetupButton(btnVisibility, QString::fromUtf8("👁"), "Toggle Visibility");

	btnLock = new QPushButton(this);
	SetupButton(btnLock, QString::fromUtf8("🔒"), "Toggle Lock");

	btnActive = new QPushButton(this);
	SetupButton(btnActive, QString::fromUtf8("⏻"), "Toggle Active");

	btnInteract = new QPushButton(this);
	SetupButton(btnInteract, QString::fromUtf8("🖱"), "Interact");

	btnPlayPause = new QPushButton(this);
	SetupButton(btnPlayPause, QString::fromUtf8("▶"), "Play/Pause");

	btnProperties = new QPushButton(this);
	SetupButton(btnProperties, QString::fromUtf8("⚙"), "Properties");

	btnFilters = new QPushButton(this);
	SetupButton(btnFilters, QString::fromUtf8("Fx"), "Filters");

	btnDisablePreview = new QPushButton(this);
	SetupButton(btnDisablePreview, QString::fromUtf8("🚫"), "Toggle Preview");

	// Layout grid
	// Row 0: Vis, Lock
	// Row 1: Active, Interact
	// Row 2: Play, DisablePrev
	// Row 3: Filters, Properties
	layout->addWidget(btnVisibility, 0, 0);
	layout->addWidget(btnLock, 0, 1);
	layout->addWidget(btnActive, 1, 0);
	layout->addWidget(btnInteract, 1, 1);
	layout->addWidget(btnPlayPause, 2, 0);
	layout->addWidget(btnDisablePreview, 2, 1);
	layout->addWidget(btnFilters, 3, 0);
	layout->addWidget(btnProperties, 3, 1);

	// Opacity effect for fade animation
	opacityEffect = new QGraphicsOpacityEffect(this);
	opacityEffect->setOpacity(0.0);
	setGraphicsEffect(opacityEffect);

	fadeAnim = new QPropertyAnimation(opacityEffect, "opacity", this);
	fadeAnim->setDuration(80);
	fadeAnim->setEasingCurve(QEasingCurve::InOutQuad);

	hide();
}

void SourcererItemOverlay::ReflowButtons()
{
	// Clear layout
	QLayoutItem *item;
	while ((item = layout->takeAt(0)) != nullptr) {
		delete item;
	}

	std::vector<QPushButton *> visibleButtons;
	// Order matters:
	if (btnVisibility && !btnVisibility->isHidden())
		visibleButtons.push_back(btnVisibility);
	if (btnLock && !btnLock->isHidden())
		visibleButtons.push_back(btnLock);
	if (btnActive && !btnActive->isHidden())
		visibleButtons.push_back(btnActive);
	if (btnInteract && !btnInteract->isHidden())
		visibleButtons.push_back(btnInteract);
	if (btnPlayPause && !btnPlayPause->isHidden())
		visibleButtons.push_back(btnPlayPause);
	if (btnDisablePreview && !btnDisablePreview->isHidden())
		visibleButtons.push_back(btnDisablePreview);
	if (btnFilters && !btnFilters->isHidden())
		visibleButtons.push_back(btnFilters);
	if (btnProperties && !btnProperties->isHidden())
		visibleButtons.push_back(btnProperties);

	for (size_t i = 0; i < visibleButtons.size(); ++i) {
		int row = (int)i / 2;
		int col = (int)i % 2;
		// If last item and odd count, span 2 columns
		if (i == visibleButtons.size() - 1 && visibleButtons.size() % 2 != 0) {
			layout->addWidget(visibleButtons[i], row, 0, 1, 2);
		} else {
			layout->addWidget(visibleButtons[i], row, col);
		}
	}
}

void SourcererItemOverlay::SetupButton(QPushButton *btn, const QString &text, const QString &tooltip)
{
	btn->setText(text);
	btn->setToolTip(tooltip);
	btn->setAccessibleName(tooltip);
	btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	btn->setMinimumSize(28, 28);
	btn->setCursor(Qt::PointingHandCursor);
	btn->setFocusPolicy(Qt::TabFocus);

	btn->setStyleSheet(
		"QPushButton {"
		"  background-color: rgba(255, 255, 255, 18);"
		"  border: 1px solid rgba(255, 255, 255, 30);"
		"  color: rgba(255, 255, 255, 220);"
		"  border-radius: 6px;"
		"  font-size: 13px;"
		"  padding: 2px;"
		"}"
		"QPushButton:hover {"
		"  background-color: rgba(255, 255, 255, 50);"
		"  border: 1px solid rgba(255, 255, 255, 90);"
		"  color: white;"
		"}"
		"QPushButton:pressed {"
		"  background-color: rgba(255, 255, 255, 70);"
		"  border: 1px solid rgba(255, 255, 255, 120);"
		"}"
		"QPushButton:focus {"
		"  outline: none;"
		"  border: 2px solid rgba(80, 160, 255, 200);"
		"}");
}

void SourcererItemOverlay::SetVisibleAnimated(bool visible)
{
	// Ensure we don't trigger hide() when trying to show
	disconnect(fadeAnim, &QPropertyAnimation::finished, this, &QWidget::hide);

	if (visible) {
		show();
		fadeAnim->stop();
		fadeAnim->setStartValue(opacityEffect->opacity());
		fadeAnim->setEndValue(1.0);
		fadeAnim->start();
	} else {
		fadeAnim->stop();
		fadeAnim->setStartValue(opacityEffect->opacity());
		fadeAnim->setEndValue(0.0);
		connect(fadeAnim, &QPropertyAnimation::finished, this, &QWidget::hide);
		fadeAnim->start();
	}
}

void SourcererItemOverlay::mousePressEvent(QMouseEvent *event)
{
	event->ignore();
}

// --- SourcererItem ---

SourcererItem::SourcererItem(obs_source_t *source, QWidget *parent) : QWidget(parent), source(source)
{
	obs_source_get_ref(source);
	obs_source_inc_showing(source);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->setSpacing(2);

	display = new SourcererDisplay(this);
	display->setMinimumSize(120, 60);
	display->setAttribute(Qt::WA_TransparentForMouseEvents); // Pass clicks to Item, but what about overlay?
	display->installEventFilter(this);                       // Install event filter to track geometry changes

	// Overlay needs to be ON TOP of display.
	// Since display is in layout, overlay should be child of Item and positioned manually or stacked.
	// But QLayout manages geometry.
	// Best approach: Parent the overlay to 'display' or 'this' and raise it,
	// and handle resize to match display size.

	overlay = new SourcererItemOverlay(this);
	// We will resize overlay in resizeEvent

	enablePreviewButton = new QPushButton(tr("Enable Preview"), this);
	enablePreviewButton->setCursor(Qt::ArrowCursor);
	enablePreviewButton->hide();
	connect(enablePreviewButton, &QPushButton::clicked, [this]() { SetPreviewDisabled(false); });

	lockIconLabel = new QLabel(this);
	lockIconLabel->setText(QString::fromUtf8("🔒"));
	lockIconLabel->setAlignment(Qt::AlignCenter);
	// Small background to ensure visibility
	lockIconLabel->setStyleSheet(
		"QLabel { color: white; background-color: rgba(0, 0, 0, 150); border-radius: 4px; padding: 2px; }");
	lockIconLabel->adjustSize();
	lockIconLabel->hide();
	lockIconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

	visIconLabel = new QLabel(this);
	visIconLabel->setText(QString::fromUtf8("❌"));
	visIconLabel->setAlignment(Qt::AlignCenter);
	visIconLabel->setStyleSheet(
		"QLabel { color: white; background-color: rgba(0, 0, 0, 150); border-radius: 4px; padding: 2px; }");
	visIconLabel->adjustSize();
	visIconLabel->hide();
	visIconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

	sceneItemCountLabel = new QLabel(this);
	sceneItemCountLabel->setAlignment(Qt::AlignCenter);
	sceneItemCountLabel->setStyleSheet(
		"QLabel { color: white; background-color: rgba(0, 0, 0, 150); border-radius: 4px; padding: 2px 4px; font-weight: bold; font-size: 10px; }");
	sceneItemCountLabel->hide();
	sceneItemCountLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

	label = new QLabel(this);
	label->setAlignment(Qt::AlignCenter);
	label->setWordWrap(true);

	layout->addWidget(display);
	layout->addWidget(label);

	UpdateName();
	UpdateStatus(); // Initial status for button text/colors?
	UpdateOverlayButtonState();
	SetupOverlayConnections();

	auto OnDisplayCreated = [this](OBSQTDisplay *w) {
		if (w != display)
			return;
		obs_display_add_draw_callback(display->GetDisplay(), &DrawPreview, this);
	};

	connect(display, &OBSQTDisplay::DisplayCreated, this, OnDisplayCreated);
	display->CreateDisplay();

	signal_handler_t *sh = obs_source_get_signal_handler(source);
	signal_handler_connect(sh, "rename", SourceRenamed, this);
	signal_handler_connect(sh, "enable", SourceEnabled, this);
	// FIXME: might not be a valid signal
	signal_handler_connect(sh, "disable", SourceDisabled, this);

	if (obs_scene_from_source(source)) {
		signal_handler_connect(sh, "item_add", SceneItemAdded, this);
		signal_handler_connect(sh, "item_remove", SceneItemRemoved, this);
	}

	// Enable mouse tracking for hover events
	setMouseTracking(true);
}

SourcererItem::~SourcererItem()
{
	if (display && display->GetDisplay()) {
		obs_display_remove_draw_callback(display->GetDisplay(), &DrawPreview, this);
	}

	if (!isPreviewDisabled) {
		obs_source_dec_showing(source);
	}

	signal_handler_t *sh = obs_source_get_signal_handler(source);
	signal_handler_disconnect(sh, "rename", SourceRenamed, this);
	signal_handler_disconnect(sh, "enable", SourceEnabled, this);
	// FIXME: might not be a valid signal
	signal_handler_disconnect(sh, "disable", SourceDisabled, this);

	if (obs_scene_from_source(source)) {
		signal_handler_disconnect(sh, "item_add", SceneItemAdded, this);
		signal_handler_disconnect(sh, "item_remove", SceneItemRemoved, this);
	}

	obs_source_release(source);
}

void SourcererItem::SetupOverlayConnections()
{
	if (!overlay)
		return;

	connect(overlay->btnVisibility, &QPushButton::clicked, [this]() {
		emit ToggleVisibilityRequested(this);
		// Optimistically toggle icon?
		// We'll wait for status update for now.
	});

	connect(overlay->btnLock, &QPushButton::clicked, [this]() { emit ToggleLockRequested(this); });

	connect(overlay->btnActive, &QPushButton::clicked, [this]() {
		if (source) {
			bool enabled = obs_source_enabled(source);
			obs_source_set_enabled(source, !enabled);
			// Status update comes via signal
		}
	});

	connect(overlay->btnInteract, &QPushButton::clicked, [this]() {
		if (source) {
			obs_frontend_open_source_interaction(source);
		}
	});

	connect(overlay->btnPlayPause, &QPushButton::clicked, [this]() {
		if (source) {
			auto state = obs_source_media_get_state(source);
			obs_source_media_play_pause(source, state == OBS_MEDIA_STATE_PLAYING);
		}
	});

	connect(overlay->btnProperties, &QPushButton::clicked, [this]() {
		if (source) {
			obs_frontend_open_source_properties(source);
		}
	});

	connect(overlay->btnFilters, &QPushButton::clicked, [this]() {
		if (source) {
			obs_frontend_open_source_filters(source);
		}
	});

	connect(overlay->btnDisablePreview, &QPushButton::clicked,
		[this]() { SetPreviewDisabled(!isPreviewDisabled); });
}

void SourcererItem::SetOverlayEnabled(bool enabled)
{
	if (isOverlayEnabled == enabled)
		return;
	isOverlayEnabled = enabled;
	if (!enabled && overlay) {
		overlay->hide(); // Hide immediately without animation if disabled
	}
}

void SourcererItem::SetBadgesHidden(bool hidden)
{
	if (badgesHidden == hidden)
		return;
	badgesHidden = hidden;
	UpdateBadgeVisibility();
}

void SourcererItem::UpdateBadgeVisibility()
{
	if (badgesHidden) {
		if (lockIconLabel)
			lockIconLabel->hide();
		if (visIconLabel)
			visIconLabel->hide();
		if (sceneItemCountLabel)
			sceneItemCountLabel->hide();
	} else {
		if (lockIconLabel)
			lockIconLabel->setVisible(isSceneItemLocked);
		if (visIconLabel)
			visIconLabel->setVisible(!isSceneItemVisible);

		// Count label is handled by UpdateSceneItemCount, but we might need to restore it
		// We can just re-run UpdateSceneItemCount logic or store the count.
		// UpdateSceneItemCount sets visibility based on count.
		// Let's call UpdateSceneItemCount to be safe? No, that enumerates items.
		// Let's just check the label text or visibility state logic.
		// Actually, `UpdateSceneItemCount` sets visibility. If we hid it, we need to know if we should show it.
		// We can re-call UpdateSceneItemCount() which is slightly expensive but correct.
		UpdateSceneItemCount();
	}
	UpdateIconLayout();
}

void SourcererItem::SetHasSceneContext(bool hasContext)
{
	hasSceneContext = hasContext;
	UpdateOverlayButtonState();
}

void SourcererItem::UpdateOverlayButtonState()
{
	if (!overlay)
		return;

	// Gather Source Flags
	uint32_t flags = 0;
	bool configurable = false;

	if (source) {
		flags = obs_source_get_output_flags(source);
		configurable = obs_source_configurable(source);
	}

	// 1. Visibility (Needs Scene Context)
	if (overlay->btnVisibility) {
		overlay->btnVisibility->setVisible(hasSceneContext);
	}

	// 2. Lock (Needs Scene Context)
	if (overlay->btnLock) {
		overlay->btnLock->setVisible(hasSceneContext);
	}

	// 3. Active (Source Enabled)
	if (overlay->btnActive) {
		bool isScene = obs_scene_from_source(source) != nullptr;
		overlay->btnActive->setVisible(!isScene);
	}

	// 4. Filters - Default shown
	// Always visible
	if (overlay->btnFilters) {
		overlay->btnFilters->setVisible(true);
	}

	// 5. Interact (OBS_SOURCE_INTERACTION)
	if (overlay->btnInteract) {
		overlay->btnInteract->setVisible(flags & OBS_SOURCE_INTERACTION);
	}

	// 6. Play/Pause (OBS_SOURCE_CONTROLLABLE_MEDIA)
	if (overlay->btnPlayPause) {
		overlay->btnPlayPause->setVisible(flags & OBS_SOURCE_CONTROLLABLE_MEDIA);
	}

	// 7. Properties (Configurable)
	if (overlay->btnProperties) {
		overlay->btnProperties->setVisible(configurable);
	}

	// 8. Disable Preview - Always visible
	if (overlay->btnDisablePreview) {
		overlay->btnDisablePreview->setVisible(true);
	}

	overlay->ReflowButtons();
}

void SourcererItem::SetAltPressed(bool pressed)
{
	if (isAltPressed == pressed)
		return;
	isAltPressed = pressed;
	UpdateOverlayVisibility();
}

void SourcererItem::enterEvent(QEnterEvent *event)
{
	isHovered = true;
	// Check modifiers immediately on enter
	isAltPressed = (event->modifiers() & Qt::AltModifier);
	UpdateOverlayVisibility();
	QWidget::enterEvent(event);
}

void SourcererItem::leaveEvent(QEvent *event)
{
	// Prevent overlay from disappearing if mouse is still technically within the item or its children
	// (e.g. over a button, label, or the overlay itself)
	if (const QWidget *w = QApplication::widgetAt(QCursor::pos()); w && (w == this || this->isAncestorOf(w))) {
		return;
	}

	isHovered = false;
	UpdateOverlayVisibility();
	QWidget::leaveEvent(event);
}

void SourcererItem::mouseMoveEvent(QMouseEvent *event)
{
	// Update modifiers while moving inside
	bool alt = (event->modifiers() & Qt::AltModifier);
	if (alt != isAltPressed) {
		isAltPressed = alt;
		UpdateOverlayVisibility();
	}
	QWidget::mouseMoveEvent(event);
}

void SourcererItem::UpdateOverlayVisibility()
{
	if (overlay) {
		bool show = isOverlayEnabled && isHovered && isAltPressed;
		overlay->SetVisibleAnimated(show);

		if (show) {
			// --- Visibility button ---
			if (overlay->btnVisibility) {
				overlay->btnVisibility->setText(isSceneItemVisible
									? QString::fromUtf8("👁")
									: QString::fromUtf8("❌"));
				overlay->btnVisibility->setAccessibleName(
					isSceneItemVisible ? "Hide Source" : "Show Source");
				overlay->btnVisibility->setToolTip(
					isSceneItemVisible ? "Hide Source" : "Show Source");
			}

			// --- Lock button ---
			if (overlay->btnLock) {
				overlay->btnLock->setText(isSceneItemLocked ? QString::fromUtf8("🔒")
									  : QString::fromUtf8("🔓"));
				overlay->btnLock->setAccessibleName(
					isSceneItemLocked ? "Unlock Source" : "Lock Source");
				overlay->btnLock->setToolTip(
					isSceneItemLocked ? "Unlock Source" : "Lock Source");
			}

			// --- Active toggle (colored background for state) ---
			if (overlay->btnActive) {
				overlay->btnActive->setStyleSheet(
					isSourceEnabled
						? "QPushButton {"
						  "  color: #a8ffb0; background-color: rgba(40, 160, 70, 60);"
						  "  border: 1px solid rgba(100, 220, 120, 80); border-radius: 6px;"
						  "  font-size: 13px; padding: 2px;"
						  "}"
						  "QPushButton:hover {"
						  "  background-color: rgba(40, 160, 70, 100);"
						  "  border: 1px solid rgba(100, 220, 120, 150);"
						  "}"
						  "QPushButton:pressed {"
						  "  background-color: rgba(40, 160, 70, 130);"
						  "}"
						  "QPushButton:focus {"
						  "  outline: none; border: 2px solid rgba(80, 160, 255, 200);"
						  "}"
						: "QPushButton {"
						  "  color: #ffaaaa; background-color: rgba(180, 50, 50, 60);"
						  "  border: 1px solid rgba(220, 80, 80, 80); border-radius: 6px;"
						  "  font-size: 13px; padding: 2px;"
						  "}"
						  "QPushButton:hover {"
						  "  background-color: rgba(180, 50, 50, 100);"
						  "  border: 1px solid rgba(220, 80, 80, 150);"
						  "}"
						  "QPushButton:pressed {"
						  "  background-color: rgba(180, 50, 50, 130);"
						  "}"
						  "QPushButton:focus {"
						  "  outline: none; border: 2px solid rgba(80, 160, 255, 200);"
						  "}");
				overlay->btnActive->setAccessibleName(
					isSourceEnabled ? "Deactivate Source" : "Activate Source");
				overlay->btnActive->setToolTip(
					isSourceEnabled ? "Deactivate Source" : "Activate Source");
			}

			// --- Disable Preview button ---
			if (overlay->btnDisablePreview) {
				overlay->btnDisablePreview->setText(
					isPreviewDisabled ? QString::fromUtf8("👁")
							 : QString::fromUtf8("🚫"));
				overlay->btnDisablePreview->setAccessibleName(
					isPreviewDisabled ? "Enable Preview" : "Disable Preview");
				overlay->btnDisablePreview->setToolTip(
					isPreviewDisabled ? "Enable Preview" : "Disable Preview");
			}
		}
	}
}

void SourcererItem::SetSceneItem(obs_sceneitem_t *item)
{
	sceneItem = item;
}

void SourcererItem::SetItemWidth(int width)
{
	if (display) {
		int height = display->heightForWidth(width);
		display->setFixedSize(width, height);
		// Force update layout immediately after size change
		QTimer::singleShot(0, this, &SourcererItem::UpdateIconLayout);
	}
}

void SourcererItem::UpdateName()
{
	if (source) {
		const char *name = obs_source_get_name(source);
		label->setText(QString::fromUtf8(name));
	}
}

void SourcererItem::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	if (display) {
		// Center buttons
		if (enablePreviewButton) {
			enablePreviewButton->move(display->x() + (display->width() - enablePreviewButton->width()) / 2,
						  display->y() +
							  (display->height() - enablePreviewButton->height()) / 2);
		}
		// Resize overlay to cover display exactly
		if (overlay) {
			overlay->setGeometry(display->geometry());
			// overlay->raise(); // Handled in UpdateIconLayout
		}
		// UpdateIconLayout(); // Now handled via eventFilter on display
	}
}

void SourcererItem::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	// Ensure icons are positioned correctly when first shown
	QTimer::singleShot(0, this, &SourcererItem::UpdateIconLayout);
}

bool SourcererItem::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == display) {
		if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
			UpdateIconLayout();
		}
	}
	return QWidget::eventFilter(obj, event);
}

void SourcererItem::UpdateIconLayout()
{
	if (!display)
		return;

	int margin = 2;
	int spacing = 2;
	int currentX = display->x() + display->width() - margin;
	int y = display->y() + margin;

	// Scene Item Count (Rightmost)
	if (sceneItemCountLabel && sceneItemCountLabel->isVisible()) {
		currentX -= sceneItemCountLabel->width();
		sceneItemCountLabel->move(currentX, y);
		sceneItemCountLabel->raise();
		currentX -= spacing;
	}

	// Lock icon (Left of Count)
	if (lockIconLabel && lockIconLabel->isVisible()) {
		currentX -= lockIconLabel->width();
		lockIconLabel->move(currentX, y);
		lockIconLabel->raise();
		currentX -= spacing;
	}

	// Visibility icon (Left of Lock)
	if (visIconLabel && visIconLabel->isVisible()) {
		currentX -= visIconLabel->width();
		visIconLabel->move(currentX, y);
		visIconLabel->raise();
	}

	// Ensure overlay is on top of everything
	if (overlay) {
		overlay->raise();
	}
}

void SourcererItem::UpdateSceneItemCount()
{
	if (!source || !sceneItemCountLabel)
		return;

	obs_scene_t *scene = obs_scene_from_source(source);
	if (!scene) {
		sceneItemCountLabel->hide();
		return;
	}

	int count = 0;
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *, void *param) {
			int *c = (int *)param;
			(*c)++;
			return true;
		},
		&count);

	if (count > 0) {
		sceneItemCountLabel->setText(QString::number(count));
		sceneItemCountLabel->adjustSize();
		sceneItemCountLabel->setVisible(!badgesHidden);
	} else {
		sceneItemCountLabel->hide();
	}

	UpdateIconLayout();
	emit SceneItemCountChanged(this, count);
}

void SourcererItem::SetPreviewDisabled(bool disabled)
{
	if (isPreviewDisabled == disabled)
		return;
	isPreviewDisabled = disabled;

	if (disabled) {
		obs_source_dec_showing(source);
	} else {
		obs_source_inc_showing(source);
	}

	if (enablePreviewButton) {
		enablePreviewButton->setVisible(disabled);
		if (disabled)
			enablePreviewButton->raise();
	}

	if (display) {
		display->update();
	}
}

void SourcererItem::SetSelected(bool selected)
{
	if (isSelected == selected)
		return;
	isSelected = selected;
	update();
}

void SourcererItem::SetProgram(bool program)
{
	if (isProgram == program)
		return;
	isProgram = program;
	update();
}

void SourcererItem::SetFTB(bool ftb)
{
	if (isFTB == ftb)
		return;
	isFTB = ftb;
	update();
}

void SourcererItem::SetSceneItemVisible(bool visible)

{
	if (isSceneItemVisible == visible)
		return;
	isSceneItemVisible = visible;
	UpdateStatus();
	// Also update overlay icon if visible
	if (overlay && overlay->isVisible()) {
		overlay->btnVisibility->setText(isSceneItemVisible ? QString::fromUtf8("👁") : QString::fromUtf8("❌"));
	}

	if (visIconLabel) {
		visIconLabel->setVisible(!isSceneItemVisible && !badgesHidden);
	}
	UpdateIconLayout();
}

void SourcererItem::SetSceneItemLocked(bool locked)
{
	if (isSceneItemLocked == locked)
		return;
	isSceneItemLocked = locked;
	// Update overlay
	if (overlay && overlay->isVisible()) {
		overlay->btnLock->setText(isSceneItemLocked ? QString::fromUtf8("🔒") : QString::fromUtf8("🔓"));
	}
	if (lockIconLabel) {
		lockIconLabel->setVisible(isSceneItemLocked && !badgesHidden);
	}
	UpdateIconLayout();
	update();
}

void SourcererItem::UpdateStatus()
{
	bool active = isSourceEnabled && isSceneItemVisible;
	if (label) {
		label->setEnabled(active);
	}
	UpdateSceneItemCount();
	update();
}

void SourcererItem::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);

	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	QRect r = rect().adjusted(1, 1, -1, -1);
	int radius = 4;
	int borderWidth = 1;
	QColor borderColor = palette().color(QPalette::Mid);

	if (isProgram && isSelected) {
		p.setPen(QPen(Qt::blue, 2));
		p.drawRoundedRect(r, radius, radius);
		p.setPen(QPen(isFTB ? Qt::yellow : Qt::red, 2));
		p.drawRoundedRect(r.adjusted(2, 2, -2, -2), radius - 1, radius - 1);
		return;
	} else if (isProgram) {
		borderColor = isFTB ? Qt::yellow : Qt::red;
		borderWidth = 4;
		p.setPen(QPen(borderColor, borderWidth));
		p.drawRoundedRect(r, radius, radius);
	} else if (isSelected) {

		borderColor = Qt::blue;
		borderWidth = 4;
		p.setPen(QPen(borderColor, borderWidth));
		p.drawRoundedRect(r, radius, radius);
	} else {
		borderColor = QColor(60, 60, 60);
		borderWidth = 1;
		p.setPen(QPen(borderColor, borderWidth));
		p.drawRoundedRect(r, radius, radius);
	}

	// Draw locked icon if needed: Handled by lockIconLabel now
}

void SourcererItem::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		emit Clicked(this, event->modifiers());
		event->accept();
		return;
	}
	QWidget::mousePressEvent(event);
}

void SourcererItem::mouseDoubleClickEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		emit DoubleClicked(this);
		event->accept();
		return;
	}
	QWidget::mouseDoubleClickEvent(event);
}

void SourcererItem::contextMenuEvent(QContextMenuEvent *event)
{
	QMenu menu(this);

	QAction *renameAction = menu.addAction(tr("Rename"));
	connect(renameAction, &QAction::triggered, [this]() {
		if (!source)
			return;
		const char *oldName = obs_source_get_name(source);
		bool ok;
		QString newName = QInputDialog::getText(this, tr("Rename Source"), tr("Name:"), QLineEdit::Normal,
							QString::fromUtf8(oldName), &ok);
		if (ok && !newName.isEmpty()) {
			obs_source_set_name(source, newName.toUtf8().constData());
			UpdateName();
		}
	});

	QAction *deleteAction = menu.addAction(tr("Delete"));
	connect(deleteAction, &QAction::triggered, [this]() {
		if (!source)
			return;

		QString name = QString::fromUtf8(obs_source_get_name(source));
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(this, tr("Delete Source"),
					      tr("Are you sure you want to delete '%1'?").arg(name),
					      QMessageBox::Yes | QMessageBox::No);
		if (reply == QMessageBox::Yes) {
			obs_source_remove(source);
		}
	});

	menu.addSeparator();

	QAction *filtersAction = menu.addAction(tr("Filters"));
	connect(filtersAction, &QAction::triggered, [this]() {
		if (source)
			obs_frontend_open_source_filters(source);
	});

	QAction *propsAction = menu.addAction(tr("Properties"));
	connect(propsAction, &QAction::triggered, [this]() {
		if (source)
			obs_frontend_open_source_properties(source);
	});

	menu.addSeparator();

	QAction *windowedProjAction = menu.addAction(tr("Windowed Projector (Source)"));
	connect(windowedProjAction, &QAction::triggered, [this]() {
		if (source) {
			const char *name = obs_source_get_name(source);
			obs_frontend_open_projector("Source", -1, nullptr, name);
		}
	});

	QMenu *fsProjMenu = menu.addMenu(tr("Fullscreen Projector (Source)"));
	const auto &screens = QGuiApplication::screens();
	for (int i = 0; i < screens.size(); i++) {
		QScreen *screen = screens[i];
		QString label = QString("Display %1").arg(i + 1);

		QAction *action = fsProjMenu->addAction(label);
		connect(action, &QAction::triggered, [this, i]() {
			if (source) {
				const char *name = obs_source_get_name(source);
				obs_frontend_open_projector("Source", i, nullptr, name);
			}
		});
	}

	QAction *screenshotAction = menu.addAction(tr("Screenshot (Source)"));
	connect(screenshotAction, &QAction::triggered, [this]() {
		if (source)
			obs_frontend_take_source_screenshot(source);
	});

	menu.addSeparator();

	QAction *disablePreviewAction = menu.addAction(tr("Disable Preview"));
	disablePreviewAction->setCheckable(true);
	disablePreviewAction->setChecked(isPreviewDisabled);
	connect(disablePreviewAction, &QAction::toggled, [this](bool checked) { SetPreviewDisabled(checked); });

	emit MenuRequested(this, &menu);

	menu.exec(event->globalPos());
}

void SourcererItem::DrawPreview(void *data, uint32_t cx, uint32_t cy)
{
	SourcererItem *item = static_cast<SourcererItem *>(data);
	if (!item || !item->source)
		return;

	if (item->isPreviewDisabled) {
		return;
	}

	obs_source_t *source = item->source;
	uint32_t sourceCX = obs_source_get_width(source);
	uint32_t sourceCY = obs_source_get_height(source);

	if (!sourceCX || !sourceCY)
		return;

	float scaleX = (float)cx / (float)sourceCX;
	float scaleY = (float)cy / (float)sourceCY;
	float scale = (scaleX < scaleY) ? scaleX : scaleY;

	float newWidth = (float)sourceCX * scale;
	float newHeight = (float)sourceCY * scale;
	float x = ((float)cx - newWidth) * 0.5f;
	float y = ((float)cy - newHeight) * 0.5f;

	gs_matrix_push();
	gs_matrix_translate3f(x, y, 0.0f);
	gs_matrix_scale3f(scale, scale, 1.0f);

	obs_source_video_render(source);

	gs_matrix_pop();
}

void SourcererItem::SourceRenamed(void *data, calldata_t *cd)
{
	Q_UNUSED(cd);
	SourcererItem *item = static_cast<SourcererItem *>(data);
	QMetaObject::invokeMethod(item, "UpdateName", Qt::QueuedConnection);
}

void SourcererItem::SourceEnabled(void *data, calldata_t *cd)
{
	Q_UNUSED(cd);
	SourcererItem *item = static_cast<SourcererItem *>(data);
	QMetaObject::invokeMethod(
		item,
		[item]() {
			item->isSourceEnabled = true;
			item->UpdateStatus();
		},
		Qt::QueuedConnection);
}

void SourcererItem::SourceDisabled(void *data, calldata_t *cd)
{
	Q_UNUSED(cd);
	SourcererItem *item = static_cast<SourcererItem *>(data);
	QMetaObject::invokeMethod(
		item,
		[item]() {
			item->isSourceEnabled = false;
			item->UpdateStatus();
		},
		Qt::QueuedConnection);
}

void SourcererItem::SceneItemAdded(void *data, calldata_t *cd)
{
	Q_UNUSED(cd);
	SourcererItem *item = static_cast<SourcererItem *>(data);
	QMetaObject::invokeMethod(item, [item]() { item->UpdateSceneItemCount(); }, Qt::QueuedConnection);
}

void SourcererItem::SceneItemRemoved(void *data, calldata_t *cd)
{
	Q_UNUSED(cd);
	SourcererItem *item = static_cast<SourcererItem *>(data);
	QMetaObject::invokeMethod(item, [item]() { item->UpdateSceneItemCount(); }, Qt::QueuedConnection);
}
