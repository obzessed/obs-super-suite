#pragma once

#include <obs.h>
#include <obs-module.h>

#include <QMainWindow>
#include <QEvent>
#include <QLabel>
#include <QCloseEvent>
#include <QDockWidget> // Added this include for QDockWidget

class SecondaryWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit SecondaryWindow(int index, QWidget *parent = nullptr);
	~SecondaryWindow() override;
	
	void reparentDock(QDockWidget *dock);

protected:
	void contextMenuEvent(QContextMenuEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

private:
	void setupUi();
	void checkCentralWidgetVisibility();
	
	QLabel *instructionLabel = nullptr;
};
