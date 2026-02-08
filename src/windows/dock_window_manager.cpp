#include "dock_window_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QLabel>
#include <QCheckBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QByteArray>
#include <obs-module.h>
#include <obs-frontend-api.h>

DockWindowManager::DockWindowManager(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(obs_module_text("DockWindowManager.Title"));
	resize(400, 300);
	setupUi();
	
	// Start with one window if none exist? Or let user create one.
}

DockWindowManager::~DockWindowManager()
{
	// Windows are parented to main window, so they clean themselves up, 
	// unless we want to manually close them.
	for (auto *win : managedWindows) {
		if (win) win->close();
	}
}

void DockWindowManager::setupUi()
{
	auto *layout = new QVBoxLayout(this);

	// List
	windowList = new QListWidget(this);
	layout->addWidget(new QLabel(obs_module_text("DockWindowManager.ListLabel"), this));
	layout->addWidget(windowList);

	// Input for new name
	auto *inputLayout = new QHBoxLayout();
	nameInput = new QLineEdit(this);
	nameInput->setPlaceholderText(obs_module_text("DockWindowManager.NamePlaceholder"));
	createBtn = new QPushButton(obs_module_text("Add"), this);
	inputLayout->addWidget(nameInput);
	inputLayout->addWidget(createBtn);
	layout->addLayout(inputLayout);

	// Action buttons
	auto *btnLayout = new QHBoxLayout();
	renameBtn = new QPushButton(obs_module_text("Rename"), this);
	removeBtn = new QPushButton(obs_module_text("Remove"), this);
	showHideBtn = new QPushButton(obs_module_text("ShowHide"), this);
	
	btnLayout->addWidget(showHideBtn);
	btnLayout->addWidget(renameBtn);
	btnLayout->addWidget(removeBtn);
	layout->addLayout(btnLayout);

	// Opacity Control
	auto *opacityLayout = new QHBoxLayout();
	opacityLabel = new QLabel(obs_module_text("DockWindowManager.Opacity"), this);
	opacitySlider = new QSlider(Qt::Horizontal, this);
	opacitySlider->setRange(20, 100); // Don't allow 0% (invisible/unclickable)
	opacitySlider->setValue(100);
	opacityLayout->addWidget(opacityLabel);
	opacityLayout->addWidget(opacitySlider);
	layout->addLayout(opacityLayout);

	// Connections
	connect(createBtn, &QPushButton::clicked, this, &DockWindowManager::createNewWindow);
	connect(renameBtn, &QPushButton::clicked, this, &DockWindowManager::renameSelectedWindow);
	connect(removeBtn, &QPushButton::clicked, this, &DockWindowManager::removeSelectedWindow);
	connect(showHideBtn, &QPushButton::clicked, this, &DockWindowManager::toggleVisibility);
	
	connect(opacitySlider, &QSlider::valueChanged, this, &DockWindowManager::onOpacityChanged);
	
	connect(windowList, &QListWidget::itemSelectionChanged, [this]() {
		auto items = windowList->selectedItems();
		bool hasSel = !items.isEmpty();
		renameBtn->setEnabled(hasSel);
		removeBtn->setEnabled(hasSel);
		showHideBtn->setEnabled(hasSel);
		opacitySlider->setEnabled(hasSel);
		
		if (hasSel) {
			SecondaryWindow *win = static_cast<SecondaryWindow*>(items.first()->data(Qt::UserRole).value<void*>());
			if (win) {
				opacitySlider->blockSignals(true);
				opacitySlider->setValue((int)(win->windowOpacity() * 100));
				opacitySlider->blockSignals(false);
			}
		}
	});
	
	connect(windowList, &QListWidget::itemDoubleClicked, this, &DockWindowManager::toggleVisibility);

	refreshWindowList();
}

void DockWindowManager::refreshWindowList()
{
	windowList->clear();
	for (SecondaryWindow *win : managedWindows) {
		if (!win) continue;
		
		QString title = win->windowTitle();
		QString status = win->isVisible() ? "[Visible]" : "[Hidden]";
		
		QListWidgetItem *item = new QListWidgetItem(QString("%1 %2").arg(title, status));
		item->setData(Qt::UserRole, QVariant::fromValue(static_cast<void*>(win)));
		windowList->addItem(item);
	}
	
	bool hasSel = !windowList->selectedItems().isEmpty();
	renameBtn->setEnabled(hasSel);
	removeBtn->setEnabled(hasSel);
	showHideBtn->setEnabled(hasSel);
}

void DockWindowManager::createNewWindow()
{
	QString name = nameInput->text().trimmed();
	if (name.isEmpty()) {
		name = QString("Dock Window %1").arg(managedWindows.size() + 1);
	}

	QWidget *parent = this->parentWidget(); // Should be main window
	
	// Create with unique index
	// Pass nullptr as parent to ensure it shows up in taskbar as independent window
	SecondaryWindow *win = new SecondaryWindow(nextId++, this, nullptr);
	win->setWindowTitle(name);
	win->setObjectName(QString("SuperSuiteSecondaryWindow_Dynamic_%1").arg(nextId)); // Ensure unique object name independent of index if needed
	win->show();
	
	connect(win, &QObject::destroyed, this, &DockWindowManager::onWindowDestroyed);
	
	managedWindows.append(win);
	nameInput->clear();
	refreshWindowList();
}

void DockWindowManager::renameSelectedWindow()
{
	auto items = windowList->selectedItems();
	if (items.isEmpty()) return;
	
	SecondaryWindow *win = static_cast<SecondaryWindow*>(items.first()->data(Qt::UserRole).value<void*>());
	if (!win) return;

	bool ok;
	QString text = QInputDialog::getText(this, 
		obs_module_text("DockWindowManager.RenameDlgTitle"),
		obs_module_text("DockWindowManager.RenameDlgLabel"), 
		QLineEdit::Normal,
		win->windowTitle(), &ok);
		
	if (ok && !text.isEmpty()) {
		win->setWindowTitle(text);
		refreshWindowList();
	}
}

void DockWindowManager::removeSelectedWindow()
{
	auto items = windowList->selectedItems();
	if (items.isEmpty()) return;

	SecondaryWindow *win = static_cast<SecondaryWindow*>(items.first()->data(Qt::UserRole).value<void*>());
	if (!win) return;

	auto reply = QMessageBox::question(this,
		obs_module_text("DockWindowManager.ConfirmRemove"),
		obs_module_text("DockWindowManager.ConfirmRemoveMsg"),
		QMessageBox::Yes | QMessageBox::No);

	if (reply == QMessageBox::Yes) {
		managedWindows.removeOne(win);
		win->close();
		win->deleteLater();
		refreshWindowList();
	}
}

void DockWindowManager::toggleVisibility()
{
	auto items = windowList->selectedItems();
	if (items.isEmpty()) return;

	SecondaryWindow *win = static_cast<SecondaryWindow*>(items.first()->data(Qt::UserRole).value<void*>());
	if (!win) return;

	if (win->isVisible()) {
		win->hide();
	} else {
		win->show();
		win->raise();
		win->activateWindow();
	}
	refreshWindowList();
}

void DockWindowManager::onWindowDestroyed(QObject *obj)
{
	// Safe to cast? Maybe. Better to just remove by pointer comparison if it's in list.
	// But object is half destroyed.
	// Actually, we shouldn't rely on QObject cast here.
	// But we stored SecondaryWindow* in managedWindows.
	// We should just clean up nulls.
	
	// Since obj is being destroyed, we can't reliably cast it to SecondaryWindow to check properties
	// but we can check if it matches pointer address.
	for (int i = 0; i < managedWindows.size(); i++) {
		if (managedWindows[i] == obj) {
			managedWindows.removeAt(i);
			break;
		}
	}
	refreshWindowList();
}

void DockWindowManager::onOpacityChanged(int value)
{
	auto items = windowList->selectedItems();
	if (items.isEmpty()) return;

	SecondaryWindow *win = static_cast<SecondaryWindow*>(items.first()->data(Qt::UserRole).value<void*>());
	if (!win) return;
	
	win->setWindowOpacity((qreal)value / 100.0);
}

QJsonObject DockWindowManager::saveToConfig()
{
	QJsonObject root;
	QJsonArray windowsArray;

	for (SecondaryWindow *win : managedWindows) {
		if (!win) continue;
		
		QJsonObject winObj;
		winObj["title"] = win->windowTitle();
		winObj["objectName"] = win->objectName();
		winObj["geometry"] = QString(win->saveGeometry().toBase64());
		winObj["state"] = QString(win->saveState().toBase64());
		winObj["fullscreen"] = win->isFullScreen();
		winObj["stayOnTop"] = (bool)(win->windowFlags() & Qt::WindowStaysOnTopHint);
		winObj["visible"] = win->isVisible();
		winObj["opacity"] = win->windowOpacity();
		winObj["showDockTitles"] = win->getShowDockTitles();
		
		// Save dock list to reclaim them later
		QJsonArray ownedDocks;
		QList<QDockWidget *> docks = win->findChildren<QDockWidget *>();
		for (QDockWidget *dock : docks) {
			if (dock->isVisible()) { // Allow floating docks too
				if (!dock->objectName().isEmpty()) {
					ownedDocks.append(dock->objectName());
				}
			}
		}
		winObj["ownedDocks"] = ownedDocks;

		windowsArray.append(winObj);
	}
	
	root["windows"] = windowsArray;
	root["nextId"] = nextId;
	
	// Save snapshots
	QJsonObject snapsObj;
	for (auto it = snapshots.begin(); it != snapshots.end(); ++it) {
		snapsObj[it.key()] = it.value();
	}
	root["snapshots"] = snapsObj;
	
	return root;
}

void DockWindowManager::loadFromConfig(const QJsonObject &data)
{
	if (data.contains("nextId")) {
		nextId = data["nextId"].toInt();
	}

	if (data.contains("snapshots")) {
		QJsonObject snapsObj = data["snapshots"].toObject();
		snapshots.clear();
		for (auto it = snapsObj.begin(); it != snapsObj.end(); ++it) {
			snapshots[it.key()] = it.value().toObject();
		}
	}

	if (data.contains("windows")) {
		QJsonArray windowsArray = data["windows"].toArray();
		
		QMainWindow *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		
		// Find all Main Docks to potentially reclaim
		QList<QDockWidget *> allMainDocks = mainWindow->findChildren<QDockWidget *>();
		QMap<QString, QDockWidget*> dockMap;
		for (auto *dock : allMainDocks) {
			if (!dock->objectName().isEmpty()) {
				dockMap[dock->objectName()] = dock;
			}
		}

		for (const QJsonValue &val : windowsArray) {
			QJsonObject winObj = val.toObject();
			
			// Recreate window (nullptr parent for taskbar)
			// Recreate window (nullptr parent for taskbar)
			SecondaryWindow *win = new SecondaryWindow(0, this, nullptr);
			
			if (winObj.contains("objectName")) {
				win->setObjectName(winObj["objectName"].toString());
			}
			if (winObj.contains("title")) {
				win->setWindowTitle(winObj["title"].toString());
			}
			
			// Restore geometry/state
			// We restore geometry first to set size
			if (winObj.contains("geometry")) {
				win->restoreGeometry(QByteArray::fromBase64(winObj["geometry"].toString().toUtf8()));
			}
			
			// Restore docks
			// We must reparent them BEFORE restoring state, otherwise state restore won't know about them
			if (winObj.contains("ownedDocks")) {
				QJsonArray dockNames = winObj["ownedDocks"].toArray();
				for (const QJsonValue &dockVal : dockNames) {
					QString name = dockVal.toString();
					if (dockMap.contains(name)) {
						win->reparentDock(dockMap[name]);
					}
				}
			}
			
			// Restore state (dock positions)
			if (winObj.contains("state")) {
				win->restoreState(QByteArray::fromBase64(winObj["state"].toString().toUtf8()));
			}
			
			// Restore flags
			bool stayOnTop = winObj["stayOnTop"].toBool(false);
			if (stayOnTop) {
				win->setWindowFlags(win->windowFlags() | Qt::WindowStaysOnTopHint);
			}
			
			if (winObj["fullscreen"].toBool(false)) {
				win->showFullScreen();
			} else {
				if (winObj["visible"].toBool(true)) {
					win->show();
				} else {
					// Ensure it's hidden, though new windows are hidden by default
					// But we might need to make sure docks are initialized?
					// Docks are reparented above.
					win->hide();
				}
			}
			
			if (winObj.contains("opacity")) {
				win->setWindowOpacity(winObj["opacity"].toDouble(1.0));
			}

			if (winObj.contains("showDockTitles")) {
				win->setShowDockTitles(winObj["showDockTitles"].toBool(true));
			}
			
			connect(win, &QObject::destroyed, this, &DockWindowManager::onWindowDestroyed);
			managedWindows.append(win);
		}
	}
	
	refreshWindowList();
}

void DockWindowManager::saveSnapshot(const QString &name, const QJsonObject &data)
{
	snapshots[name] = data;
}

void DockWindowManager::deleteSnapshot(const QString &name)
{
	snapshots.remove(name);
}

QStringList DockWindowManager::getSnapshotNames() const
{
	return snapshots.keys();
}

bool DockWindowManager::requestRestoreSnapshot(const QString &name, SecondaryWindow *target)
{
	if (!snapshots.contains(name) || !target) return false;
	
	QJsonObject snapData = snapshots[name];
	
	// Show Restore Options Dialog
	QDialog dlg(target);
	dlg.setWindowTitle(obs_module_text("DockWindowManager.Snapshot.RestoreTitle"));
	if (dlg.windowTitle().isEmpty()) dlg.setWindowTitle("Restore Layout Snapshot");
	
	QVBoxLayout *layout = new QVBoxLayout(&dlg);
	
	layout->addWidget(new QLabel(QString("Restore snapshot '%1'?").arg(name), &dlg));
	
	QString restorePrefix = "Restore ";

	QCheckBox *chkPos = new QCheckBox(restorePrefix + obs_module_text("DockWindowManager.Snapshot.Option.Pos"), &dlg);
	if (chkPos->text() == restorePrefix) chkPos->setText("Restore Window Position & Size");
	chkPos->setChecked(snapData.contains("geometry"));
	chkPos->setEnabled(snapData.contains("geometry"));
	layout->addWidget(chkPos);
	
	QCheckBox *chkDocks = new QCheckBox(restorePrefix + obs_module_text("DockWindowManager.Snapshot.Option.Docks"), &dlg);
	if (chkDocks->text() == restorePrefix) chkDocks->setText("Restore Docks Layout");
	chkDocks->setChecked(snapData.contains("ownedDocks") || snapData.contains("state"));
	chkDocks->setEnabled(snapData.contains("ownedDocks") || snapData.contains("state"));
	layout->addWidget(chkDocks);
	
	QCheckBox *chkOpacity = new QCheckBox(restorePrefix + obs_module_text("DockWindowManager.Snapshot.Option.Opacity"), &dlg);
	if (chkOpacity->text() == restorePrefix) chkOpacity->setText("Restore Opacity");
	chkOpacity->setChecked(snapData.contains("opacity"));
	chkOpacity->setEnabled(snapData.contains("opacity"));
	layout->addWidget(chkOpacity);
	
	QCheckBox *chkTop = new QCheckBox(restorePrefix + obs_module_text("DockWindowManager.Snapshot.Option.StayOnTop"), &dlg);
	if (chkTop->text() == restorePrefix) chkTop->setText("Restore 'Stay on Top' State");
	chkTop->setChecked(snapData.contains("stayOnTop"));
	chkTop->setEnabled(snapData.contains("stayOnTop"));
	layout->addWidget(chkTop);
	
	QCheckBox *chkTitles = new QCheckBox(restorePrefix + obs_module_text("DockWindowManager.Snapshot.Option.DockHeaders"), &dlg);
	if (chkTitles->text() == restorePrefix) chkTitles->setText("Restore 'Show Dock Headers' State");
	chkTitles->setChecked(snapData.contains("showDockTitles"));
	chkTitles->setEnabled(snapData.contains("showDockTitles"));
	layout->addWidget(chkTitles);

	QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	layout->addWidget(btns);
	
	if (dlg.exec() != QDialog::Accepted) {
		return false;
	}

	// Check for conflicts ONLY if restoring docks
	QJsonArray ownedDocks = snapData["ownedDocks"].toArray();
	if (chkDocks->isChecked() && !ownedDocks.isEmpty()) {
		QStringList conflicts;
		
		QMap<QString, QDockWidget*> dockMap;

		// 1. Docks from Main Window
		QMainWindow *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		if (mainWindow) {
			for (auto *dock : mainWindow->findChildren<QDockWidget *>()) {
				if (!dock->objectName().isEmpty()) dockMap[dock->objectName()] = dock;
			}
		}

		// 2. Docks from ALL Secondary Windows
		for (SecondaryWindow *win : managedWindows) {
			if (!win) continue;
			for (auto *dock : win->findChildren<QDockWidget *>()) {
				if (!dock->objectName().isEmpty()) dockMap[dock->objectName()] = dock;
			}
		}
		
		for (const QJsonValue &val : ownedDocks) {
			QString dockName = val.toString();
			if (dockMap.contains(dockName)) {
				QDockWidget *dock = dockMap[dockName];
				
				// Identify current parent window
				QWidget *topLevel = dock->window();
				if (dock->isFloating() && dock->parentWidget()) {
					topLevel = dock->parentWidget()->window();
				}
				
				SecondaryWindow *parentWin = qobject_cast<SecondaryWindow*>(topLevel);
				
				// Conflict if it's in ANOTHER secondary window
				if (parentWin && parentWin != target) {
					conflicts.append(QString("%1 (in %2)").arg(dockName, parentWin->windowTitle()));
				}
				// Also conflict if it's in Main Window
				else if (topLevel == mainWindow) {
					conflicts.append(QString("%1 (in Main Window)").arg(dockName));
				}
			}
		}
		
		if (!conflicts.isEmpty()) {
			QString msg = obs_module_text("DockWindowManager.SnapshotConflict");
			if (msg.startsWith("DockWindowManager")) msg = "The following docks are currently in use by other windows:\n\n%1\n\nExisting layouts will be disrupted. Continue?";
			
			int ret = QMessageBox::question(target, "Restore Snapshot Conflict", 
				msg.arg(conflicts.join("\n")),
				QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
				
			if (ret != QMessageBox::Yes) {
				return false;
			}
		}
		
		// Reparent docks
		for (const QJsonValue &val : ownedDocks) {
			QString dockName = val.toString();
			if (dockMap.contains(dockName)) {
				target->reparentDock(dockMap[dockName]);
			}
		}
		
		// Restore State
		if (snapData.contains("state")) {
			target->restoreState(QByteArray::fromBase64(snapData["state"].toString().toUtf8()));
		}
	}
	
	// Apply other properties
	if (chkPos->isChecked() && snapData.contains("geometry")) {
		target->restoreGeometry(QByteArray::fromBase64(snapData["geometry"].toString().toUtf8()));
		if (snapData["fullscreen"].toBool(false)) {
			target->showFullScreen();
		} else {
			target->show();
		}
	}
	
	if (chkOpacity->isChecked() && snapData.contains("opacity")) {
		target->setWindowOpacity(snapData["opacity"].toDouble(1.0));
	}
	
	if (chkTop->isChecked() && snapData.contains("stayOnTop")) {
		bool stayOnTop = snapData["stayOnTop"].toBool(false);
		if (stayOnTop) {
			target->setWindowFlags(target->windowFlags() | Qt::WindowStaysOnTopHint);
		} else {
			target->setWindowFlags(target->windowFlags() & ~Qt::WindowStaysOnTopHint);
		}
		target->show(); // Apply flags
	}
	
	if (chkTitles->isChecked() && snapData.contains("showDockTitles")) {
		target->setShowDockTitles(snapData["showDockTitles"].toBool(true));
	}

	return true;
}

