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
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
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
	// Semi-transparent black background
	setAutoFillBackground(true);
	setAttribute(Qt::WA_TransparentForMouseEvents);
	QPalette pal = palette();
	pal.setColor(QPalette::Window, QColor(0, 0, 0, 150));
	setPalette(pal);

	layout = new QGridLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->setSpacing(4);
	layout->setAlignment(Qt::AlignCenter);

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

	btnRefresh = new QPushButton(this);
	SetupButton(btnRefresh, QString::fromUtf8("↻"),
		    "Refresh (Properties)"); // Using Refresh icon but tooltip mentions properties

	// Layout grid
	// Row 0: Vis, Lock
	// Row 1: Active, Interact
	// Row 2: Play, Refresh
	layout->addWidget(btnVisibility, 0, 0);
	layout->addWidget(btnLock, 0, 1);
	layout->addWidget(btnActive, 1, 0);
	layout->addWidget(btnInteract, 1, 1);
	layout->addWidget(btnPlayPause, 2, 0);
	layout->addWidget(btnRefresh, 2, 1);

	// Opacity Effect for Animation
	opacityEffect = new QGraphicsOpacityEffect(this);
	opacityEffect->setOpacity(0.0); // Start hidden
	setGraphicsEffect(opacityEffect);

	fadeAnim = new QPropertyAnimation(opacityEffect, "opacity", this);
	fadeAnim->setDuration(50); // ms
	fadeAnim->setEasingCurve(QEasingCurve::InOutQuad);

	hide(); // Initially hidden
}

void SourcererItemOverlay::SetupButton(QPushButton *btn, const QString &text, const QString &tooltip)
{
	btn->setText(text);
	btn->setToolTip(tooltip);
	btn->setFixedSize(30, 30);
	btn->setCursor(Qt::ArrowCursor);

	// Simple style
	btn->setStyleSheet("QPushButton { "
			   "  background-color: rgba(255, 255, 255, 30); "
			   "  border: 1px solid rgba(255, 255, 255, 50); "
			   "  color: white; "
			   "  border-radius: 4px; "
			   "  font-weight: bold; "
			   "}"
			   "QPushButton:hover { "
			   "  background-color: rgba(255, 255, 255, 80); "
			   "  border: 1px solid rgba(255, 255, 255, 150); "
			   "}"
			   "QPushButton:pressed { "
			   "  background-color: rgba(255, 255, 255, 100); "
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

	label = new QLabel(this);
	label->setAlignment(Qt::AlignCenter);
	label->setWordWrap(true);

	layout->addWidget(display);
	layout->addWidget(label);

	UpdateName();
	UpdateStatus(); // Initial status for button text/colors?
	SetupOverlayConnections();

	auto OnDisplayCreated = [this](OBSQTDisplay *w) {
		if (w != display)
			return;
		obs_display_add_draw_callback(display->GetDisplay(), SourcererItem::DrawPreview, this);
	};

	connect(display, &OBSQTDisplay::DisplayCreated, this, OnDisplayCreated);
	display->CreateDisplay();

	signal_handler_t *sh = obs_source_get_signal_handler(source);
	signal_handler_connect(sh, "rename", SourceRenamed, this);
	signal_handler_connect(sh, "enable", SourceEnabled, this);
	signal_handler_connect(sh, "disable", SourceDisabled, this);

	// Enable mouse tracking for hover events
	setMouseTracking(true);
}

SourcererItem::~SourcererItem()
{
	if (display && display->GetDisplay()) {
		obs_display_remove_draw_callback(display->GetDisplay(), SourcererItem::DrawPreview, this);
	}

	if (!isPreviewDisabled) {
		obs_source_dec_showing(source);
	}

	signal_handler_t *sh = obs_source_get_signal_handler(source);
	signal_handler_disconnect(sh, "rename", SourceRenamed, this);
	signal_handler_disconnect(sh, "enable", SourceEnabled, this);
	signal_handler_disconnect(sh, "disable", SourceDisabled, this);

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

	connect(overlay->btnRefresh, &QPushButton::clicked, [this]() {
		if (source) {
			obs_frontend_open_source_properties(source);
		}
	});
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

void SourcererItem::SetHasSceneContext(bool hasContext)
{
	hasSceneContext = hasContext;
	// Show/Hide relevant buttons on overlay
	if (overlay) {
		overlay->btnVisibility->setVisible(hasContext);
		overlay->btnLock->setVisible(hasContext);
	}
}

void SourcererItem::SetCtrlPressed(bool pressed)
{
	if (isCtrlPressed == pressed)
		return;
	isCtrlPressed = pressed;
	UpdateOverlayVisibility();
}

void SourcererItem::enterEvent(QEnterEvent *event)
{
	isHovered = true;
	// Check modifiers immediately on enter
	isCtrlPressed = (event->modifiers() & Qt::ControlModifier);
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
	bool ctrl = (event->modifiers() & Qt::ControlModifier);
	if (ctrl != isCtrlPressed) {
		isCtrlPressed = ctrl;
		UpdateOverlayVisibility();
	}
	QWidget::mouseMoveEvent(event);
}

void SourcererItem::UpdateOverlayVisibility()
{
	if (overlay) {
		bool show = isOverlayEnabled && isHovered && isCtrlPressed;
		overlay->SetVisibleAnimated(show);

		if (show) {
			// Update Icons based on state
			if (overlay->btnVisibility) {
				overlay->btnVisibility->setText(isSceneItemVisible
									? QString::fromUtf8("👁")
									: QString::fromUtf8("❌")); // Or crossed eye
			}
			if (overlay->btnLock) {
				overlay->btnLock->setText(isSceneItemLocked ? QString::fromUtf8("🔒")
									    : QString::fromUtf8("🔓"));
			}
			if (overlay->btnActive) {
				overlay->btnActive->setStyleSheet(
					isSourceEnabled
						? "QPushButton { color: #88ff88; font-weight: bold; background-color: rgba(0,0,0,50); border: 1px solid rgba(255,255,255,50); }"
						: "QPushButton { color: #ff8888; font-weight: bold; background-color: rgba(0,0,0,50); border: 1px solid rgba(255,255,255,50); }");
			}
		}
	}
}

void SourcererItem::SetItemWidth(int width)
{
	if (display) {
		int height = display->heightForWidth(width);
		display->setFixedSize(width, height);
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
			overlay->raise(); // Ensure on top
		}
	}
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
}

void SourcererItem::UpdateStatus()
{
	bool active = isSourceEnabled && isSceneItemVisible;
	if (label) {
		label->setEnabled(active);
	}
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
		p.setPen(QPen(Qt::red, 2));
		p.drawRoundedRect(r.adjusted(2, 2, -2, -2), radius - 1, radius - 1);
		return;
	} else if (isProgram) {
		borderColor = Qt::red;
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
}

void SourcererItem::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		emit Clicked(this);
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
