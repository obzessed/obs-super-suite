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

class AudioSourceDialog;

class AudioChannelsDialog : public QDialog {
	Q_OBJECT
	static AudioChannelsDialog* instance_; // Singleton instance pointer for global access in signal handlers
public:
	explicit AudioChannelsDialog(QWidget *parent = nullptr);
	~AudioChannelsDialog() override;

	static AudioChannelsDialog *getInstance();

	void toggle_show_hide();
	
	// Update methods called by signal handlers to keep UI in sync (use UUID for unique identification)
	void updateSourceSettings(const QString &sourceUuid, const QJsonObject &settings);
	void updateSourceFilters(const QString &sourceUuid, const QJsonArray &filters);
	void updateSourceName(const QString &sourceUuid, const QString &name);
	void updateSourceMuted(const QString &sourceUuid, bool muted);
	void updateSourceVolume(const QString &sourceUuid, float volume);
	void updateSourceBalance(const QString &sourceUuid, float balance);
	void updateSourceMonitoring(const QString &sourceUuid, int type);
	void updateSourceMono(const QString &sourceUuid, bool mono);
	void updateSourceAudioMixers(const QString &sourceUuid, uint32_t mixers);
	void updateSourceAudioActive(const QString &sourceUuid, bool active);
	void updateSpeakerLayoutByUuid(const QString &sourceUuid);
	void updateSourceUuid(int configIndex, const QString &uuid);
	void updateSourceNameByIndex(int configIndex, const QString &name);

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
	void updateActiveIndicator(int row);
	void updateSpeakerLayout(int row);
	int findNextAvailableChannel(const QString &canvasUuid = QString()) const;
	QString generateUniqueName(const QString &baseName) const;

	QTableWidget *tableWidget;
	QPushButton *btnAdd;
	QPushButton *btnRemove;
};

