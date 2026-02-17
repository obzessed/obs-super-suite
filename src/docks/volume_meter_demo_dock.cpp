#include "volume_meter_demo_dock.hpp"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QComboBox>
#include <QPushButton>
#include <QTimer>
#include <obs.hpp>
#include <obs-source.h>
#include <obs-frontend-api.h>
#include <algorithm>

static void refreshSources(obs_frontend_event event, void *data)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING || event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
		static_cast<VolumeMeterDemoDock *>(data)->populateSourceComboBox();
	}
}

VolumeMeterDemoDock::VolumeMeterDemoDock(QWidget *parent) : QWidget(parent)
{
	setWindowTitle("Volume Meter Demo");
	setMinimumWidth(400);

	QVBoxLayout *mainLayout = new QVBoxLayout(this);

	// Control row
	QHBoxLayout *controlLayout = new QHBoxLayout();
	mainLayout->addLayout(controlLayout);

	controlLayout->addWidget(new QLabel("Audio Source:"));
	sourceComboBox = new QComboBox(this);
	controlLayout->addWidget(sourceComboBox);

	refreshButton = new QPushButton("Refresh", this);
	connect(refreshButton, &QPushButton::clicked, this, &VolumeMeterDemoDock::populateSourceComboBox);
	controlLayout->addWidget(refreshButton);

	addButton = new QPushButton("Add Volume Meter", this);
	connect(addButton, &QPushButton::clicked, this, &VolumeMeterDemoDock::addVolumeMeter);
	controlLayout->addWidget(addButton);

	controlLayout->addWidget(new QLabel("Style:"));
	styleComboBox = new QComboBox(this);
	styleComboBox->addItem("Modern", static_cast<int>(VolumeMeter::Style::Modern));
	styleComboBox->addItem("Vintage", static_cast<int>(VolumeMeter::Style::Vintage));
	styleComboBox->addItem("Analog", static_cast<int>(VolumeMeter::Style::Analog));
	styleComboBox->addItem("Fluid", static_cast<int>(VolumeMeter::Style::Fluid));
	styleComboBox->setCurrentIndex(0); // Default to Modern
	controlLayout->addWidget(styleComboBox);

	connect(styleComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&VolumeMeterDemoDock::updateMeterStyles);

	controlLayout->addStretch();

	// Scroll area for meters
	scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	mainLayout->addWidget(scrollArea);

	QWidget *metersWidget = new QWidget();
	metersLayout = new QVBoxLayout(metersWidget);
	metersLayout->setAlignment(Qt::AlignTop);
	scrollArea->setWidget(metersWidget);

	populateSourceComboBox();

	// Auto-refresh sources when collection changes
	obs_frontend_add_event_callback(refreshSources, this);
}

VolumeMeterDemoDock::~VolumeMeterDemoDock()
{
	obs_frontend_remove_event_callback(refreshSources, this);

	// Clean up meters
	for (auto &meter : volumeMeters) {
		if (meter) {
			delete meter;
		}
	}
}

void VolumeMeterDemoDock::populateSourceComboBox()
{
	sourceComboBox->clear();
	sourceComboBox->addItem(QStringLiteral("Select Source"), QVariant());

	obs_enum_sources(enumAudioSources, this);
}

bool VolumeMeterDemoDock::enumAudioSources(void *param, obs_source_t *source)
{
	VolumeMeterDemoDock *dock = static_cast<VolumeMeterDemoDock *>(param);

	if (!source)
		return true;

	// Show all sources - the volume meter will only work for those with audio
	const char *name = obs_source_get_name(source);
	dock->sourceComboBox->addItem(name,
				      QVariant::fromValue(static_cast<void *>(obs_source_get_weak_source(source))));

	return true;
}

void VolumeMeterDemoDock::addVolumeMeter()
{
	int index = sourceComboBox->currentIndex();
	if (index <= 0)
		return; // No source selected

	obs_weak_source_t *weakSource =
		static_cast<obs_weak_source_t *>(sourceComboBox->itemData(index).value<void *>());
	obs_source_t *source = obs_weak_source_get_source(weakSource);
	if (!source)
		return;

	uint32_t flags = obs_source_get_output_flags(source);
	if ((flags & OBS_SOURCE_AUDIO) == 0) {
		obs_source_release(source);
		return; // Not an audio source
	}

	VolumeMeter::Style selectedStyle = static_cast<VolumeMeter::Style>(styleComboBox->currentData().toInt());

	VolumeMeter *meter = new VolumeMeter(nullptr, source, selectedStyle);
	obs_source_release(source);

	metersLayout->addWidget(meter);
	volumeMeters.push_back(meter);

	// Scroll to bottom
	QTimer::singleShot(0, [this]() {
		scrollArea->verticalScrollBar()->setValue(scrollArea->verticalScrollBar()->maximum());
	});
}

void VolumeMeterDemoDock::updateMeterStyles()
{
	VolumeMeter::Style selectedStyle = static_cast<VolumeMeter::Style>(styleComboBox->currentData().toInt());
	for (auto &meter : volumeMeters) {
		if (meter) {
			meter->setStyle(selectedStyle);
		}
	}
}

int VolumeMeterDemoDock::getSelectedStyleIndex() const
{
	return styleComboBox->currentIndex();
}

void VolumeMeterDemoDock::setSelectedStyleIndex(int index)
{
	styleComboBox->setCurrentIndex(index);
}