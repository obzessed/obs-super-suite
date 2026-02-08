#include "secondary_window.h"

#include <QTextEdit>
#include <QDockWidget>
#include <QTableWidget>
#include <QListWidget>
#include <QCalendarWidget>
#include <QLabel>

#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QApplication>

#include <plugin-support.h>
#include <obs-frontend-api.h>

SecondaryWindow::SecondaryWindow(int index, QWidget *parent)
	: QMainWindow(parent)
{
	setWindowTitle(QString(obs_module_text("SecondaryWindow.Title")) + " " + QString::number(index + 1));
	setObjectName(QString("SuperSuiteSecondaryWindow%1").arg(index + 1));
	setWindowFlags(Qt::Window);
	resize(1280, 720);
	
	// Enable docking features
	setDockOptions(QMainWindow::AnimatedDocks | 
	               QMainWindow::AllowNestedDocks | 
	               QMainWindow::AllowTabbedDocks | 
	               QMainWindow::GroupedDragging);

	setupUi();
}

SecondaryWindow::~SecondaryWindow()
{
}

void SecondaryWindow::setupUi()
{	
	// Create central widget with instructions
	instructionLabel = new QLabel(
		"Right click to import a dock\nfrom another window.",
		this
	);
	instructionLabel->setAlignment(Qt::AlignCenter);
	instructionLabel->setStyleSheet(
		"QLabel {"
		"  color: #888;"
		"  font-size: 16px;"
		"  background-color: transparent;"
		"}"
	);
	
	setCentralWidget(instructionLabel);
}

void SecondaryWindow::checkCentralWidgetVisibility()
{
	if (!instructionLabel) return;

	bool hasDockedWidgets = false;
	QList<QDockWidget *> docks = findChildren<QDockWidget *>();
	for (QDockWidget *dock : docks) {
		if (dock->isVisible() && !dock->isFloating()) {
			hasDockedWidgets = true;
			break;
		}
	}
	
	instructionLabel->setVisible(!hasDockedWidgets);
}

void SecondaryWindow::contextMenuEvent(QContextMenuEvent *event)
{
	QMenu menu(this);

	// Fullscreen toggle
	QAction *fullscreenAction = menu.addAction("Fullscreen");
	fullscreenAction->setCheckable(true);
	fullscreenAction->setChecked(isFullScreen());
	connect(fullscreenAction, &QAction::triggered, [this](bool checked) {
		if (checked) {
			showFullScreen();
		} else {
			showNormal();
		}
	});

	menu.addSeparator();
	
	// Add "Import Dock" submenu
	QMenu *importMenu = menu.addMenu("Import Dock");
	
	QMainWindow *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	bool foundAny = false;

	if (mainWindow) {
		QList<QDockWidget *> allDocks = mainWindow->findChildren<QDockWidget *>();
		
		// Map window -> list of docks
		QMap<QWidget *, QList<QDockWidget *>> docksByWindow;
		
		for (QDockWidget *dock : allDocks) {
			if (!dock->isVisible()) continue;
			
			// We want to group by the WINDOW the dock is currently in
			QWidget *topLevel = dock->window();
			
			// If floating, use its parent (which should be the main/secondary window)
			if (dock->isFloating() && dock->parentWidget()) {
				topLevel = dock->parentWidget()->window();
			}

			docksByWindow[topLevel].append(dock);
		}

		// Helper to add docks for a specific window
		auto addDocksForWindow = [&](QWidget *window, const QString &headerTitle) {
			if (!docksByWindow.contains(window)) return;
			
			QList<QDockWidget *> docks = docksByWindow[window];
			if (docks.isEmpty()) return;

			QMenu *windowMenu = importMenu->addMenu(headerTitle);

			for (QDockWidget *dock : docks) {
				QString title = dock->windowTitle().isEmpty() ? dock->objectName() : dock->windowTitle();
				
				if (dock->isFloating()) {
					title += " (undocked)";
				}
				
				QAction *action = windowMenu->addAction(title);
				
				bool alreadyHere = (dock->window() == this);
				
				if (alreadyHere) {
					action->setEnabled(false);
					action->setText(title + " (Current Window)");
				} else {
					connect(action, &QAction::triggered, [this, dock]() {
						reparentDock(dock);
					});
					foundAny = true;
				}
			}
		};

		// 1. Add Main Window first
		addDocksForWindow(mainWindow, "Main OBS Window");
		docksByWindow.remove(mainWindow);

		// 2. Add remaining windows (Secondary Windows)
		// We use an iterator to go through the rest
		for (auto it = docksByWindow.begin(); it != docksByWindow.end(); ++it) {
			QWidget *win = it.key();
			if (win == this) {
				addDocksForWindow(win, "Current Window");
			} else {
				QString title = win->windowTitle();
				if (title.isEmpty()) title = "Other Window";
				addDocksForWindow(win, title);
			}
		}
	}
	
	if (!foundAny) {
		importMenu->addAction("No importable docks found")->setEnabled(false);
		importMenu->addSeparator();
		importMenu->addAction("Hint: Enable docks in View -> Docks")->setEnabled(false);
		importMenu->addAction("      and ensure they are docked in main window.")->setEnabled(false);
	}

	menu.addSeparator();

	// Add "Send Dock to..." submenu
	QMenu *sendMenu = menu.addMenu("Send Dock to Window");
	
	// Find docks belonging to THIS window
	QList<QDockWidget *> myDocks;
	for (QDockWidget *dock : findChildren<QDockWidget *>()) {
		if (dock->isVisible()) {
			myDocks.append(dock);
		}
	}

	if (myDocks.isEmpty()) {
		sendMenu->addAction("No docks in this window")->setEnabled(false);
	} else {
		// Find potential targets
		QList<QMainWindow *> targets;
		targets.append(mainWindow); // Main Window

		// Find other SecondaryWindows
		if (mainWindow) {
			QList<SecondaryWindow *> secWindows = mainWindow->findChildren<SecondaryWindow *>();
			for (SecondaryWindow *sec : secWindows) {
				if (sec != this && sec->isVisible()) {
					targets.append(sec);
				}
			}
		}

		for (QDockWidget *dock : myDocks) {
			QString localTitle = dock->windowTitle().isEmpty() ? dock->objectName() : dock->windowTitle();
			if (dock->isFloating()) {
				localTitle += " (undocked)";
			}
			QMenu *dockSubMenu = sendMenu->addMenu(localTitle);

			for (QMainWindow *target : targets) {
				QString targetName = (target == mainWindow) ? "Main OBS Window" : target->windowTitle();
				
				connect(dockSubMenu->addAction(targetName), &QAction::triggered, [this, dock, target]() {
					// Check if target is another SecondaryWindow
					if (auto *secTarget = qobject_cast<SecondaryWindow*>(target)) {
						secTarget->reparentDock(dock);
					} else {
						// Fallback for Main Window or others
						dock->setParent(target);
						target->addDockWidget(Qt::RightDockWidgetArea, dock);
						dock->setFloating(false);
						dock->show();
					}
					
					// Check our own visibility after moving dock out
					checkCentralWidgetVisibility();
				});
			}
		}
	}

	menu.exec(event->globalPos());
}

void SecondaryWindow::reparentDock(QDockWidget *dock)
{
	if (!dock) return;
	
	// Remove from main window area (it's already a child, but let's be safe)
	dock->setParent(this);
	addDockWidget(Qt::RightDockWidgetArea, dock);
	dock->setFloating(false);
	dock->show();
	
	// Monitor this dock for visibility changes
	connect(dock, &QDockWidget::visibilityChanged, this, [this](bool) {
		checkCentralWidgetVisibility();
	});
	connect(dock, &QDockWidget::topLevelChanged, this, [this](bool) {
		checkCentralWidgetVisibility();
	});

	checkCentralWidgetVisibility();
}

void SecondaryWindow::closeEvent(QCloseEvent *event)
{
	// Just hide instead of closing to preserve dock state? 
	// Or actually close and let caller handle show/hide.
	// For now, standard behavior.
	QMainWindow::closeEvent(event);
}
