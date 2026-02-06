#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QKeyEvent>
#include <QGraphicsColorizeEffect>

class AsioSourceDialog;

class AsioSettingsDialog : public QDialog {
	Q_OBJECT

public:
	explicit AsioSettingsDialog(QWidget *parent = nullptr);
	~AsioSettingsDialog();

	void toggle_show_hide();
	
	// Update methods called by signal handlers to keep UI in sync
	void updateSourceSettings(int channel, const QJsonObject &settings);
	void updateSourceFilters(int channel, const QJsonArray &filters);
	void updateSourceName(int channel, const QString &name);
	void updateSourceMuted(int channel, bool muted);
	void updateSourceVolume(int channel, float volume);
	void updateSourceBalance(int channel, float balance);
	void updateSourceMonitoring(int channel, int type);
	void updateSourceMono(int channel, bool mono);

protected:
	void showEvent(QShowEvent *event) override;
	void closeEvent(QCloseEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	bool eventFilter(QObject *obj, QEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;

private slots:
	void addSource();
	void editSource(int row);
	void duplicateSource(int row);
	void deleteSource(int row);
	void removeSelectedSource();
	void openSourceProperties(int row);
	void openSourceFilters(int row);
	void showContextMenu(const QPoint &pos);

private:
	void setupUi();
	void loadFromConfig();
	void saveToConfig(bool refreshSources = true);
	void updateRemoveButtonState();
	void updateAddButtonState();
	void addRowWidgets(int row, const struct AsioSourceConfig &src);
	void updateRowTooltip(int row);
	int findNextAvailableChannel(const QString &canvasUuid = QString()) const;
	QString generateUniqueName(const QString &baseName) const;

	QTableWidget *tableWidget;
	QPushButton *btnAdd;
	QPushButton *btnRemove;
};

