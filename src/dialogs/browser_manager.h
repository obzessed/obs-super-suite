#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QJsonObject>
#include <QPointer>

#include "../docks/browser-panel.hpp"

class BrowserDock;

class BrowserManager : public QDialog {
	Q_OBJECT

public:
	explicit BrowserManager(QWidget *parent = nullptr);
	~BrowserManager() override;

	static void cleanup();

	void onOBSBrowserReady();

	// Persistence
	QJsonObject saveToConfig();
	void loadFromConfig(const QJsonObject &data);
	void setDeferredLoad(bool deferredLoad);

	std::pair<QCef*, QCefCookieManager*> getQCef();

private slots:
	void onAdd();
	void onEdit();
	void onRemove();
	void onSelectionChanged();

private:
	void setupUi();
	void refreshList();
	void createBrowserDock(const QString &id, const QString &title, const QString &url, const QString &script, const QString &css);
	void deleteBrowserDock(const QString &id);

	struct BrowserPreset {
		QString name;
		QString url;
		QString script;
		QString css;
	};

	struct BrowserDockEntry {
		QString id; // UUID
		QString title;
		QString url;
		QString script;
		QString css;
	};

	QList<BrowserDockEntry> docks;
	QList<BrowserPreset> presets;
	QMap<QString, QPointer<BrowserDock>> activeDocks;

	void loadPresets(const QJsonObject &data);
	QJsonObject savePresets();
	void initBuiltInPresets();

	QListWidget *dockList;
	QPushButton *addBtn;
	QPushButton *editBtn;
	QPushButton *removeBtn;

	bool deferred_load;
};
