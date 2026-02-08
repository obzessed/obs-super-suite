#pragma once

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QPointer>
#include <QJsonObject>
#include "secondary_window.h"

class DockWindowManager : public QDialog {
	Q_OBJECT

public:
	explicit DockWindowManager(QWidget *parent = nullptr);
	~DockWindowManager();

	// Global instance accessor if needed, or just managed by main plugin
	
public slots:
	void refreshWindowList();

	// Persistence methods
	QJsonObject saveToConfig();
	void loadFromConfig(const QJsonObject &data);

private:
	void setupUi();
	void createNewWindow();
	void renameSelectedWindow();
	void removeSelectedWindow();
	void toggleVisibility();
	void onWindowDestroyed(QObject *obj);

	QListWidget *windowList;
	QLineEdit *nameInput;
	QPushButton *createBtn;
	QPushButton *renameBtn;
	QPushButton *removeBtn;
	QPushButton *showHideBtn;

	// We need to track our windows.
	// We can't rely solely on QObject parentage if we want to manage them specifically.
	QList<SecondaryWindow*> managedWindows;
	int nextId = 0;
};
