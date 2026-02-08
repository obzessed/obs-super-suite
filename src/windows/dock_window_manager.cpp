#include "dock_window_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QLabel>
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
	showHideBtn = new QPushButton(obs_module_text("Show/Hide"), this);
	
	btnLayout->addWidget(showHideBtn);
	btnLayout->addWidget(renameBtn);
	btnLayout->addWidget(removeBtn);
	layout->addLayout(btnLayout);

	// Connections
	connect(createBtn, &QPushButton::clicked, this, &DockWindowManager::createNewWindow);
	connect(renameBtn, &QPushButton::clicked, this, &DockWindowManager::renameSelectedWindow);
	connect(removeBtn, &QPushButton::clicked, this, &DockWindowManager::removeSelectedWindow);
	connect(showHideBtn, &QPushButton::clicked, this, &DockWindowManager::toggleVisibility);
	
	connect(windowList, &QListWidget::itemSelectionChanged, [this]() {
		bool hasSel = !windowList->selectedItems().isEmpty();
		renameBtn->setEnabled(hasSel);
		removeBtn->setEnabled(hasSel);
		showHideBtn->setEnabled(hasSel);
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
	SecondaryWindow *win = new SecondaryWindow(nextId++, nullptr);
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
		
		// Save dock list to reclaim them later
		QJsonArray ownedDocks;
		QList<QDockWidget *> docks = win->findChildren<QDockWidget *>();
		for (QDockWidget *dock : docks) {
			if (dock->isVisible() && !dock->isFloating()) {
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
	
	return root;
}

void DockWindowManager::loadFromConfig(const QJsonObject &data)
{
	if (data.contains("nextId")) {
		nextId = data["nextId"].toInt();
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
			SecondaryWindow *win = new SecondaryWindow(0, nullptr);
			
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
				win->show();
			}
			
			connect(win, &QObject::destroyed, this, &DockWindowManager::onWindowDestroyed);
			managedWindows.append(win);
		}
	}
	
	refreshWindowList();
}
