#pragma once

#include <obs.h>
#include <obs-module.h>

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QComboBox>
#include <QCheckBox>
#include <QTimer>
#include <QScrollArea>

#include "mixer_meter.hpp"

class MixerChannel : public QWidget {
	Q_OBJECT

public:
	explicit MixerChannel(obs_source_t *source, QWidget *parent = nullptr);
	~MixerChannel() override;

	void setSource(obs_source_t *source);
	obs_source_t *getSource() const { return m_source; }
	QString getSourceName() const;
	QString getSourceUuid() const;

	// External update methods (called from signal handlers)
	void updateVolume(float volume);
	void updateMute(bool muted);
	void updateBalance(float balance);
	void updateLevelMeter(float level);
	void updateMonitoringType(int type);

	// Visibility Controls
	void setEffectsVisible(bool visible);
	void setFadersVisible(bool visible);
	bool areEffectsVisible() const;
	bool areFadersVisible() const;

signals:
	void volumeChanged(float volume);
	void muteChanged(bool muted);
	void balanceChanged(float balance);
	void monitoringChanged(int type);
	
	// Context Menu Signals
	void moveLeftRequest();
	void moveRightRequest();
	void renameRequest();

private slots:
	void onVolumeSliderChanged(int value);
	void onMuteClicked();
	void onCueClicked();
	void onEditClicked();
	// onFiltersClicked removed
	void onAddFilterClicked();
	
	// Filter row slots
	// onFilterBypass handled via lambda
	void onFilterSettings(obs_source_t *filter);

private:
	void setupUi();
	void connectSource();
	void disconnectSource();
	void rebuildFiltersList();
	QWidget* createFilterRow(obs_source_t *filter);

	static void SourceRename(void *data, calldata_t *cd);
	static void SourceVolume(void *data, calldata_t *cd);
	static void SourceMute(void *data, calldata_t *cd);
	static void VolmeterCallback(void *param, const float magnitude[MAX_AUDIO_CHANNELS],
		const float peak[MAX_AUDIO_CHANNELS], const float input_peak[MAX_AUDIO_CHANNELS]);

	// Filter signals
	static void SourceFilterAdd(void *data, calldata_t *cd);
	static void SourceFilterRemove(void *data, calldata_t *cd);
	static void SourceFilterReorder(void *data, calldata_t *cd);
	static void FilterEnabled(void *data, calldata_t *cd); // To update bypass dot
	static void SourceDestroyed(void *data, calldata_t *cd);

	obs_source_t *m_source = nullptr;
	obs_volmeter_t *m_volmeter = nullptr;

	// UI elements
	QVBoxLayout *m_mainLayout; // Changed from Grid to VBox for the stack
	QScrollArea *m_filtersScrollArea;
	QWidget *m_filtersContainer;
	QVBoxLayout *m_filtersListLayout;
	
	// Header
	QLabel *m_nameLabel;
	QPushButton *m_addBtn; // "+" button, now wide
	
	// Main Section
	QWidget *m_centerContainer; // Container for Meter/Fader/Controls
	MixerMeter *m_levelMeter;
	QSlider *m_volumeSlider;
	QLabel *m_volDbLabel;
	QPushButton *m_editBtn; // Now in meter area?
	
	// Side Controls (Remaining ones)
	QPushButton *m_linkBtn;
	QPushButton *m_cueBtn;
	QPushButton *m_muteBtn;
	QComboBox *m_deviceCombo;
	// m_filtersBtn and m_filtersBadge removed

protected:
	void contextMenuEvent(QContextMenuEvent *event) override;

	// State
	bool m_updatingFromSource = false;
};
