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

private slots:
	void addSource();
	void removeSelectedSource();
	void openSourceProperties(int row);
	void openSourceFilters(int row);

private:
	void setupUi();
	void loadFromConfig();
	void saveToConfig(bool refreshSources = true);
	void updateRemoveButtonState();
	void updateAddButtonState();
	void addRowWidgets(int row, const struct AsioSourceConfig &src);
	int findNextAvailableChannel() const;
	bool isChannelOccupied(int channel, int excludeRow = -1) const;
	void onChannelChanged(int row);
	void onItemChanged(QTableWidgetItem *item);

	QTableWidget *tableWidget;
	QPushButton *btnAdd;
	QPushButton *btnRemove;
};
