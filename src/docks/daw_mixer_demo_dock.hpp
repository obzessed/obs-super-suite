#pragma once

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QPointer>
#include <vector>
#include "components/daw_mixer_channel.hpp"

class DawMixerDemoDock : public QWidget {
	Q_OBJECT

public:
	explicit DawMixerDemoDock(QWidget *parent = nullptr);
	~DawMixerDemoDock() override;

	void populateSourceComboBox();
	static bool enumAudioSources(void *param, obs_source_t *source);

private slots:
	void addMixerChannel();

private:
	QComboBox *sourceComboBox;
	QPushButton *refreshButton;
	QPushButton *addButton;
	QHBoxLayout *channelsLayout; // Horizontal layout for mixer channels
	QScrollArea *scrollArea;
	std::vector<QPointer<DawMixerChannel>> mixerChannels;
};
