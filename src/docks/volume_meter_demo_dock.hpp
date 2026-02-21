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
#include "utils/volume_meter.hpp"

class VolumeMeterDemoDock : public QWidget {
	Q_OBJECT

public:
	explicit VolumeMeterDemoDock(QWidget *parent = nullptr);
	~VolumeMeterDemoDock() override;

	int getSelectedStyleIndex() const;
	void setSelectedStyleIndex(int index);

	void populateSourceComboBox();
	void clearMeters();
	static bool enumAudioSources(void *param, obs_source_t *source);

private slots:
	void addVolumeMeter();
	void updateMeterStyles();

private:
	QComboBox *sourceComboBox;
	QComboBox *styleComboBox;
	QPushButton *refreshButton;
	QPushButton *addButton;
	QVBoxLayout *metersLayout;
	QScrollArea *scrollArea;
	std::vector<QPointer<VolumeMeter>> volumeMeters;
};