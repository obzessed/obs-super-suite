#pragma once

#include <obs.h>
#include <obs-module.h>

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>

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

signals:
	void volumeChanged(float volume);
	void muteChanged(bool muted);
	void balanceChanged(float balance);

private slots:
	void onVolumeSliderChanged(int value);
	void onBalanceSliderChanged(int value);
	void onMuteClicked();

private:
	void setupUi();
	void connectSource();
	void disconnectSource();

	obs_source_t *m_source = nullptr;

	// UI elements
	QVBoxLayout *m_layout;
	QLabel *m_levelMeter;
	QSlider *m_volumeSlider;
	QSlider *m_balanceSlider;
	QPushButton *m_muteBtn;
	QLabel *m_nameLabel;

	// State
	bool m_updatingFromSource = false;
};
