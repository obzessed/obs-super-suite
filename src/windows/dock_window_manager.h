#pragma once

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QSlider>
#include <QLabel>
#include <QPointer>
#include <QJsonObject>
#include "secondary_window.h"

class DockWindowManager : public QDialog {
	Q_OBJECT

public:
	explicit DockWindowManager(QWidget *parent = nullptr);
	~DockWindowManager() override;

	// Global instance accessor if needed, or just managed by main plugin
	
public slots:
	void refreshWindowList();
	void onOpacityChanged(int value);

	// Persistence methods
	QJsonObject saveToConfig();
	void loadFromConfig(const QJsonObject &data);

	// Snapshot methods
	void saveSnapshot(const QString &name, const QJsonObject &data);
	void deleteSnapshot(const QString &name);
	QStringList getSnapshotNames() const;
	
	// Prompts user if conflicts are detected.
	// Returns true if restore was successful or cancelled (handled internally).
	bool requestRestoreSnapshot(const QString &name, SecondaryWindow *target);

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

	QSlider *opacitySlider;
	QLabel *opacityLabel;

	// We need to track our windows.
	// We can't rely solely on QObject parentage if we want to manage them specifically.
	QList<SecondaryWindow*> managedWindows;
	int nextId = 0;
	
	QMap<QString, QJsonObject> snapshots;
};
