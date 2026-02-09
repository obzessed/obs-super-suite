#include "./mixer_channel.h"
#include <QMenu>
#include <QStackedLayout>
#include <QInputDialog>
#include <vector>
#include <obs.h>
#include <obs-frontend-api.h>
#include "../models/audio_channel_source_config.h"

MixerChannel::MixerChannel(obs_source_t *source, QWidget *parent)
	: QWidget(parent)
{
	setupUi();
	setSource(source);
}

MixerChannel::~MixerChannel()
{
	disconnectSource();
}

// ... (Constructor remains similar)

void MixerChannel::setupUi()
{
	setFixedWidth(90);
	setMinimumHeight(400);

	// Main Layout: VBox
	m_mainLayout = new QVBoxLayout(this);
	m_mainLayout->setContentsMargins(4, 4, 4, 4);
	m_mainLayout->setSpacing(4);

	// --- 1. Header ---
	auto *headerLayout = new QHBoxLayout();
	m_nameLabel = new QLabel("Track", this);
	m_nameLabel->setAlignment(Qt::AlignCenter);
	m_nameLabel->setStyleSheet("font-weight: bold; color: #ddd; font-size: 11px;");
	headerLayout->addWidget(m_nameLabel, 1);
	
	// Dropdown arrow
	auto *menuBtn = new QPushButton("v", this);
	menuBtn->setFixedSize(16, 16);
	menuBtn->setFlat(true);
	menuBtn->setStyleSheet("color: #888;");
	headerLayout->addWidget(menuBtn);
	
	m_mainLayout->addLayout(headerLayout);

	// --- 2. Filter List Container (Scroll Area) ---
	m_filtersScrollArea = new QScrollArea(this);
	m_filtersScrollArea->setWidgetResizable(true);
	m_filtersScrollArea->setFrameShape(QFrame::NoFrame);
	m_filtersScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_filtersScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_filtersScrollArea->setStyleSheet("background: transparent;");
	
	m_filtersContainer = new QWidget();
	m_filtersContainer->setObjectName("filtersContainer");
	m_filtersContainer->setStyleSheet("background: transparent;");
	
	m_filtersListLayout = new QVBoxLayout(m_filtersContainer);
	m_filtersListLayout->setSpacing(2);
	m_filtersListLayout->setContentsMargins(0, 0, 0, 0);
	// Align top so items don't spread out
	m_filtersListLayout->setAlignment(Qt::AlignTop);
	
	m_filtersScrollArea->setWidget(m_filtersContainer);
	m_mainLayout->addWidget(m_filtersScrollArea);

	// --- 3. Add Button ---
	m_addBtn = new QPushButton("+", this);
	m_addBtn->setObjectName("addBtn");
	m_addBtn->setFixedHeight(24);
	m_addBtn->setCursor(Qt::PointingHandCursor);
	m_addBtn->setToolTip("Add Filter");
	m_addBtn->setStyleSheet(
		"QPushButton#addBtn { background: #222; border: 1px solid #333; border-radius: 4px; color: #888; }"
		"QPushButton#addBtn:hover { background: #333; color: #fff; border: 1px solid #555; }"
		"QPushButton#addBtn:pressed { background: #111; }"
	);
	connect(m_addBtn, &QPushButton::clicked, this, &MixerChannel::onAddFilterClicked);
	m_mainLayout->addWidget(m_addBtn);



	// --- 4. Center Section (Meter | Fader | Controls) ---
	m_centerContainer = new QWidget(this);
	m_centerContainer->setObjectName("centerContainer");
	m_centerContainer->setStyleSheet("background: transparent;");
	
	auto *centerLayout = new QHBoxLayout(m_centerContainer);
	centerLayout->setContentsMargins(0, 0, 0, 0);
	centerLayout->setSpacing(2);
	
	// Left: Meter + Scale
	auto *meterLayout = new QVBoxLayout();
	meterLayout->setSpacing(0);
	
	// DB Scale Top
	auto *infLabel = new QLabel("-inf", this);
	infLabel->setStyleSheet("font-size: 8px; color: #666;");
	infLabel->setAlignment(Qt::AlignLeft);
	meterLayout->addWidget(infLabel);
	
	// Meter
	m_levelMeter = new MixerMeter(this);
	m_levelMeter->setFixedWidth(10);
	meterLayout->addWidget(m_levelMeter, 1);
	
	// DB Scale Bottom
	auto *dbLabelBot = new QLabel("-60", this);
	dbLabelBot->setStyleSheet("font-size: 8px; color: #666;");
	meterLayout->addWidget(dbLabelBot);
	
	centerLayout->addLayout(meterLayout);

	// Center: Fader + Values + Edit Button
	auto *faderLayout = new QVBoxLayout();
	faderLayout->setSpacing(4);
	
	// Row for Value + Edit Button (Above fader)
	auto *valEditLayout = new QHBoxLayout();
	valEditLayout->setSpacing(2);
	
	m_volDbLabel = new QLabel("0.0", this);
	m_volDbLabel->setAlignment(Qt::AlignCenter);
	m_volDbLabel->setFixedWidth(32);
	m_volDbLabel->setStyleSheet("font-size: 10px; color: white; font-weight: bold;");
	valEditLayout->addWidget(m_volDbLabel);
	
	// Moved EDIT Button here
	m_editBtn = new QPushButton("EDIT", this);
	m_editBtn->setFixedSize(32, 16); // Small button
	m_editBtn->setToolTip("Source Properties");
	m_editBtn->setStyleSheet(
		"QPushButton { font-size: 9px; font-weight: bold; background: #333; color: #ccc; border: 1px solid #444; border-radius: 3px; }"
		"QPushButton:hover { background: #444; color: #fff; border: 1px solid #666; }"
	);
	connect(m_editBtn, &QPushButton::clicked, this, &MixerChannel::onEditClicked);
	valEditLayout->addWidget(m_editBtn);
	
	faderLayout->addLayout(valEditLayout);

	// Slider
	m_volumeSlider = new QSlider(Qt::Vertical, this);
	m_volumeSlider->setRange(0, 100);
	m_volumeSlider->setValue(100);
	m_volumeSlider->setTickPosition(QSlider::NoTicks);
	m_volumeSlider->setFixedWidth(20);
	m_volumeSlider->setStyleSheet(
		"QSlider::groove:vertical { background: #333; width: 4px; border-radius: 2px; }"
		"QSlider::handle:vertical { background: #e0e0e0; height: 30px; margin: 0 -5px; border-radius: 2px; }"
		"QSlider::add-page:vertical { background: #333; }"
		"QSlider::sub-page:vertical { background: #333; }"
	);
	faderLayout->addWidget(m_volumeSlider, 1, Qt::AlignHCenter);
	
	centerLayout->addLayout(faderLayout, 1);

	// Right: Side Controls (Link/Cue/Mute) - Filters button removed (using list now)
	auto *sideLayout = new QVBoxLayout();
	sideLayout->setSpacing(4);
	sideLayout->setContentsMargins(0, 0, 0, 0);

	// Spacer to push controls down? Or align top?
	// Reference image didn't show them, but we keep them for functionality
	sideLayout->addStretch(1);
	
	m_linkBtn = new QPushButton("🔗", this);
	m_linkBtn->setFixedSize(36, 26);
	m_linkBtn->setCheckable(true);
	m_linkBtn->setStyleSheet(
		"QPushButton { font-weight: bold; font-size: 14px; background: transparent; border: 1px solid #555; border-radius: 4px; color: #888; }"
		"QPushButton:checked { border: 2px solid #4CAF50; color: #4CAF50; background: #0d2e11; }"
	);
	sideLayout->addWidget(m_linkBtn);

	m_cueBtn = new QPushButton("CUE", this);
	m_cueBtn->setFixedSize(36, 26);
	m_cueBtn->setCheckable(true);
	m_cueBtn->setStyleSheet(
		"QPushButton { font-weight: bold; font-size: 10px; background: transparent; border: 1px solid #555; border-radius: 4px; color: #888; }"
		"QPushButton:checked { border: 2px solid #03A9F4; color: #03A9F4; background: #0a2030; }"
	);
	sideLayout->addWidget(m_cueBtn);

	m_muteBtn = new QPushButton("MUTE", this);
	m_muteBtn->setFixedSize(36, 26);
	m_muteBtn->setCheckable(true);
	m_muteBtn->setStyleSheet(
		"QPushButton { font-weight: bold; font-size: 9px; background: transparent; border: 1px solid #555; border-radius: 4px; color: #888; }"
		"QPushButton:checked { border: 2px solid #F44336; color: #F44336; background: #301010; }"
	);
	sideLayout->addWidget(m_muteBtn);
	
	centerLayout->addLayout(sideLayout);
	m_mainLayout->addWidget(m_centerContainer, 1);

	// --- Footer ---
	m_deviceCombo = new QComboBox(this);
	m_deviceCombo->addItem("Select device");
	m_deviceCombo->setStyleSheet("QComboBox { background: #222; color: #ccc; border: 1px solid #444; border-radius: 3px; font-size: 10px; padding: 2px; } QComboBox::drop-down { border: none; }");
	m_mainLayout->addWidget(m_deviceCombo);

	// --- Global Styling ---
	setAttribute(Qt::WA_StyledBackground, true);
	setObjectName("MixerChannel");
	setStyleSheet(
		"#MixerChannel { background: #1a1a1a; border-radius: 6px; border: 1px solid #444; }"
		"#MixerChannel:hover { border: 1px solid #555; }"
	);
	
	// Connections
	connect(m_volumeSlider, &QSlider::valueChanged, this, &MixerChannel::onVolumeSliderChanged);
	connect(m_muteBtn, &QPushButton::clicked, this, &MixerChannel::onMuteClicked);
	connect(m_cueBtn, &QPushButton::clicked, this, &MixerChannel::onCueClicked);
	// onEditClicked already connected above
}

void MixerChannel::rebuildFiltersList()
{
	if (!m_source) return;
	
	// Clear existing
	QLayoutItem *item;
	while ((item = m_filtersListLayout->takeAt(0)) != nullptr) {
		if (item->widget()) delete item->widget();
		delete item;
	}
	
	int count = 0;
	// Re-populate
	obs_source_enum_filters(m_source, [](obs_source_t *parent, obs_source_t *filter, void *param) {
		MixerChannel *channel = static_cast<MixerChannel*>(param);
		QWidget *row = channel->createFilterRow(filter);
		channel->m_filtersListLayout->addWidget(row);
	}, this);
	
	// Calculate height
	count = m_filtersListLayout->count();
	
	// Row height = 18, Spacing = 2
	// Height = count * 18 + (count - 1) * 2
	int totalHeight = 0;
	if (count > 0) {
		totalHeight = count * 18 + (count - 1) * 2;
	}
	
	// Cap at 4 items: 4 * 18 + 3 * 2 = 78
	int maxHeight = 4 * 18 + 3 * 2; 
	
	if (totalHeight > maxHeight) {
		m_filtersScrollArea->setFixedHeight(maxHeight);
	} else {
		m_filtersScrollArea->setFixedHeight(totalHeight);
	}
}
// ... (Existing implementation)

void MixerChannel::setEffectsVisible(bool visible)
{
	m_filtersScrollArea->setVisible(visible);
	m_addBtn->setVisible(visible);
}

void MixerChannel::setFadersVisible(bool visible)
{
	m_centerContainer->setVisible(visible);
}

bool MixerChannel::areEffectsVisible() const
{
	return m_filtersScrollArea->isVisible();
}

bool MixerChannel::areFadersVisible() const
{
	return m_centerContainer->isVisible();
}



void MixerChannel::setSource(obs_source_t *source)
{
	if (m_source == source) return;
	
	disconnectSource();
	m_source = source;
	
	if (m_source) {
		// Get source name
		const char *name = obs_source_get_name(m_source);
		m_nameLabel->setText(name ? QString::fromUtf8(name) : "Channel");
		
		// Get current values
		float volume = obs_source_get_volume(m_source);
		bool muted = obs_source_muted(m_source);
		
		uint32_t monitoringType = obs_source_get_monitoring_type(m_source);
		bool cueActive = (monitoringType != OBS_MONITORING_TYPE_NONE);
		
		m_updatingFromSource = true;
		
		// Cubic Taper: Slider (0-100) -> linear volume (0.0-1.0)
		// vol = (slider/100)^3
		// slider = cbrt(vol) * 100
		float sliderVal = cbrtf(volume) * 100.0f;
		m_volumeSlider->setValue((int)sliderVal);
		
		float db = 20.0f * log10f(fmaxf(volume, 0.0001f));
		m_volDbLabel->setText(QString::number(db, 'f', 1));

		m_muteBtn->setChecked(muted);
		m_cueBtn->setChecked(cueActive);
		
		m_updatingFromSource = false;
		
		connectSource();
	} else {
		m_nameLabel->setText("---");
	}
}

QString MixerChannel::getSourceName() const
{
	if (!m_source) return QString();
	const char *name = obs_source_get_name(m_source);
	return name ? QString::fromUtf8(name) : QString();
}

QString MixerChannel::getSourceUuid() const
{
	if (!m_source) return QString();
	const char *uuid = obs_source_get_uuid(m_source);
	return uuid ? QString::fromUtf8(uuid) : QString();
}

void MixerChannel::connectSource()
{
	if (!m_source) return;

	// signals
	signal_handler_t *sh = obs_source_get_signal_handler(m_source);
	if (sh) {
		signal_handler_connect(sh, "rename", SourceRename, this);
		signal_handler_connect(sh, "volume", SourceVolume, this);
		signal_handler_connect(sh, "mute", SourceMute, this);
		// Filter signals
		signal_handler_connect(sh, "filter_add", SourceFilterAdd, this);
		signal_handler_connect(sh, "filter_remove", SourceFilterRemove, this);
		signal_handler_connect(sh, "reorder_filters", SourceFilterReorder, this);
		// "enable" signal on filters? Or source? 
		// For filters, the signal is on the filter source itself or the parent?
		// "item_visible" / "enable"? 
		// Actually, standard sources have "enable".
		// We'd need to connect to EACH filter's "enable" signal to update the dot.
	}

	// Meter
	m_volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_attach_source(m_volmeter, m_source);
	obs_volmeter_add_callback(m_volmeter, VolmeterCallback, this);
	
	// Initial Filter Build
	rebuildFiltersList();
}

void MixerChannel::disconnectSource()
{
	if (m_volmeter) {
		obs_volmeter_remove_callback(m_volmeter, VolmeterCallback, this);
		obs_volmeter_detach_source(m_volmeter);
		obs_volmeter_destroy(m_volmeter);
		m_volmeter = nullptr;
	}

	if (m_source) {
		signal_handler_t *sh = obs_source_get_signal_handler(m_source);
		if (sh) {
			signal_handler_disconnect(sh, "rename", SourceRename, this);
			signal_handler_disconnect(sh, "volume", SourceVolume, this);
			signal_handler_disconnect(sh, "mute", SourceMute, this);
		}
	}
}

void MixerChannel::VolmeterCallback(void *param, const float magnitude[MAX_AUDIO_CHANNELS],
	const float peak[MAX_AUDIO_CHANNELS], const float input_peak[MAX_AUDIO_CHANNELS])
{
	Q_UNUSED(peak);
	Q_UNUSED(input_peak);
	
	MixerChannel *channel = static_cast<MixerChannel*>(param);
	
	// Get max magnitude across channels
	float max_mag = 0.0f;
	// MAX_AUDIO_CHANNELS is typically 8 in OBS, but we should use what config says or just iterate generic max
	// The callback signature has fixed size array usually defined by MAX_AUDIO_CHANNELS
	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		if (magnitude[i] > max_mag) max_mag = magnitude[i];
	}

	// Update UI on main thread
	QMetaObject::invokeMethod(channel, [channel, max_mag] {
		channel->updateLevelMeter(max_mag);
	});
}

// ... (Existing Signal Handlers)

void MixerChannel::SourceFilterAdd(void *data, calldata_t *cd)
{
	MixerChannel *channel = static_cast<MixerChannel*>(data);
	QMetaObject::invokeMethod(channel, [channel] {
		channel->rebuildFiltersList(); // Full rebuild is safest for order
	});
}

void MixerChannel::SourceFilterRemove(void *data, calldata_t *cd)
{
	MixerChannel *channel = static_cast<MixerChannel*>(data);
	QMetaObject::invokeMethod(channel, [channel] {
		channel->rebuildFiltersList();
	});
}

void MixerChannel::SourceFilterReorder(void *data, calldata_t *cd)
{
	MixerChannel *channel = static_cast<MixerChannel*>(data);
	QMetaObject::invokeMethod(channel, [channel] {
		channel->rebuildFiltersList();
	});
}

void MixerChannel::FilterEnabled(void *data, calldata_t *cd)
{
	// This is a bit trickier, we need to find the specific row.
	// But rebuild is fast enough for now?
	MixerChannel *channel = static_cast<MixerChannel*>(data);
	QMetaObject::invokeMethod(channel, [channel] {
		channel->rebuildFiltersList();
	});
}

QWidget* MixerChannel::createFilterRow(obs_source_t *filter)
{
	QWidget *row = new QWidget(this);
	QHBoxLayout *layout = new QHBoxLayout(row);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(2);
	row->setFixedHeight(18); // Compact height
	
	bool enabled = obs_source_enabled(filter);
	const char *name = obs_source_get_name(filter);
	
	// Bypass Dot
	QPushButton *bypass = new QPushButton(row);
	bypass->setFixedSize(8, 8);
	bypass->setCheckable(true);
	bypass->setChecked(enabled);
	bypass->setStyleSheet(
		"QPushButton { border-radius: 4px; background: #333; border: 1px solid #555; }"
		"QPushButton:checked { background: #4CAF50; border: 1px solid #4CAF50; }"
		"QPushButton:hover { border: 1px solid #fff; }"
	);
	bypass->setToolTip(enabled ? "Disable Filter" : "Enable Filter");
	
	connect(bypass, &QPushButton::clicked, this, [this, filter](bool checked) {
		obs_source_set_enabled(filter, checked);
	});
	
	layout->addWidget(bypass);
	
	// Label
	QLabel *label = new QLabel(QString::fromUtf8(name), row);
	label->setStyleSheet("color: #bbb; font-size: 9px; line-height: 18px;"); // Smaller font
	layout->addWidget(label, 1);
	
	// Settings Button
	QPushButton *settings = new QPushButton("⎚", row); 
	settings->setFixedSize(14, 14); // Smaller
	settings->setStyleSheet(
		"QPushButton { border: none; color: #666; background: transparent; padding: 0px; margin: 0px; font-size: 10px; }"
		"QPushButton:hover { color: #fff; background: #333; border-radius: 2px; }"
	);
	settings->setToolTip("Filter Settings");
	
	connect(settings, &QPushButton::clicked, this, [this, filter] {
		onFilterSettings(filter);
	});
	
	layout->addWidget(settings);
	
	return row;
}

void MixerChannel::onFilterSettings(obs_source_t *filter)
{
	// How to open just one filter's properties?
	// OBS API `obs_frontend_open_source_filters` opens the dialog.
	// It doesn't seemingly allow focusing a specific filter?
	// We can just open the filters dialog.
	obs_frontend_open_source_filters(m_source);
}

void MixerChannel::onAddFilterClicked()
{
	// Opens context menu to add filter?
	// We can show the standard Add Filter menu?
	// `obs_frontend_open_source_filters` is the standard dialog which has an Add button.
	// Currently we can just open the filters dialog.
	obs_frontend_open_source_filters(m_source);
}

// ... (Existing Callbacks for Volmeter/Rename/Mute/Volume)

void MixerChannel::SourceRename(void *data, calldata_t *cd)
{
	MixerChannel *channel = static_cast<MixerChannel*>(data);
	const char *name = calldata_string(cd, "new_name");
	QString qName = name ? QString::fromUtf8(name) : "";
	
	QMetaObject::invokeMethod(channel, [channel, qName] {
		if (channel->m_nameLabel)
			channel->m_nameLabel->setText(qName);
	});
}

void MixerChannel::SourceVolume(void *data, calldata_t *cd)
{
	MixerChannel *channel = static_cast<MixerChannel*>(data);
	double vol = calldata_float(cd, "volume");
	QMetaObject::invokeMethod(channel, [channel, vol] {
		channel->updateVolume((float)vol);
	});
}

void MixerChannel::SourceMute(void *data, calldata_t *cd)
{
	MixerChannel *channel = static_cast<MixerChannel*>(data);
	bool muted = calldata_bool(cd, "muted");
	QMetaObject::invokeMethod(channel, [channel, muted] {
		channel->updateMute(muted);
	});
}

void MixerChannel::onVolumeSliderChanged(int value)
{
	if (m_updatingFromSource || !m_source) return;
	
	// Cubic Taper: vol = (value/100)^3
	float norm = value / 100.0f;
	float volume = norm * norm * norm;
	
	obs_source_set_volume(m_source, volume);
	
	// Update dB label
	float db = 20.0f * log10f(fmaxf(volume, 0.0001f));
	m_volDbLabel->setText(QString::number(db, 'f', 1));
	
	emit volumeChanged(volume);
}

void MixerChannel::onMuteClicked()
{
	if (!m_source) return;
	
	bool muted = m_muteBtn->isChecked();
	obs_source_set_muted(m_source, muted);
	emit muteChanged(muted);
}

void MixerChannel::onCueClicked()
{
	if (!m_source) return;
	
	bool active = m_cueBtn->isChecked();
	// Toggle between Monitor Only (or Monitor+Output) and None
	obs_monitoring_type type = active ? OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT : OBS_MONITORING_TYPE_NONE;
	
	obs_source_set_monitoring_type(m_source, type);
	emit monitoringChanged((int)type);
}

void MixerChannel::updateVolume(float volume)
{
	m_updatingFromSource = true;
	float sliderVal = cbrtf(volume) * 100.0f;
	m_volumeSlider->setValue((int)sliderVal);
	
	float db = 20.0f * log10f(fmaxf(volume, 0.0001f));
	m_volDbLabel->setText(QString::number(db, 'f', 1));
	m_updatingFromSource = false;
}

void MixerChannel::updateMute(bool muted)
{
	m_updatingFromSource = true;
	m_muteBtn->setChecked(muted);
	m_updatingFromSource = false;
}

void MixerChannel::contextMenuEvent(QContextMenuEvent *event)
{
	QMenu menu; // No parent (stack allocated)
	
	// Reorder Actions
	QAction *moveLeft = menu.addAction("Move Left");
	moveLeft->setEnabled(AudioChSrcConfig::get()->canMoveSourceLeft(getSourceUuid()));
	connect(moveLeft, &QAction::triggered, this, &MixerChannel::moveLeftRequest);
	
	QAction *moveRight = menu.addAction("Move Right");
	moveRight->setEnabled(AudioChSrcConfig::get()->canMoveSourceRight(getSourceUuid()));
	connect(moveRight, &QAction::triggered, this, &MixerChannel::moveRightRequest);
	
	menu.addSeparator();
	
	// Rename
	QAction *rename = menu.addAction("Rename");
	connect(rename, &QAction::triggered, this, [this] {
		bool ok;
		QString oldName = m_nameLabel->text();
		QString newName = QInputDialog::getText(this, "Rename Channel", "New Name:", QLineEdit::Normal, oldName, &ok);
		if (ok && !newName.isEmpty()) {
			if (m_source) {
				obs_source_set_name(m_source, newName.toUtf8().constData());
			}
			emit renameRequest(); // Signal parent to refresh/update config
		}
	});
	
	menu.exec(event->globalPos());
}

void MixerChannel::updateMonitoringType(int type)
{
	m_updatingFromSource = true;
	m_cueBtn->setChecked(type != OBS_MONITORING_TYPE_NONE);
	m_updatingFromSource = false;
}

void MixerChannel::updateBalance(float balance)
{
	// Not used in new UI
	Q_UNUSED(balance);
}

void MixerChannel::updateLevelMeter(float level)
{
	// level is typically 0.0 - 1.0 (or higher if +dB)
	// Convert to dB
	float db = 20.0f * log10(fmax(level, 0.0001f));
	m_levelMeter->setLevel(db);
}

void MixerChannel::onEditClicked()
{
	if (!m_source) return;
	obs_frontend_open_source_properties(m_source);
}

// onFiltersClicked and updateFiltersCount removed as they are obsolete with the new list UI
