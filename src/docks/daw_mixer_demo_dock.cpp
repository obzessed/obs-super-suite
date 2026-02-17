#include "daw_mixer_demo_dock.hpp"

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

static void refreshSources(obs_frontend_event event, void *data)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING || event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
		static_cast<DawMixerDemoDock *>(data)->populateSourceComboBox();
	}
}

DawMixerDemoDock::DawMixerDemoDock(QWidget *parent) : QWidget(parent)
{
	setWindowTitle("DAW Mixer Demo");
	setMinimumWidth(400);

	QVBoxLayout *mainLayout = new QVBoxLayout(this);

	// Control row
	QHBoxLayout *controlLayout = new QHBoxLayout();
	mainLayout->addLayout(controlLayout);

	controlLayout->addWidget(new QLabel("Audio Source:"));
	sourceComboBox = new QComboBox(this);
	controlLayout->addWidget(sourceComboBox);

	refreshButton = new QPushButton("Refresh", this);
	connect(refreshButton, &QPushButton::clicked, this, &DawMixerDemoDock::populateSourceComboBox);
	controlLayout->addWidget(refreshButton);

	addButton = new QPushButton("Add Channel", this);
	connect(addButton, &QPushButton::clicked, this, &DawMixerDemoDock::addMixerChannel);
	controlLayout->addWidget(addButton);

	controlLayout->addStretch();

	// Scroll area for channels (Horizontal scrolling for mixer strips)
	scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	mainLayout->addWidget(scrollArea);

	QWidget *channelsWidget = new QWidget();
	channelsLayout = new QHBoxLayout(channelsWidget);
	channelsLayout->setAlignment(Qt::AlignLeft);
	channelsLayout->setSpacing(4);
	channelsLayout->setContentsMargins(4, 4, 4, 4);
	scrollArea->setWidget(channelsWidget);

	populateSourceComboBox();

	// Auto-refresh sources when collection changes
	obs_frontend_add_event_callback(refreshSources, this);
}

DawMixerDemoDock::~DawMixerDemoDock()
{
	obs_frontend_remove_event_callback(refreshSources, this);

	// Clean up channels
	for (auto &channel : mixerChannels) {
		if (channel) {
			delete channel;
		}
	}
}

void DawMixerDemoDock::populateSourceComboBox()
{
	sourceComboBox->clear();
	sourceComboBox->addItem(QStringLiteral("Select Source"), QVariant());

	obs_enum_sources(enumAudioSources, this);
}

bool DawMixerDemoDock::enumAudioSources(void *param, obs_source_t *source)
{
	DawMixerDemoDock *dock = static_cast<DawMixerDemoDock *>(param);

	if (!source)
		return true;

	// Show all sources - the mixer channel will only work for those with audio
	const char *name = obs_source_get_name(source);
	dock->sourceComboBox->addItem(name,
				      QVariant::fromValue(static_cast<void *>(obs_source_get_weak_source(source))));

	return true;
}

void DawMixerDemoDock::addMixerChannel()
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

	DawMixerChannel *channel = new DawMixerChannel(nullptr, source);
	obs_source_release(source);

	channelsLayout->addWidget(channel);
	mixerChannels.push_back(channel);

	// Scroll to right
	QTimer::singleShot(0, [this]() {
		scrollArea->horizontalScrollBar()->setValue(scrollArea->horizontalScrollBar()->maximum());
	});
}
