#pragma once

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QPointer>
#include <QTimer>
#include <vector>
#include <mutex>
#include <obs.hpp>

class VolumeMeter;

// Represents a single tapped audio stream
class AdvancedMonitoringRow : public QWidget {
	Q_OBJECT

public:
	enum class TapType {
		PreFilter,
		PostFilter,
		PostMixer,
		Track
	};

	explicit AdvancedMonitoringRow(QWidget *parent = nullptr);
	~AdvancedMonitoringRow() override;

	void populateSources();
	void populateTracks();
	
signals:
	void removeRequested(AdvancedMonitoringRow *row);

private slots:
	void onTypeChanged(int index);
	void onTargetChanged(int index);
	void onRemoveClicked();

private:
	void setupUi();
	void connectAudio();
	void disconnectAudio();

	// OBS Callbacks
	static void obs_audio_capture_cb(void *param, obs_source_t *source, const struct audio_data *audio_data, bool muted);
	static void obs_track_audio_cb(void *param, size_t mix_idx, struct audio_data *data);

	// Internal Filter routing
	obs_source_t* createHiddenFilter();

	TapType currentType = TapType::PreFilter;
	obs_source_t* currentSource = nullptr;
	obs_source_t* hiddenFilter = nullptr;
	int currentTrack = 0;

	// UI Elements
	QComboBox *typeCombo;
	QComboBox *targetCombo;
	QComboBox *deviceCombo; // Placeholder for future physical routing
	VolumeMeter *vuMeter;
	QPushButton *removeBtn;
};

// The main dock container
class AdvancedMonitoringDock : public QWidget {
	Q_OBJECT

public:
	explicit AdvancedMonitoringDock(QWidget *parent = nullptr);
	~AdvancedMonitoringDock() override;

	void disconnectAll();

private slots:
	void addMonitorRow();
	void removeRow(AdvancedMonitoringRow *row);

private:
	QPushButton *addBtn;
	QVBoxLayout *rowsLayout;
	QScrollArea *scrollArea;
	std::vector<QPointer<AdvancedMonitoringRow>> monitorRows;
};
