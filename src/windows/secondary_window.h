#pragma once

#include <obs.h>
#include <obs-module.h>

#include <QMainWindow>
#include <QEvent>
#include <QLabel>
#include <QCloseEvent>

class SecondaryWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit SecondaryWindow(int index, QWidget *parent = nullptr);
	~SecondaryWindow() override;

protected:
	void contextMenuEvent(QContextMenuEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

private:
	void setupUi();
	void reparentDock(QDockWidget *dock);
	void checkCentralWidgetVisibility();
	
	QLabel *instructionLabel = nullptr;
};
