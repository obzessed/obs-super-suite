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
	QPushButton *btnRefresh = nullptr;
	QPushButton *btnPlayPause = nullptr;

private:
	QGridLayout *layout = nullptr;
	QPropertyAnimation *fadeAnim = nullptr;
	QGraphicsOpacityEffect *opacityEffect = nullptr;

	void SetupButton(QPushButton *btn, const QString &text, const QString &tooltip);
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
	void SetProgram(bool program);
	void SetSceneItemVisible(bool visible);
	void SetSceneItemLocked(bool locked);
	void SetPreviewDisabled(bool disabled);
	void UpdateStatus();

	// Called by parent dock when global key state changes or config changes
	void SetCtrlPressed(bool pressed);
	// Helper to set context (e.g. if we are in a scene)
	void SetHasSceneContext(bool hasContext);
	void SetOverlayEnabled(bool enabled);

signals:
	void Clicked(SourcererItem *item);
	void DoubleClicked(SourcererItem *item);
	void MenuRequested(SourcererItem *item, QMenu *menu);

	// Overlay signals
	void ToggleVisibilityRequested(SourcererItem *item);
	void ToggleLockRequested(SourcererItem *item);
	// Active/Interact/PlayPause can be handled internally or signaled.
	// We'll signal generic requests that might need scene context,
	// others can be internal but signal is safer for undo/sync.
	// Actually, Active/Interact/PlayPause act on SOURCE, not scene item.
	// But let's expose them for consistency?
	// No, Source actions can be done here. Scene Item actions need parent.

protected:
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;

private:
	obs_source_t *source = nullptr;
	OBSQTDisplay *display = nullptr;
	QLabel *label = nullptr;
	QPushButton *enablePreviewButton = nullptr;
	SourcererItemOverlay *overlay = nullptr;

	bool isSelected = false;
	bool isProgram = false;

	bool isSceneItemVisible = true;
	bool isSceneItemLocked = false;
	bool isSourceEnabled = true;
	bool isPreviewDisabled = false;

	bool hasSceneContext = false;
	bool isHovered = false;
	bool isCtrlPressed = false;
	bool isOverlayEnabled = true;

	void UpdateOverlayVisibility();
	void SetupOverlayConnections();

	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);

	static void SourceRenamed(void *data, calldata_t *cd);
	static void SourceEnabled(void *data, calldata_t *cd);
	static void SourceDisabled(void *data, calldata_t *cd);
};
