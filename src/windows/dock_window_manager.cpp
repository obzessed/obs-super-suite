#include "dock_window_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QLabel>
#include <obs-module.h>

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
