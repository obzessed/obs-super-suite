#include "browser_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QLabel>
#include <QUuid>
#include <QFormLayout>
#include <QJsonArray>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QDockWidget>
#include <QIcon>
#include <QPainter>
#include <QBrush>
#include <QPen>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include "docks/browser-dock.hpp"

#include "../utils/qcef_helper.hpp"

BrowserManager::BrowserManager(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(obs_module_text("BrowserManager.Title"));
	if (windowTitle().isEmpty()) setWindowTitle("Browser Dock Manager");
	resize(500, 400);
	setupUi();
}

BrowserManager::~BrowserManager()
{
	for (const auto &entry : docks) {
		deleteBrowserDock(entry.id);
	}
	docks.clear();
}

void BrowserManager::cleanup(bool full)
{
	QCefHelper::cleanup(full);
}

void BrowserManager::onOBSBrowserReady()
{
	deferred_load = false;

	for (const auto &entry : docks) {
		if (activeDocks.contains(entry.id) && activeDocks[entry.id]) {
			activeDocks[entry.id]->onOBSBrowserReady();
		}
	}
}

void BrowserManager::setupUi()
{
	auto *layout = new QVBoxLayout(this);

	// List
	dockList = new QListWidget(this);
	layout->addWidget(new QLabel(obs_module_text("BrowserManager.ListLabel"), this));
	layout->addWidget(dockList);

	// Buttons
	auto *btnLayout = new QHBoxLayout();
	addBtn = new QPushButton(obs_module_text("Add"), this);
	editBtn = new QPushButton(obs_module_text("BrowserManager.Edit"), this);
	if (editBtn->text().isEmpty()) editBtn->setText("Edit"); // Fallback if simple "Edit" exists or not
	
	removeBtn = new QPushButton(obs_module_text("Remove"), this);
	
	btnLayout->addWidget(addBtn);
	btnLayout->addWidget(editBtn);
	
	reloadBtn = new QPushButton(obs_module_text("BrowserManager.Reload"), this);
	visibilityBtn = new QPushButton(obs_module_text("BrowserManager.Visibility"), this);
	if (reloadBtn->text().isEmpty()) reloadBtn->setText("Reload");
	if (visibilityBtn->text().isEmpty()) visibilityBtn->setText("Show/Hide");
	
	btnLayout->addWidget(reloadBtn);
	btnLayout->addWidget(visibilityBtn);
	btnLayout->addWidget(removeBtn);
	layout->addLayout(btnLayout);

	connect(addBtn, &QPushButton::clicked, this, &BrowserManager::onAdd);
	connect(editBtn, &QPushButton::clicked, this, &BrowserManager::onEdit);
	connect(reloadBtn, &QPushButton::clicked, this, &BrowserManager::onReload);
	connect(visibilityBtn, &QPushButton::clicked, this, &BrowserManager::onVisibility);
	connect(removeBtn, &QPushButton::clicked, this, &BrowserManager::onRemove);
	connect(dockList, &QListWidget::itemSelectionChanged, this, &BrowserManager::onSelectionChanged);
	
	onSelectionChanged(); // Update button state
}

void BrowserManager::refreshList()
{
	dockList->clear();
	auto getBackendIcon = [](BackendType type) -> QIcon {
		QPixmap pixmap(24, 24);
		pixmap.fill(Qt::transparent);

		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::Antialiasing);

		QColor bgColor;
		QString letter;

		switch (type) {
		case BackendType::ObsBrowserCEF:
			bgColor = QColor(60, 60, 60); // Dark Grey
			letter = "O";
			break;
		case BackendType::EdgeWebView2:
			bgColor = QColor(0, 120, 215); // Edge Blue-ish
			letter = "E";
			break;
		case BackendType::StandaloneCEF:
			bgColor = QColor(255, 140, 0); // Orange-ish
			letter = "C";
			break;
		default:
			bgColor = Qt::gray;
			letter = "?";
			break;
		}

		painter.setPen(Qt::NoPen);
		painter.setBrush(bgColor);
		painter.drawRoundedRect(0, 0, 24, 24, 4, 4);

		painter.setPen(Qt::white);
		QFont font = painter.font();
		font.setBold(true);
		font.setPixelSize(14);
		painter.setFont(font);
		painter.drawText(pixmap.rect(), Qt::AlignCenter, letter);

		return QIcon(pixmap);
	};

	for (const auto &entry : docks) {
		QListWidgetItem *item = new QListWidgetItem(QString("%1 (%2)").arg(entry.title, entry.url));
		item->setData(Qt::UserRole, entry.id);
		item->setIcon(getBackendIcon(entry.backend));
		dockList->addItem(item);
	}
}

void BrowserManager::onAdd()
{
	QDialog dlg(this);
	dlg.setWindowTitle("Add Browser Dock");
	QFormLayout *layout = new QFormLayout(&dlg);
	
	QLineEdit *titleEdit = new QLineEdit(&dlg);
	titleEdit->setText(QString("Browser Dock %1").arg(docks.size() + 1));
	titleEdit->selectAll();
	
	QLineEdit *urlEdit = new QLineEdit(&dlg);
	urlEdit->setText("https://obsproject.com/browser-source");
	
	QPlainTextEdit *scriptEdit = new QPlainTextEdit(&dlg);
	scriptEdit->setPlaceholderText("// JavaScript to run on load");
	
	QPlainTextEdit *cssEdit = new QPlainTextEdit(&dlg);
	cssEdit->setPlaceholderText("/* Custom CSS */");

	QComboBox *backendCombo = new QComboBox(&dlg);
	backendCombo->addItem("Builtin (OBS Browser)", QVariant::fromValue((int)BackendType::ObsBrowserCEF));
	backendCombo->addItem("System (Edge WebView2)", QVariant::fromValue((int)BackendType::EdgeWebView2));
	backendCombo->addItem("Chromium (Embedded)", QVariant::fromValue((int)BackendType::StandaloneCEF));
	backendCombo->setCurrentIndex(0);

	QComboBox *presetCombo = new QComboBox(&dlg);
	presetCombo->addItem("Select a Preset...", "");
	if (presets.isEmpty()) initBuiltInPresets();
	for (const auto &p : presets) {
		presetCombo->addItem(p.name, p.name);
	}

	layout->addRow("Title:", titleEdit);
	layout->addRow("Backend:", backendCombo);
	
	QHBoxLayout *presetLayout = new QHBoxLayout();
	presetLayout->addWidget(presetCombo);
	QPushButton *deletePresetBtn = new QPushButton("Delete", &dlg);
	deletePresetBtn->setEnabled(false);
	presetLayout->addWidget(deletePresetBtn);
	layout->addRow("Preset:", presetLayout);
	
	layout->addRow("URL:", urlEdit);
	layout->addRow("Startup Script:", scriptEdit);
	layout->addRow("Custom CSS:", cssEdit);
	
	QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	QPushButton *savePresetBtn = btns->addButton("Save as Preset", QDialogButtonBox::ActionRole);
	
	layout->addRow(btns);

	// Preset logic
	connect(presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int index) {
		deletePresetBtn->setEnabled(index > 0);
		if (index <= 0) return;
		QString name = presetCombo->itemData(index).toString();
		for (const auto &p : presets) {
			if (p.name == name) {
				urlEdit->setText(p.url);
				scriptEdit->setPlainText(p.script);
				cssEdit->setPlainText(p.css);
				break;
			}
		}
	});

	connect(savePresetBtn, &QPushButton::clicked, [&]() {
		QString name = QInputDialog::getText(&dlg, "Save Preset", "Preset Name:");
		if (name.isEmpty()) return;
		
		BrowserPreset p;
		p.name = name;
		p.url = urlEdit->text();
		p.script = scriptEdit->toPlainText();
		p.css = cssEdit->toPlainText();
		
		presets.append(p);
		presetCombo->addItem(p.name, p.name);
		presetCombo->setCurrentIndex(presetCombo->count() - 1);
		saveToConfig(); // Persist changes
	});

	connect(deletePresetBtn, &QPushButton::clicked, [&]() {
		int idx = presetCombo->currentIndex();
		if (idx <= 0) return; // Can't delete placeholder

		QString name = presetCombo->itemData(idx).toString();
		// Confirm
		if (QMessageBox::question(&dlg, "Delete Preset", QString("Are you sure you want to delete preset '%1'?").arg(name), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
			return;
		}

		// Remove from presets
		for (int i = 0; i < presets.size(); ++i) {
			if (presets[i].name == name) {
				presets.removeAt(i);
				break;
			}
		}

		// Remove from combo
		presetCombo->removeItem(idx);
		presetCombo->setCurrentIndex(0);
		saveToConfig(); // Persist changes
	});
	
	// Validation logic
	connect(btns, &QDialogButtonBox::accepted, [&]() {
		QString title = titleEdit->text().trimmed();
		QString url = urlEdit->text().trimmed();
		
		if (title.isEmpty()) {
			QMessageBox::warning(&dlg, "Invalid Input", "Title cannot be empty.");
			return;
		}
		if (url.isEmpty()) {
			QMessageBox::warning(&dlg, "Invalid Input", "URL cannot be empty.");
			return;
		}
		
		// Check duplicates
		for (const auto &entry : docks) {
			if (entry.title.compare(title, Qt::CaseInsensitive) == 0) {
				QMessageBox::warning(&dlg, "Duplicate Title", "A dock with this title already exists.");
				return;
			}
		}
		
		dlg.accept();
	});
	connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	
	if (dlg.exec() == QDialog::Accepted) {
		QString title = titleEdit->text().trimmed();
		QString url = urlEdit->text().trimmed();
		QString script = scriptEdit->toPlainText();
		QString css = cssEdit->toPlainText();
		BackendType backend = (BackendType)backendCombo->currentData().toInt();
		
		QString id = QUuid::createUuid().toString().remove("{").remove("}");
		
		BrowserDockEntry entry;
		entry.id = id;
		entry.title = title;
		entry.url = url;
		entry.script = script;
		entry.css = css;
		entry.backend = backend;
		
		docks.append(entry);
		createBrowserDock(id, title, url, script, css, backend, true);
		refreshList();
	}
}

void BrowserManager::onEdit()
{
	auto items = dockList->selectedItems();
	if (items.isEmpty()) return;
	
	QString id = items.first()->data(Qt::UserRole).toString();
	
	// Find entry
	int idx = -1;
	for (int i = 0; i < docks.size(); ++i) {
		if (docks[i].id == id) {
			idx = i;
			break;
		}
	}
	
	if (idx == -1) return;
	
	BrowserDockEntry &entry = docks[idx];
	
	QDialog dlg(this);
	dlg.setWindowTitle("Edit Browser Dock");
	QFormLayout *layout = new QFormLayout(&dlg);
	
	QLineEdit *titleEdit = new QLineEdit(&dlg);
	titleEdit->setText(entry.title);
	QLineEdit *urlEdit = new QLineEdit(&dlg);
	urlEdit->setText(entry.url);
	
	QPlainTextEdit *scriptEdit = new QPlainTextEdit(&dlg);
	scriptEdit->setPlainText(entry.script);
	
	QPlainTextEdit *cssEdit = new QPlainTextEdit(&dlg);
	cssEdit->setPlainText(entry.css);

	QComboBox *backendCombo = new QComboBox(&dlg);
	backendCombo->addItem("OBS Browser (CEF)", QVariant::fromValue((int)BackendType::ObsBrowserCEF));
	backendCombo->addItem("Edge WebView2", QVariant::fromValue((int)BackendType::EdgeWebView2));
	backendCombo->addItem("Standalone CEF", QVariant::fromValue((int)BackendType::StandaloneCEF));
	
	int bIdx = backendCombo->findData((int)entry.backend);
	if (bIdx != -1) backendCombo->setCurrentIndex(bIdx);
	else backendCombo->setCurrentIndex(0);
	backendCombo->setEnabled(false);

	QComboBox *presetCombo = new QComboBox(&dlg);
	presetCombo->addItem("Select a Preset...", "");
	if (presets.isEmpty()) initBuiltInPresets();
	for (const auto &p : presets) {
		presetCombo->addItem(p.name, p.name);
	}
	
	layout->addRow("Title:", titleEdit);
	layout->addRow("Backend:", backendCombo);
	
	QHBoxLayout *presetLayout = new QHBoxLayout();
	presetLayout->addWidget(presetCombo);
	QPushButton *deletePresetBtn = new QPushButton("Delete", &dlg);
	deletePresetBtn->setEnabled(false);
	presetLayout->addWidget(deletePresetBtn);
	layout->addRow("Preset:", presetLayout);

	layout->addRow("URL:", urlEdit);
	layout->addRow("Startup Script:", scriptEdit);
	layout->addRow("Custom CSS:", cssEdit);
	
	QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	QPushButton *savePresetBtn = btns->addButton("Save as Preset", QDialogButtonBox::ActionRole);
	QPushButton *clearDataBtn = btns->addButton("Clear Data", QDialogButtonBox::ActionRole);
	layout->addRow(btns);

	// Preset logic
	connect(presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int index) {
		deletePresetBtn->setEnabled(index > 0);
		if (index <= 0) return;
		QString name = presetCombo->itemData(index).toString();
		for (const auto &p : presets) {
			if (p.name == name) {
				urlEdit->setText(p.url);
				scriptEdit->setPlainText(p.script);
				cssEdit->setPlainText(p.css);
				break;
			}
		}
	});

	connect(savePresetBtn, &QPushButton::clicked, [&]() {
		QString name = QInputDialog::getText(&dlg, "Save Preset", "Preset Name:");
		if (name.isEmpty()) return;
		
		BrowserPreset p;
		p.name = name;
		p.url = urlEdit->text();
		p.script = scriptEdit->toPlainText();
		p.css = cssEdit->toPlainText();
		
		presets.append(p);
		presetCombo->addItem(p.name, p.name);
		presetCombo->setCurrentIndex(presetCombo->count() - 1);
		saveToConfig(); // Persist changes
	});

	connect(deletePresetBtn, &QPushButton::clicked, [&]() {
		int idx = presetCombo->currentIndex();
		if (idx <= 0) return; // Can't delete placeholder

		QString name = presetCombo->itemData(idx).toString();
		// Confirm
		if (QMessageBox::question(&dlg, "Delete Preset", QString("Are you sure you want to delete preset '%1'?").arg(name), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
			return;
		}

		// Remove from presets
		for (int i = 0; i < presets.size(); ++i) {
			if (presets[i].name == name) {
				presets.removeAt(i);
				break;
			}
		}

		// Remove from combo
		presetCombo->removeItem(idx);
		presetCombo->setCurrentIndex(0);
		saveToConfig(); // Persist changes
	});
	
	connect(clearDataBtn, &QPushButton::clicked, [&]() {
		if (activeDocks.contains(entry.id) && activeDocks[entry.id]) {
			if (QMessageBox::question(&dlg, "Clear Data", "Are you sure you want to clear cookies and cache for this browser?\nThis action cannot be undone.", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
				activeDocks[entry.id]->webBrowser()->clearCookies();
				QMessageBox::information(&dlg, "Data Cleared", "Browser data has been cleared.");
			}
		} else {
			QMessageBox::warning(&dlg, "Not Active", "The browser dock must be open and active to clear data.");
		}
	});

	connect(btns, &QDialogButtonBox::accepted, [&]() {
		QString newTitle = titleEdit->text().trimmed();
		QString newUrl = urlEdit->text().trimmed();
		
		if (newTitle.isEmpty()) {
			QMessageBox::warning(&dlg, "Invalid Input", "Title cannot be empty.");
			return;
		}
		if (newUrl.isEmpty()) {
			QMessageBox::warning(&dlg, "Invalid Input", "URL cannot be empty.");
			return;
		}
		
		// Check duplicates (exclude current)
		for (const auto &d : docks) {
			if (d.id != entry.id && d.title.compare(newTitle, Qt::CaseInsensitive) == 0) {
				QMessageBox::warning(&dlg, "Duplicate Title", "A dock with this title already exists.");
				return;
			}
		}
		
		dlg.accept();
	});
	connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	
	if (dlg.exec() == QDialog::Accepted) {
		QString newTitle = titleEdit->text().trimmed();
		QString newUrl = urlEdit->text().trimmed();
		QString newScript = scriptEdit->toPlainText();
		QString newCss = cssEdit->toPlainText();
		
		if (newTitle.isEmpty() || newUrl.isEmpty()) return;
		
		// If changed, recreate dock?
		// Recreating is easiest to update properties.
		// Or maybe we can update in place? 
		// BrowserDock doesn't seem to expose update methods easily in snippet, 
		// but checking BrowserDock implementation (which I saw earlier in generic planning) might reveal setUrl.
		// For now, let's destroy and recreate to be safe and simple.
		
		bool scriptChanged = (newScript != entry.script) || (newCss != entry.css) || (newUrl != entry.url);
		bool shouldRecreate = true;

		if (scriptChanged) {
			QMessageBox::StandardButton res = QMessageBox::question(this, 
				obs_module_text("BrowserManager.ReloadQueryTitle") ? obs_module_text("BrowserManager.ReloadQueryTitle") : "Reload Required",
				obs_module_text("BrowserManager.ReloadQueryText") ? obs_module_text("BrowserManager.ReloadQueryText") : "Dock settings changed. Reload dock now to apply?",
				QMessageBox::Yes | QMessageBox::No);
			
			if (res == QMessageBox::No) {
				shouldRecreate = false;
			}
		}
		
		entry.title = newTitle;
		entry.url = newUrl;
		entry.script = newScript;
		entry.css = newCss;
		
		// If not recreating (user said NO), we just saved the new config to 'docks'.
		// If user said YES (shouldRecreate == true) OR no script change (shouldRecreate == true default):
		if (shouldRecreate) {
			// Try to reload existing dock first
			if (activeDocks.contains(entry.id) && activeDocks[entry.id]) {
				BrowserDock *dock = activeDocks[entry.id];
				dock->reload(entry.url.toUtf8().constData(), entry.script.toUtf8().constData(), entry.css.toUtf8().constData());
				
				// Try to update title
				QWidget *parent = dock->parentWidget();
				while (parent) {
					if (QDockWidget *dw = qobject_cast<QDockWidget*>(parent)) {
						dw->setWindowTitle(entry.title);
						break;
					}
					parent = parent->parentWidget();
				}
			} else {
				// Not found (maybe closed or not yet created), so create/recreate
				deleteBrowserDock(entry.id);
				createBrowserDock(entry.id, entry.title, entry.url, entry.script, entry.css, entry.backend, true);
			}
		}
		
		refreshList();
	}
}

void BrowserManager::onRemove()
{
	auto items = dockList->selectedItems();
	if (items.isEmpty()) return;
	
	QString id = items.first()->data(Qt::UserRole).toString();
	
	if (QMessageBox::question(this, "Confirm Remove", "Remove this browser dock?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
		deleteBrowserDock(id);
		
		for (int i = 0; i < docks.size(); ++i) {
			if (docks[i].id == id) {
				docks.removeAt(i);
				break;
			}
		}
		refreshList();
	}
}

void BrowserManager::onReload()
{
	auto items = dockList->selectedItems();
	if (items.isEmpty()) return;
	QString id = items.first()->data(Qt::UserRole).toString();
	
	if (activeDocks.contains(id) && activeDocks[id]) {
		// Reload active dock
		// We need to fetch current config just in case, but here we just reload current state?
		// Better to look up the dock entry to get the "correct" url/script/css from config
		// in case it drifted or was just edited but not reloaded?
		// But onEdit updates active dock.
		// So active dock state should be current.
		// We'll just call reload with current internal state of dock?
		// `reload()` without args uses existing URL/Script?
		// No, `BrowserDock::reload` takes args.
		// So we must fetch from `docks` list.
		
		for (const auto &entry : docks) {
			if (entry.id == id) {
				activeDocks[id]->reload(entry.url.toUtf8().constData(), entry.script.toUtf8().constData(), entry.css.toUtf8().constData());
				break;
			}
		}
	}
}

void BrowserManager::onVisibility()
{
	auto items = dockList->selectedItems();
	if (items.isEmpty()) return;
	QString id = items.first()->data(Qt::UserRole).toString();

	if (activeDocks.contains(id) && activeDocks[id]) {
		// Toggle visibility
		BrowserDock *dock = activeDocks[id];
		QWidget *parent = dock->parentWidget();
		while (parent) {
			if (QDockWidget *dw = qobject_cast<QDockWidget*>(parent)) {
				dw->setVisible(!dw->isVisible());
				if (dw->isVisible()) dw->raise();
				break;
			}
			parent = parent->parentWidget();
		}
	} else {
		// Not active/created => create and show
		for (const auto &entry : docks) {
			if (entry.id == id) {
				createBrowserDock(entry.id, entry.title, entry.url, entry.script, entry.css, entry.backend, true);
				break;
			}
		}
	}
}

void BrowserManager::onSelectionChanged()
{
	bool hasSel = !dockList->selectedItems().isEmpty();
	editBtn->setEnabled(hasSel);
	reloadBtn->setEnabled(hasSel);
	visibilityBtn->setEnabled(hasSel);
	removeBtn->setEnabled(hasSel);
}

void BrowserManager::createBrowserDock(const QString &id, const QString &title, const QString &url, const QString &script, const QString &css, BackendType backend, bool visible)
{
	// ID must be unique
	QString dockId = "SuperSuite_BrowserDock_" + id;
	
	QMainWindow *mainWin = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	BrowserDock *dock = new BrowserDock(*this, id, url.toUtf8().constData(), script.toUtf8().constData(), css.toUtf8().constData(), backend, deferred_load, mainWin);
	
	obs_frontend_add_dock_by_id(dockId.toUtf8().constData(), title.toUtf8().constData(), dock);
	
	activeDocks[id] = dock;

	if (visible) {
		// Find the QDockWidget parent and show it
		QWidget *parent = dock->parentWidget();
		while (parent) {
			if (QDockWidget *dw = qobject_cast<QDockWidget*>(parent)) {
				dw->setVisible(true);
				dw->raise();
				break;
			}
			parent = parent->parentWidget();
		}
	}
}

void BrowserManager::deleteBrowserDock(const QString &id)
{
	QString dockId = "SuperSuite_BrowserDock_" + id;
	obs_frontend_remove_dock(dockId.toUtf8().constData());
	activeDocks.remove(id);
}

QJsonObject BrowserManager::saveToConfig()
{
	QJsonObject root;
	QJsonArray arr;
	for (const auto &entry : docks) {
		QJsonObject obj;
		obj["id"] = entry.id;
		obj["title"] = entry.title;
		obj["url"] = entry.url;
		obj["script"] = entry.script;
		obj["css"] = entry.css;
		obj["backend"] = QString::fromStdString(BackendHelpers::ToString(entry.backend));
		arr.append(obj);
	}
	root["docks"] = arr;
	
	root["presets"] = savePresets()["presets"];
	
	return root;
}

void BrowserManager::loadFromConfig(const QJsonObject &data)
{
	loadPresets(data);

	docks.clear();
	if (data.contains("docks")) {
		QJsonArray arr = data["docks"].toArray();
		for (const auto &val : arr) {
			QJsonObject obj = val.toObject();
			BrowserDockEntry entry;
			entry.id = obj["id"].toString();
			entry.title = obj["title"].toString();
			entry.url = obj["url"].toString();
			entry.script = obj["script"].toString();
			entry.css = obj["css"].toString();
			entry.script = obj["script"].toString();
			entry.css = obj["css"].toString();
			
			QString backendStr = obj["backend"].toString();
			if (backendStr.isEmpty()) {
				entry.backend = BackendType::ObsBrowserCEF;
			} else {
				entry.backend = BackendHelpers::FromString(backendStr.toStdString());
			}
			
			docks.append(entry);
			createBrowserDock(entry.id, entry.title, entry.url, entry.script, entry.css, entry.backend);
		}
	}
	refreshList();
}
void BrowserManager::setDeferredLoad(bool deferredLoad)
{
	deferred_load = deferredLoad;
}

void BrowserManager::initBuiltInPresets()
{
	if (!presets.isEmpty()) return;

	BrowserPreset p1;
	p1.name = "Google";
	p1.url = "https://google.com";
	p1.script = "";
	p1.css = "";
	presets.append(p1);

	BrowserPreset p2;
	p2.name = "WhatsApp Web";
	p2.url = "https://web.whatsapp.com";
	p2.script = "";
	p2.css = "";
	presets.append(p2);

	BrowserPreset p3;
	p3.name = "Telegram Web";
	p3.url = "https://web.telegram.org";
	p3.script = "";
	p3.css = "";
	presets.append(p3);
}

QJsonObject BrowserManager::savePresets()
{
	QJsonObject root;
	QJsonArray arr;
	for (const auto &p : presets) {
		QJsonObject obj;
		obj["name"] = p.name;
		obj["url"] = p.url;
		obj["script"] = p.script;
		obj["css"] = p.css;
		arr.append(obj);
	}
	root["presets"] = arr;
	return root;
}

void BrowserManager::loadPresets(const QJsonObject &data)
{
	if (data.contains("presets")) {
		presets.clear();
		QJsonArray arr = data["presets"].toArray();
		for (const auto &val : arr) {
			QJsonObject obj = val.toObject();
			BrowserPreset p;
			p.name = obj["name"].toString();
			p.url = obj["url"].toString();
			p.script = obj["script"].toString();
			p.css = obj["css"].toString();
			presets.append(p);
		}
	} else {
		initBuiltInPresets();
	}
}
