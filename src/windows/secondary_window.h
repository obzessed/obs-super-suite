#pragma once

#include <obs.h>
#include <obs-module.h>

#include <QMainWindow>
#include <QEvent>
#include <QLabel>
#include <QCloseEvent>
#include <QDockWidget>

class DockWindowManager;

class SecondaryWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit SecondaryWindow(int index, DockWindowManager *manager, QWidget *parent = nullptr);
	~SecondaryWindow() override;
	
	void reparentDock(QDockWidget *dock);

protected:
	void contextMenuEvent(QContextMenuEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

public:
	Q_INVOKABLE void toggleVisibility(bool pressed);
	static void hotkey_callback(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

	void setShowDockTitles(bool visible);
	bool getShowDockTitles() const { return showDockTitles; }

private:
	void setupUi();
	void checkCentralWidgetVisibility();
	
	QLabel *instructionLabel = nullptr;
	obs_hotkey_id hotkeyId;
	DockWindowManager *manager = nullptr;
	bool showDockTitles = true;
	
private slots:
	void onDockTopLevelChanged(bool topLevel);
};
