#include "secondary_window.h"

#include <QJsonArray>
#include <QTextEdit>
#include <QDockWidget>
#include <QTableWidget>
#include <QListWidget>
#include <QCalendarWidget>
#include <QMessageBox>
#include <QLabel>

#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QContextMenuEvent>
#include <QInputDialog>
#include <QDialog>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QLabel>
#include <QDateTime>
#include <QApplication>
#include "dock_window_manager.h"

#include <plugin-support.h>
#include <obs-frontend-api.h>

SecondaryWindow::SecondaryWindow(int index, DockWindowManager *manager, QWidget *parent)
	: QMainWindow(parent), manager(manager)
{
	setWindowTitle(QString(obs_module_text("SecondaryWindow.Title")) + " " + QString::number(index + 1));
	setObjectName(QString("SuperSuiteSecondaryWindow%1").arg(index + 1));
	resize(1280, 720);
	
	// Enable docking features
	setDockOptions(QMainWindow::AnimatedDocks | 
	               QMainWindow::AllowNestedDocks | 
	               QMainWindow::AllowTabbedDocks | 
	               QMainWindow::GroupedDragging);

	// Register Hotkey
	QString hkName = QString("Show/Hide '%1'").arg(windowTitle());
	QString hkDesc = QString("Toggle visibility of %1").arg(windowTitle());
	hotkeyId = obs_hotkey_register_frontend(
		QString("SuperSuite.SecondaryWindow.%1").arg(objectName()).toUtf8().constData(),
		hkDesc.toUtf8().constData(),
		SecondaryWindow::hotkey_callback,
		this
	);

	setupUi();
}

SecondaryWindow::~SecondaryWindow()
{
	if (hotkeyId != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(hotkeyId);
	}
}

void SecondaryWindow::setupUi()
{	
	// Create central widget with instructions
	instructionLabel = new QLabel(
		obs_module_text("SecondaryWindow.Instruction"),
		this
	);
	if (instructionLabel->text().isEmpty()) instructionLabel->setText("Right click to import a dock\nfrom another window.");
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

	// Stay on Top toggle
	QAction *stayOnTopAction = menu.addAction(obs_module_text("SecondaryWindow.Context.StayOnTop"));
	if (stayOnTopAction->text().isEmpty()) stayOnTopAction->setText("Stay on Top");
	stayOnTopAction->setCheckable(true);
	stayOnTopAction->setChecked(windowFlags() & Qt::WindowStaysOnTopHint);
	connect(stayOnTopAction, &QAction::triggered, [this](bool checked) {
		Qt::WindowFlags flags = windowFlags();
		if (checked) {
			flags |= Qt::WindowStaysOnTopHint;
		} else {
			flags &= ~Qt::WindowStaysOnTopHint;
		}
		setWindowFlags(flags);
		show();
	});

	// Fullscreen toggle
	QAction *fullscreenAction = menu.addAction(obs_module_text("SecondaryWindow.Context.Fullscreen"));
	if (fullscreenAction->text().isEmpty()) fullscreenAction->setText("Fullscreen");
	fullscreenAction->setCheckable(true);
	fullscreenAction->setChecked(isFullScreen());
	connect(fullscreenAction, &QAction::triggered, [this](bool checked) {
		if (checked) {
			showFullScreen();
		} else {
			showNormal();
		}
	});

	// Opacity Presets
	QMenu *opacityMenu = menu.addMenu(obs_module_text("SecondaryWindow.Context.WindowOpacity"));
	if (opacityMenu->title().isEmpty()) opacityMenu->setTitle("Window Opacity");
	const int opacities[] = {20, 40, 60, 80, 100};
	// Create action group to make them exclusive visually
	QActionGroup *opacityGroup = new QActionGroup(&menu);
	for (int val : opacities) {
		QString text = QString("%1%").arg(val);
		QAction *act = opacityMenu->addAction(text);
		act->setCheckable(true);
		act->setData(val);
		if (qRound(windowOpacity() * 100) == val) {
			act->setChecked(true);
		}
		opacityGroup->addAction(act);
		connect(act, &QAction::triggered, [this, val]() {
			setWindowOpacity(val / 100.0);
		});
	}

	menu.addSeparator();

	menu.addSeparator();

	// Show/Hide Dock Headers
	QAction *titlesAction = menu.addAction(QString(obs_module_text("SecondaryWindow.Context.ShowDockHeaders")));
	if (titlesAction->text().isEmpty()) titlesAction->setText("Show Dock Headers");
	titlesAction->setCheckable(true);
	titlesAction->setChecked(showDockTitles);
	connect(titlesAction, &QAction::triggered, [this](bool checked) {
		setShowDockTitles(checked);
	});

	// Layout Snapshots
	if (manager) {
		QMenu *snapshotMenu = menu.addMenu(obs_module_text("SecondaryWindow.Context.LayoutSnapshots"));
		if (snapshotMenu->title().isEmpty()) snapshotMenu->setTitle("Layout Snapshots");
		
		QAction *saveAction = snapshotMenu->addAction(obs_module_text("SecondaryWindow.Context.SaveLayout"));
		if (saveAction->text().isEmpty()) saveAction->setText("Save Current Layout...");
		connect(saveAction, &QAction::triggered, [this]() {
			QDialog dlg(this);
			dlg.setWindowTitle(obs_module_text("DockWindowManager.Snapshot.SaveTitle"));
			if (dlg.windowTitle().isEmpty()) dlg.setWindowTitle("Save Layout Snapshot");
			
			QVBoxLayout *layout = new QVBoxLayout(&dlg);
			
			// Name Input
			layout->addWidget(new QLabel("Snapshot Name:", &dlg));
			QLineEdit *nameEdit = new QLineEdit(&dlg);
			nameEdit->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
			nameEdit->selectAll();
			layout->addWidget(nameEdit);
			
			// Options
			QString savePrefix = "Save ";
			
			QCheckBox *chkPos = new QCheckBox(savePrefix + obs_module_text("DockWindowManager.Snapshot.Option.Pos"), &dlg);
			if (chkPos->text() == savePrefix) chkPos->setText("Save Window Position & Size");
			chkPos->setChecked(true);
			layout->addWidget(chkPos);
			
			QCheckBox *chkDocks = new QCheckBox(savePrefix + obs_module_text("DockWindowManager.Snapshot.Option.Docks"), &dlg);
			if (chkDocks->text() == savePrefix) chkDocks->setText("Save Docks Layout");
			chkDocks->setChecked(true);
			layout->addWidget(chkDocks);
			
			QCheckBox *chkOpacity = new QCheckBox(savePrefix + obs_module_text("DockWindowManager.Snapshot.Option.Opacity"), &dlg);
			if (chkOpacity->text() == savePrefix) chkOpacity->setText("Save Opacity");
			chkOpacity->setChecked(true);
			layout->addWidget(chkOpacity);
			
			QCheckBox *chkTop = new QCheckBox(savePrefix + obs_module_text("DockWindowManager.Snapshot.Option.StayOnTop"), &dlg);
			if (chkTop->text() == savePrefix) chkTop->setText("Save 'Stay on Top' State");
			chkTop->setChecked(true);
			layout->addWidget(chkTop);
			
			QCheckBox *chkTitles = new QCheckBox(savePrefix + obs_module_text("DockWindowManager.Snapshot.Option.DockHeaders"), &dlg);
			if (chkTitles->text() == savePrefix) chkTitles->setText("Save 'Show Dock Headers' State");
			chkTitles->setChecked(true);
			layout->addWidget(chkTitles);

			// Buttons
			QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
			connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
			connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
			layout->addWidget(btns);
			
			if (dlg.exec() == QDialog::Accepted && !nameEdit->text().isEmpty()) {
				QJsonObject winObj;
				
				if (chkPos->isChecked()) {
					winObj["geometry"] = QString(saveGeometry().toBase64());
					winObj["fullscreen"] = isFullScreen();
				}
				
				if (chkTop->isChecked()) {
					winObj["stayOnTop"] = (bool)(windowFlags() & Qt::WindowStaysOnTopHint);
				}
				
				if (chkTitles->isChecked()) {
					winObj["showDockTitles"] = showDockTitles;
				}

				if (chkOpacity->isChecked()) {
					winObj["opacity"] = windowOpacity();
				}
				
				if (chkDocks->isChecked()) {
					winObj["state"] = QString(saveState().toBase64());
					
					QJsonArray ownedDocks;
					QList<QDockWidget *> docks = findChildren<QDockWidget *>();
					for (QDockWidget *dock : docks) {
						if (dock->isVisible()) {
							if (!dock->objectName().isEmpty()) {
								ownedDocks.append(dock->objectName());
							}
						}
					}
					winObj["ownedDocks"] = ownedDocks;
				}
				
				manager->saveSnapshot(nameEdit->text(), winObj);
			}
		});
		
		snapshotMenu->addSeparator();
		
		QMenu *restoreMenu = snapshotMenu->addMenu(obs_module_text("SecondaryWindow.Context.Restore"));
		if (restoreMenu->title().isEmpty()) restoreMenu->setTitle("Restore");
		
		QStringList snapshots = manager->getSnapshotNames();
		if (snapshots.isEmpty()) {
			QAction *emptyAction = restoreMenu->addAction("(No Snapshots)");
			emptyAction->setEnabled(false);
		} else {
			for (const QString &name : snapshots) {
				QAction *restoreAction = restoreMenu->addAction(name);
				connect(restoreAction, &QAction::triggered, [this, name]() {
					manager->requestRestoreSnapshot(name, this);
				});
			}
		}
		
		
		QAction *deleteDlgAction = snapshotMenu->addAction(obs_module_text("SecondaryWindow.Context.DeleteSnapshot"));
		if (deleteDlgAction->text().isEmpty()) deleteDlgAction->setText("Delete Snapshot...");
		connect(deleteDlgAction, &QAction::triggered, [this]() {
			QStringList names = manager->getSnapshotNames();
			if (names.isEmpty()) {
				QMessageBox::information(this, 
					obs_module_text("SecondaryWindow.Context.DeleteSnapshot"), // Title matches menu item
					obs_module_text("DockWindowManager.Snapshot.NoSnapshots"));
				return;
			}
			
			QDialog dlg(this);
			dlg.setWindowTitle(obs_module_text("DockWindowManager.Snapshot.DeleteTitle"));
			if (dlg.windowTitle().isEmpty()) dlg.setWindowTitle("Delete Snapshots");

			QVBoxLayout *layout = new QVBoxLayout(&dlg);
			
			QString selectMsg = obs_module_text("DockWindowManager.Snapshot.SelectDelete");
			if (selectMsg.isEmpty()) selectMsg = "Select snapshots to delete:";
			layout->addWidget(new QLabel(selectMsg, &dlg));
			
			QListWidget *list = new QListWidget(&dlg);
			for (const QString &name : names) {
				QListWidgetItem *item = new QListWidgetItem(name, list);
				item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
				item->setCheckState(Qt::Unchecked);
			}
			layout->addWidget(list);
			
			QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
			connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
			connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
			layout->addWidget(btns);
			
			if (dlg.exec() == QDialog::Accepted) {
				QStringList toDelete;
				for (int i = 0; i < list->count(); i++) {
					QListWidgetItem *item = list->item(i);
					if (item->checkState() == Qt::Checked) {
						toDelete.append(item->text());
					}
				}
				
				if (!toDelete.isEmpty()) {
					QString title = obs_module_text("DockWindowManager.Snapshot.ConfirmDelete");
					if (title.isEmpty()) title = "Confirm Delete";
					
					QString msgFormat = obs_module_text("DockWindowManager.Snapshot.ConfirmDeleteMsg");
					if (msgFormat.isEmpty()) msgFormat = "Are you sure you want to delete %1 snapshot(s)?";

					if (QMessageBox::question(this, title, 
						msgFormat.arg(toDelete.size()),
						QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) 
					{
						for (const QString &name : toDelete) {
							manager->deleteSnapshot(name);
						}
					}
				}
			}
		});
	}

	menu.addSeparator();
	
	// Add "Import Dock" submenu
	QMenu *importMenu = menu.addMenu(obs_module_text("SecondaryWindow.Context.ImportDock"));
	if (importMenu->title().isEmpty()) importMenu->setTitle("Import Dock");
	
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
	QMenu *sendMenu = menu.addMenu(obs_module_text("SecondaryWindow.Context.SendDockTo"));
	if (sendMenu->title().isEmpty()) sendMenu->setTitle("Send Dock to Window");
	
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

	// Apply Dock Title visibility
	disconnect(dock, &QDockWidget::topLevelChanged, this, &SecondaryWindow::onDockTopLevelChanged);
	connect(dock, &QDockWidget::topLevelChanged, this, &SecondaryWindow::onDockTopLevelChanged, Qt::UniqueConnection);
	
	if (showDockTitles) {
		dock->setTitleBarWidget(nullptr);
	} else {
		// If docked, hide title
		if (!dock->isFloating()) {
			dock->setTitleBarWidget(new QWidget(dock));
		}
	}

	checkCentralWidgetVisibility();
}

void SecondaryWindow::closeEvent(QCloseEvent *event)
{
	QMainWindow *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (mainWindow) {
		initialDocks.clear();

		// Reparent all docks back to main window
		// We use findChildren to locate them. specific direct children check might be safer 
		// but typically docks are direct children or managed by QMainWindow layout.
		QList<QDockWidget *> docks = findChildren<QDockWidget *>();
		for (QDockWidget *dock : docks) {
			// Ensure we are only moving docks that we actually own/manage
			// and not some internal sub-widgets if any (unlikely for QDockWidget)
			if (dock->parent() == this || dock->window() == this) {
				if (!dock->objectName().isEmpty()) {
					initialDocks.append(dock->objectName());
				}

				dock->hide();
				dock->setParent(mainWindow);
				mainWindow->addDockWidget(Qt::LeftDockWidgetArea, dock);
				dock->setFloating(true);
			}
		}
	}

	QMainWindow::closeEvent(event);
}

void SecondaryWindow::hotkey_callback(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	
	SecondaryWindow *win = static_cast<SecondaryWindow*>(data);
	if (pressed && win) {
		QMetaObject::invokeMethod(win, "toggleVisibility", Q_ARG(bool, true));
	}
}

void SecondaryWindow::toggleVisibility(bool pressed)
{
	UNUSED_PARAMETER(pressed);
	if (isVisible()) {
		hide();
	} else {
		show();
		raise();
		activateWindow();
	}
}

void SecondaryWindow::setShowDockTitles(bool visible)
{
	showDockTitles = visible;
	QList<QDockWidget *> docks = findChildren<QDockWidget *>();
	for (QDockWidget *dock : docks) {
		// Disconnect first to avoid multiple connections if called repeatedly (though harmless with UniqueConnection)
		disconnect(dock, &QDockWidget::topLevelChanged, this, &SecondaryWindow::onDockTopLevelChanged);
		connect(dock, &QDockWidget::topLevelChanged, this, &SecondaryWindow::onDockTopLevelChanged, Qt::UniqueConnection);
		
		if (showDockTitles) {
			dock->setTitleBarWidget(nullptr); // Restore default
		} else {
			if (!dock->isFloating()) {
				if (!dock->titleBarWidget()) {
					dock->setTitleBarWidget(new QWidget(dock));
				}
			} else {
				dock->setTitleBarWidget(nullptr);
			}
		}
	}
}

void SecondaryWindow::onDockTopLevelChanged(bool topLevel)
{
	QDockWidget *dock = qobject_cast<QDockWidget *>(sender());
	if (!dock) return;
	
	if (topLevel) {
		// Floating: Always show title bar
		if (dock->titleBarWidget()) {
			delete dock->titleBarWidget();
			dock->setTitleBarWidget(nullptr);
		}
	} else {
		// Docked: Hide if showDockTitles is false
		if (!showDockTitles) {
			if (!dock->titleBarWidget()) {
				dock->setTitleBarWidget(new QWidget(dock));
			}
		} else {
			if (dock->titleBarWidget()) {
				delete dock->titleBarWidget();
				dock->setTitleBarWidget(nullptr);
			}
		}
	}
}

void SecondaryWindow::restoreInitialDocks()
{
	if (initialDocks.isEmpty()) return;

	QMainWindow *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!mainWindow) return;

	// Find the docks by name in the main window
	QList<QDockWidget *> allDocks = mainWindow->findChildren<QDockWidget *>();
	for (QDockWidget *dock : allDocks) {
		if (initialDocks.contains(dock->objectName())) {
			// Condition: Skip if it is docked in main window AND visible
			// If it is floating, we can take it back.
			// If it is hidden, we can take it back.
			bool isDockedInMain = !dock->isFloating() && (dock->window() == mainWindow);
			bool isVisible = dock->isVisible();

			if (isDockedInMain && isVisible) {
				continue; // Skip, user is using it in main window
			}

			reparentDock(dock);
		}
	}
	// Clear the list so we don't try to restore them again unless they are saved again on close
	initialDocks.clear();
}
