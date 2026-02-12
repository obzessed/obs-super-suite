#pragma once

#include <QWidget>
#include <QLabel>
#include <obs.hpp>
#include "../utils/widgets/qt-display.hpp"

class QMenu;
class QContextMenuEvent;
class QPushButton;
class QGridLayout;
class QPropertyAnimation;
class QGraphicsOpacityEffect;

class SourcererItemOverlay : public QWidget {
	Q_OBJECT

public:
	explicit SourcererItemOverlay(QWidget *parent = nullptr);

	void SetVisibleAnimated(bool visible);

	// Buttons
	QPushButton *btnVisibility = nullptr;
	QPushButton *btnLock = nullptr;
	QPushButton *btnActive = nullptr;
	QPushButton *btnInteract = nullptr;
	QPushButton *btnProperties = nullptr;
	QPushButton *btnPlayPause = nullptr;
	QPushButton *btnFilters = nullptr;
	QPushButton *btnDisablePreview = nullptr;

	void ReflowButtons();

private:
	QGridLayout *layout = nullptr;
	QPropertyAnimation *fadeAnim = nullptr;
	QGraphicsOpacityEffect *opacityEffect = nullptr;

	void SetupButton(QPushButton *btn, const QString &text, const QString &tooltip);

protected:
	void mousePressEvent(QMouseEvent *event) override;
};

class SourcererItem : public QWidget {

	Q_OBJECT

public:
	explicit SourcererItem(obs_source_t *source, QWidget *parent = nullptr);
	~SourcererItem();

	void UpdateName();
	void SetItemWidth(int width);
	obs_source_t *GetSource() const { return source; }
	void SetSelected(bool selected);
	bool IsSelected() const { return isSelected; }
	void SetProgram(bool program);
	void SetFTB(bool ftb);

	void SetSceneItemVisible(bool visible);

	void SetSceneItemLocked(bool locked);
	void SetPreviewDisabled(bool disabled);
	void UpdateStatus();

	// Called by parent dock when global key state changes or config changes
	void SetAltPressed(bool pressed);
	// Helper to set context (e.g. if we are in a scene)
	void SetHasSceneContext(bool hasContext);
	void SetOverlayEnabled(bool enabled);
	void SetBadgesHidden(bool hidden);

	void UpdateOverlayButtonState();

	void SetSceneItem(obs_sceneitem_t *item);
	obs_sceneitem_t *GetSceneItem() const { return sceneItem; }

signals:
	void Clicked(SourcererItem *item, Qt::KeyboardModifiers modifiers);
	void DoubleClicked(SourcererItem *item);

	void MenuRequested(SourcererItem *item, QMenu *menu);

	// Overlay signals
	void ToggleVisibilityRequested(SourcererItem *item);
	void ToggleLockRequested(SourcererItem *item);
	void SceneItemCountChanged(SourcererItem *item, int count);
	// Active/Interact/PlayPause can be handled internally or signaled.
	// We'll signal generic requests that might need scene context,
	// others can be internal but signal is safer for undo/sync.
	// Actually, Active/Interact/PlayPause act on SOURCE, not scene item.
	// But let's expose them for consistency?
	// No, Source actions can be done here. Scene Item actions need parent.

protected:
	void resizeEvent(QResizeEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	bool eventFilter(QObject *obj, QEvent *event) override;

private:
	obs_source_t *source = nullptr;
	obs_sceneitem_t *sceneItem = nullptr;
	OBSQTDisplay *display = nullptr;
	QLabel *label = nullptr;
	QLabel *lockIconLabel = nullptr;
	QLabel *visIconLabel = nullptr;
	QLabel *sceneItemCountLabel = nullptr;
	QPushButton *enablePreviewButton = nullptr;
	SourcererItemOverlay *overlay = nullptr;

	bool isSelected = false;
	bool isProgram = false;
	bool isFTB = false;

	bool isSceneItemVisible = true;

	bool isSceneItemLocked = false;
	bool isSourceEnabled = true;
	bool isPreviewDisabled = false;

	bool hasSceneContext = false;
	bool isHovered = false;
	bool isAltPressed = false;
	bool isOverlayEnabled = true;
	bool badgesHidden = false;

	void UpdateOverlayVisibility();
	void UpdateIconLayout();
	void UpdateBadgeVisibility();
	void SetupOverlayConnections();
	void UpdateSceneItemCount();

	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);

	static void SourceRenamed(void *data, calldata_t *cd);
	static void SourceEnabled(void *data, calldata_t *cd);
	static void SourceDisabled(void *data, calldata_t *cd);
	static void SceneItemAdded(void *data, calldata_t *cd);
	static void SceneItemRemoved(void *data, calldata_t *cd);
};
