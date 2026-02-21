#include "advanced_monitoring_dock.hpp"
#include "utils/volume_meter.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <media-io/audio-io.h>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QMessageBox>
#include <math.h>

// Forward declare the hidden filter register
extern "C" void register_hidden_monitor_filter();

// === Row Implementation ===

AdvancedMonitoringRow::AdvancedMonitoringRow(QWidget *parent) : QWidget(parent)
{
	setupUi();
	populateSources();
}

AdvancedMonitoringRow::~AdvancedMonitoringRow()
{
	disconnectAudio();
}

void AdvancedMonitoringRow::setupUi()
{
	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);

	typeCombo = new QComboBox(this);
	typeCombo->addItem("Source Pre-Filter", static_cast<int>(TapType::PreFilter));
	typeCombo->addItem("Source Post-Filter", static_cast<int>(TapType::PostFilter));
	typeCombo->addItem("Source Post-Mixer", static_cast<int>(TapType::PostMixer));
	typeCombo->addItem("Master Track", static_cast<int>(TapType::Track));
	layout->addWidget(typeCombo);
	
	targetCombo = new QComboBox(this);
	layout->addWidget(targetCombo);

	deviceCombo = new QComboBox(this);
	deviceCombo->addItem("System Default Device");
	deviceCombo->setEnabled(false); // Deferred to Phase 2
	layout->addWidget(deviceCombo);

	vuMeter = new VolumeMeter(this, nullptr, VolumeMeter::Style::Modern);
	vuMeter->setMinimumWidth(150);
	vuMeter->setFixedHeight(20);
	layout->addWidget(vuMeter, 1);

	removeBtn = new QPushButton("X", this);
	removeBtn->setFixedWidth(30);
	layout->addWidget(removeBtn);

	connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AdvancedMonitoringRow::onTypeChanged);
	connect(targetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AdvancedMonitoringRow::onTargetChanged);
	connect(removeBtn, &QPushButton::clicked, this, &AdvancedMonitoringRow::onRemoveClicked);
}

void AdvancedMonitoringRow::populateSources()
{
	targetCombo->blockSignals(true);
	targetCombo->clear();
	targetCombo->addItem("Select Source...", QVariant());

	auto enumAudioSources = [](void *param, obs_source_t *source) {
		AdvancedMonitoringRow *row = static_cast<AdvancedMonitoringRow*>(param);
		if ((obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) != 0) {
			row->targetCombo->addItem(obs_source_get_name(source), QVariant::fromValue(static_cast<void*>(obs_source_get_weak_source(source))));
		}
		return true;
	};

	obs_enum_sources(enumAudioSources, this);
	targetCombo->blockSignals(false);
}

void AdvancedMonitoringRow::populateTracks()
{
	targetCombo->blockSignals(true);
	targetCombo->clear();
	for (int i = 0; i < 6; i++) {
		targetCombo->addItem(QString("Track %1").arg(i + 1), i);
	}
	targetCombo->blockSignals(false);
}

void AdvancedMonitoringRow::onTypeChanged(int index)
{
	disconnectAudio();
	
	currentType = static_cast<TapType>(typeCombo->itemData(index).toInt());
	if (currentType == TapType::Track) {
		populateTracks();
	} else {
		populateSources();
	}
}

void AdvancedMonitoringRow::onTargetChanged(int index)
{
	disconnectAudio();
	
	if (index <= 0 && currentType != TapType::Track) return; // "Select..."

	if (currentType == TapType::Track) {
		currentTrack = targetCombo->itemData(index).toInt();
		connectAudio();
	} else {
		obs_weak_source_t *ws = static_cast<obs_weak_source_t*>(targetCombo->itemData(index).value<void*>());
		currentSource = obs_weak_source_get_source(ws);
		if (currentSource) {
			connectAudio();
		}
	}
}

void AdvancedMonitoringRow::onRemoveClicked()
{
	emit removeRequested(this);
}

void AdvancedMonitoringRow::connectAudio()
{
	if (currentType == TapType::PreFilter && currentSource) {
		obs_source_add_audio_capture_callback(currentSource, obs_audio_capture_cb, this);
	} 
	else if ((currentType == TapType::PostFilter || currentType == TapType::PostMixer) && currentSource) {
		// Create and attach our hidden filter
		obs_data_t *settings = obs_data_create();
		obs_data_set_int(settings, "row_ptr", reinterpret_cast<long long>(this));
		
		hiddenFilter = obs_source_create_private("super_advanced_monitor_filter", "Hidden Monitor", settings);
		obs_data_release(settings);
		
		if (hiddenFilter) {
			obs_source_filter_add(currentSource, hiddenFilter);
		}
	}
	else if (currentType == TapType::Track) {
		audio_t *audio = obs_get_audio();
		if (audio) {
			struct audio_convert_info conversion = {};
			conversion.format = AUDIO_FORMAT_FLOAT_PLANAR;
			conversion.speakers = SPEAKERS_STEREO; // Assuming stereo for meter
			audio_output_connect(audio, currentTrack, &conversion, obs_track_audio_cb, this);
		}
	}
}

void AdvancedMonitoringRow::disconnectAudio()
{
	if (currentType == TapType::PreFilter && currentSource) {
		obs_source_remove_audio_capture_callback(currentSource, obs_audio_capture_cb, this);
	}
	else if ((currentType == TapType::PostFilter || currentType == TapType::PostMixer) && currentSource && hiddenFilter) {
		obs_source_filter_remove(currentSource, hiddenFilter);
		obs_source_release(hiddenFilter);
		hiddenFilter = nullptr;
	}
	else if (currentType == TapType::Track) {
		audio_t *audio = obs_get_audio();
		if (audio) {
			audio_output_disconnect(audio, currentTrack, obs_track_audio_cb, this);
		}
	}

	if (currentSource) {
		obs_source_release(currentSource);
		currentSource = nullptr;
	}
}

// Static Callbacks
void AdvancedMonitoringRow::obs_audio_capture_cb(void *param, obs_source_t *source, const struct audio_data *audio_data, bool muted)
{
	UNUSED_PARAMETER(source);
	UNUSED_PARAMETER(muted);
	// Handle metering (requires volume meter API to parse RAW)
	// TODO: Send to QAudioSink for playback
}

void AdvancedMonitoringRow::obs_track_audio_cb(void *param, size_t mix_idx, struct audio_data *data)
{
	// Handle track metering
	// TODO: Send to QAudioSink
}


// === Dock Implementation ===

AdvancedMonitoringDock::AdvancedMonitoringDock(QWidget *parent) : QWidget(parent)
{
	setWindowTitle("Advanced Monitoring");
	setMinimumSize(400, 200);

	QVBoxLayout *mainLayout = new QVBoxLayout(this);

	QHBoxLayout *toolbar = new QHBoxLayout();
	addBtn = new QPushButton("Add Monitor Tap", this);
	toolbar->addWidget(addBtn);
	toolbar->addStretch();
	mainLayout->addLayout(toolbar);

	scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);
	
	QWidget *scrollWidget = new QWidget();
	rowsLayout = new QVBoxLayout(scrollWidget);
	rowsLayout->setAlignment(Qt::AlignTop);
	scrollArea->setWidget(scrollWidget);
	
	mainLayout->addWidget(scrollArea);

	connect(addBtn, &QPushButton::clicked, this, &AdvancedMonitoringDock::addMonitorRow);
}

AdvancedMonitoringDock::~AdvancedMonitoringDock()
{
	disconnectAll();
}

void AdvancedMonitoringDock::disconnectAll()
{
	for (auto &row : monitorRows) {
		if (row) {
			delete row;
		}
	}
	monitorRows.clear();
}

void AdvancedMonitoringDock::addMonitorRow()
{
	AdvancedMonitoringRow *row = new AdvancedMonitoringRow(this);
	connect(row, &AdvancedMonitoringRow::removeRequested, this, &AdvancedMonitoringDock::removeRow);
	rowsLayout->addWidget(row);
	monitorRows.push_back(row);
}

void AdvancedMonitoringDock::removeRow(AdvancedMonitoringRow *row)
{
	auto it = std::find(monitorRows.begin(), monitorRows.end(), row);
	if (it != monitorRows.end()) {
		rowsLayout->removeWidget(*it);
		delete *it;
		monitorRows.erase(it);
	}
}

// === Hidden OBS Filter Definition ===
struct mon_filter_data {
	AdvancedMonitoringRow* row;
};

static void* mon_filter_create(obs_data_t *settings, obs_source_t *context)
{
	UNUSED_PARAMETER(context);
	auto *data = new mon_filter_data();
	data->row = reinterpret_cast<AdvancedMonitoringRow*>(obs_data_get_int(settings, "row_ptr"));
	return data;
}

static void mon_filter_destroy(void *data)
{
	delete static_cast<mon_filter_data*>(data);
}

static struct obs_audio_data* mon_filter_audio(void *data, struct obs_audio_data *audio)
{
	auto *filterData = static_cast<mon_filter_data*>(data);
	if (filterData->row) {
		// Pass `audio` to the row for handling
		// filterData->row->handle_post_filter_audio(audio);
	}
	return audio;
}

static const char* mon_filter_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return "Hidden Monitor Filter";
}

extern "C" void register_hidden_monitor_filter()
{
	struct obs_source_info info = {};
	info.id = "super_advanced_monitor_filter";
	info.type = OBS_SOURCE_TYPE_FILTER;
	info.version = 0x00000001;
	info.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_CAP_DISABLED;
	info.create = mon_filter_create;
	info.destroy = mon_filter_destroy;
	info.get_name = mon_filter_get_name;
	info.filter_audio = mon_filter_audio;
	
	obs_register_source(&info);
}
