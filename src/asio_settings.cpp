#include <obs-module.h>
#include <obs-frontend-api.h>

#include <plugin-support.h>

#include "asio_settings.h"
#include "asio_source_dialog.h"
#include "asio_config.h"
#include "super_suite.h"

#include <QShowEvent>
#include <QCloseEvent>
#include <QHideEvent>
#include <QMessageBox>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>

enum TableItemUserDataSlot: int {
	kTIDS_SourceSettings = Qt::UserRole + 0,
	kTIDS_SourceFilters = Qt::UserRole + 1,
	kTIDS_AudioMuted = Qt::UserRole + 2,
	kTIDS_AudioMonitoringType = Qt::UserRole + 3,
	kTIDS_AudioVolume = Qt::UserRole + 4,
	kTIDS_AudioBalance = Qt::UserRole + 5,
	kTIDS_AudioForceMono = Qt::UserRole + 6,
	kTIDS_OutputChannel = Qt::UserRole + 7,
	kTIDS_Canvas = Qt::UserRole + 8,
	kTIDS_SourceType = Qt::UserRole + 9,
	kTIDS_SourceUuid = Qt::UserRole + 10,
	kTIDS_AudioMixers = Qt::UserRole + 11,
	kTIDS_AudioActive = Qt::UserRole + 12,
};

// Helper function to convert speaker_layout enum to decimal notation
static QString speakerLayoutToString(enum speaker_layout layout)
{
	switch (layout) {
	case SPEAKERS_UNKNOWN: return "-";
	case SPEAKERS_MONO: return "1";
	case SPEAKERS_STEREO: return "2";
	case SPEAKERS_2POINT1: return "2.1";
	case SPEAKERS_4POINT0: return "4.0";
	case SPEAKERS_4POINT1: return "4.1";
	case SPEAKERS_5POINT1: return "5.1";
	case SPEAKERS_7POINT1: return "7.1";
	default: return QString::number((int)layout);
	}
}

AsioSettingsDialog::AsioSettingsDialog(QWidget *parent)
	: QDialog(parent),
	  tableWidget(nullptr),
	  btnAdd(nullptr),
	  btnRemove(nullptr)
{
	setupUi();
	loadFromConfig();
}

AsioSettingsDialog::~AsioSettingsDialog()
{
	// Don't save here - we save on every change in the UI
	// Saving during shutdown could access cleaned-up resources
}

void AsioSettingsDialog::setupUi()
{
	setWindowTitle(obs_module_text("AsioSettings.Title"));
	resize(700, 400);

	auto *mainLayout = new QVBoxLayout(this);

	// Info label
	auto *infoLabel = new QLabel(obs_module_text("AsioSettings.Info"), this);
	infoLabel->setWordWrap(true);
	mainLayout->addWidget(infoLabel);

	// Table - 15 columns: Active, Name, Ch, Spk, Canvas, Vol, Bal, Mute, Mono, Monitor, Mixer, Props, Filters, Edit, Delete
	tableWidget = new QTableWidget(this);
	tableWidget->setColumnCount(15);
	tableWidget->setHorizontalHeaderLabels({
		"",         // Active indicator
		obs_module_text("AsioSettings.SourceName"),
		"Ch",       // Channel (read-only)
		"Spk",      // Speaker layout (read-only)
		obs_module_text("AsioSettings.Canvas"),  // Canvas
		"Vol",      // Volume
		"Bal",      // Balance
		"M",        // Mute
		"Mo",       // Mono
		"Mon",      // Monitoring
		"Mx",       // Show in Mixer (audio active)
		"",         // Configure button
		"",         // Filters button
		"",         // Edit button
		""          // Delete button column
	});

	tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
	tableWidget->setColumnWidth(0, 25);   // Active
	tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
	tableWidget->setColumnWidth(2, 35);   // Channel
	tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
	tableWidget->setColumnWidth(3, 35);   // Speaker layout
	tableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
	tableWidget->setColumnWidth(4, 60);   // Canvas
	tableWidget->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
	tableWidget->setColumnWidth(5, 80);   // Volume
	tableWidget->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
	tableWidget->setColumnWidth(6, 80);   // Balance
	tableWidget->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Fixed);
	tableWidget->setColumnWidth(7, 30);   // Mute
	tableWidget->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Fixed);
	tableWidget->setColumnWidth(8, 30);   // Mono
	tableWidget->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Fixed);
	tableWidget->setColumnWidth(9, 70);   // Monitoring
	tableWidget->horizontalHeader()->setSectionResizeMode(10, QHeaderView::Fixed);
	tableWidget->setColumnWidth(10, 30);  // Mixer
	tableWidget->horizontalHeader()->setSectionResizeMode(11, QHeaderView::Fixed);
	tableWidget->setColumnWidth(11, 30);  // Configure
	tableWidget->horizontalHeader()->setSectionResizeMode(12, QHeaderView::Fixed);
	tableWidget->setColumnWidth(12, 30);  // Filters
	tableWidget->horizontalHeader()->setSectionResizeMode(13, QHeaderView::Fixed);
	tableWidget->setColumnWidth(13, 30);  // Edit
	tableWidget->horizontalHeader()->setSectionResizeMode(14, QHeaderView::Fixed);
	tableWidget->setColumnWidth(14, 30);  // Delete

	tableWidget->verticalHeader()->setVisible(false);
	tableWidget->verticalHeader()->setDefaultSectionSize(36);
	tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
	tableWidget->setAlternatingRowColors(true);
	tableWidget->setShowGrid(false);
	tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers); // Name editing via dialog now

	// Enable drag-and-drop reordering
	tableWidget->setDragEnabled(true);
	tableWidget->setAcceptDrops(true);
	tableWidget->setDragDropMode(QAbstractItemView::InternalMove);
	tableWidget->setDefaultDropAction(Qt::MoveAction);

	// Context menu
	tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(tableWidget, &QTableWidget::customContextMenuRequested, this, &AsioSettingsDialog::showContextMenu);

	mainLayout->addWidget(tableWidget);

	// Buttons
	auto *btnLayout = new QHBoxLayout();

	btnAdd = new QPushButton(obs_module_text("AsioSettings.AddSource"), this);
	btnRemove = new QPushButton(obs_module_text("AsioSettings.RemoveSource"), this);

	connect(btnAdd, &QPushButton::clicked, this, &AsioSettingsDialog::addSource);
	connect(btnRemove, &QPushButton::clicked, this, &AsioSettingsDialog::removeSelectedSource);
	connect(tableWidget, &QTableWidget::itemSelectionChanged, this, &AsioSettingsDialog::updateRemoveButtonState);

	// Handle drag-drop reorder - save config when rows are moved
	connect(tableWidget->model(), &QAbstractItemModel::rowsMoved, this, [this]() {
		saveToConfig(true);
	});

	btnLayout->addWidget(btnAdd);
	btnLayout->addStretch();
	btnLayout->addWidget(btnRemove);

	mainLayout->addLayout(btnLayout);

	updateRemoveButtonState();
	updateAddButtonState();
}

void AsioSettingsDialog::addRowWidgets(int row, const AsioSourceConfig &src)
{
	// Column 0: Active indicator (read-only, based on obs_source_active)
	auto *activeWidget = new QWidget();
	auto *activeLayout = new QHBoxLayout(activeWidget);
	activeLayout->setContentsMargins(0, 0, 0, 0);
	activeLayout->setAlignment(Qt::AlignCenter);
	auto *activeLabel = new QLabel();
	activeLabel->setObjectName("activeIndicator");
	// Check if source exists and is active
	obs_source_t *source = obs_get_source_by_uuid(src.sourceUuid.toUtf8().constData());
	if (source) {
		bool active = obs_source_active(source);
		activeLabel->setText(active ? "●" : "○");
		activeLabel->setToolTip(active ? "Active" : "Inactive");
		obs_source_release(source);
	} else {
		activeLabel->setText("-");
		activeLabel->setToolTip("Source not created yet");
	}
	activeLayout->addWidget(activeLabel);
	tableWidget->setCellWidget(row, 0, activeWidget);
	
	// Column 1: Source name (read-only, edited via dialog)
	auto *nameItem = new QTableWidgetItem(src.name);
	// Store settings and filters in UserRole slots
	nameItem->setData(kTIDS_SourceSettings, src.sourceSettings);
	nameItem->setData(kTIDS_SourceFilters, src.sourceFilters);
	nameItem->setData(kTIDS_AudioMuted, src.muted);
	nameItem->setData(kTIDS_AudioMonitoringType, src.monitoringType);
	nameItem->setData(kTIDS_AudioVolume, src.volume);
	nameItem->setData(kTIDS_AudioBalance, src.balance);
	nameItem->setData(kTIDS_AudioForceMono, src.forceMono);
	// Store channel in a new slot for easy retrieval
	nameItem->setData(kTIDS_OutputChannel, src.outputChannel);
	// Store canvas UUID
	nameItem->setData(kTIDS_Canvas, src.canvas);
	// Store source type
	nameItem->setData(kTIDS_SourceType, src.sourceType);
	// Store source UUID (populated when source is created)
	nameItem->setData(kTIDS_SourceUuid, src.sourceUuid);
	// Store audio mixers (tracks bitmask)
	nameItem->setData(kTIDS_AudioMixers, src.audioMixers);
	tableWidget->blockSignals(true);
	tableWidget->setItem(row, 1, nameItem);
	tableWidget->blockSignals(false);

	// Column 2: Output channel (read-only label)
	auto *channelWidget = new QWidget();
	auto *channelLayout = new QHBoxLayout(channelWidget);
	channelLayout->setContentsMargins(4, 0, 4, 0);
	channelLayout->setAlignment(Qt::AlignCenter);
	QString channelText = (src.outputChannel == -1) ? "" : QString::number(src.outputChannel);
	auto *channelLabel = new QLabel(channelText);
	channelLayout->addWidget(channelLabel);
	tableWidget->setCellWidget(row, 2, channelWidget);

	// Column 3: Speaker layout (read-only, from obs_source_get_speaker_layout)
	auto *spkWidget = new QWidget();
	auto *spkLayout = new QHBoxLayout(spkWidget);
	spkLayout->setContentsMargins(4, 0, 4, 0);
	spkLayout->setAlignment(Qt::AlignCenter);
	obs_source_t *spkSource = obs_get_source_by_uuid(src.sourceUuid.toUtf8().constData());
	QString spkText = "-";
	if (spkSource) {
		enum speaker_layout layout = obs_source_get_speaker_layout(spkSource);
		spkText = speakerLayoutToString(layout);
		obs_source_release(spkSource);
	}
	auto *spkLabel = new QLabel(spkText);
	spkLabel->setObjectName("speakerLabel");
	spkLayout->addWidget(spkLabel);
	tableWidget->setCellWidget(row, 3, spkWidget);

	// Column 4: Canvas label - look up canvas name by UUID
	auto *canvasWidget = new QWidget();
	auto *canvasLayout = new QHBoxLayout(canvasWidget);
	canvasLayout->setContentsMargins(4, 0, 4, 0);
	canvasLayout->setAlignment(Qt::AlignCenter);
	
	// Find canvas name from UUID
	QString canvasName = obs_module_text("AsioSettings.MainCanvas");
	if (!src.canvas.isEmpty()) {
		obs_canvas_t *mainCanvas = obs_get_main_canvas();
		// Look up canvas by UUID to get its name
		obs_canvas_t *canvas = obs_get_canvas_by_uuid(src.canvas.toUtf8().constData());
		if (canvas) {
			if (canvas == mainCanvas) {
				canvasName = obs_module_text("AsioSettings.MainCanvas");
			} else {
				const char *name = obs_canvas_get_name(canvas);
				canvasName = name ? QString::fromUtf8(name) : src.canvas.left(8);
			}
		} else {
			canvasName = QString("? %1").arg(src.canvas.left(6)); // Unknown canvas
		}
	}
	
	auto *canvasLabel = new QLabel(canvasName);
	canvasLabel->setToolTip(src.canvas.isEmpty() ? "Main Canvas" : src.canvas);
	canvasLayout->addWidget(canvasLabel);
	tableWidget->setCellWidget(row, 4, canvasWidget);

	// Column 5: Volume slider (0-100, representing 0.0-1.0)
	auto *volWidget = new QWidget();
	auto *volLayout = new QHBoxLayout(volWidget);
	volLayout->setContentsMargins(2, 0, 2, 0);
	auto *volSlider = new QSlider(Qt::Horizontal);
	volSlider->setRange(0, 100);
	volSlider->setValue((int)(src.volume * 100));
	volSlider->setToolTip("Double-click to reset to 100%");
	volLayout->addWidget(volSlider);
	tableWidget->setCellWidget(row, 5, volWidget);

	connect(volSlider, &QSlider::valueChanged, this, [this]() { saveToConfig(true); });
	
	// Double-click to reset volume to 100
	volSlider->installEventFilter(this);
	volSlider->setProperty("resetValue", 100);

	// Column 6: Balance slider (0-100, representing 0.0-1.0, 50=center)
	auto *balWidget = new QWidget();
	auto *balLayout = new QHBoxLayout(balWidget);
	balLayout->setContentsMargins(2, 0, 2, 0);
	auto *balSlider = new QSlider(Qt::Horizontal);
	balSlider->setRange(0, 100);
	balSlider->setValue((int)(src.balance * 100));
	balSlider->setToolTip("Double-click to reset to center");
	balLayout->addWidget(balSlider);
	tableWidget->setCellWidget(row, 6, balWidget);

	connect(balSlider, &QSlider::valueChanged, this, [this]() { saveToConfig(true); });
	
	// Double-click to reset balance to 50 (center)
	balSlider->installEventFilter(this);
	balSlider->setProperty("resetValue", 50);

	// Column 7: Mute checkbox with custom icons
	auto *muteWidget = new QWidget();
	auto *muteLayout = new QHBoxLayout(muteWidget);
	muteLayout->setContentsMargins(0, 0, 0, 0);
	muteLayout->setAlignment(Qt::AlignCenter);
	auto *muteCheck = new QCheckBox();
	muteCheck->setChecked(src.muted);
	muteCheck->setCursor(Qt::PointingHandCursor);
	muteCheck->setStyleSheet(
		"QCheckBox::indicator { width: 20px; height: 20px; }"
		"QCheckBox::indicator:unchecked { fill: #ffffff; image: url(:/super/assets/icons/volume-2.svg); }"
		"QCheckBox::indicator:checked { fill: #ff0000; image: url(:/super/assets/icons/volume-x.svg); }"
	);
	muteCheck->setToolTip(src.muted ? "Unmute" : "Mute");
	
	muteLayout->addWidget(muteCheck);
	tableWidget->setCellWidget(row, 7, muteWidget);

	connect(muteCheck, &QCheckBox::checkStateChanged, this, [this, muteCheck](int state) {
		muteCheck->setToolTip(state == Qt::Checked ? "Unmute" : "Mute");
		saveToConfig(true);
	});

	// Column 8: Mono checkbox
	auto *monoWidget = new QWidget();
	auto *monoLayout = new QHBoxLayout(monoWidget);
	monoLayout->setContentsMargins(0, 0, 0, 0);
	monoLayout->setAlignment(Qt::AlignCenter);
	auto *monoCheck = new QCheckBox();
	monoCheck->setChecked(src.forceMono);
	monoLayout->addWidget(monoCheck);
	tableWidget->setCellWidget(row, 8, monoWidget);

	connect(monoCheck, &QCheckBox::checkStateChanged, this, [this] { saveToConfig(true); });

	// Column 9: Monitoring dropdown
	auto *monWidget = new QWidget();
	auto *monLayout = new QHBoxLayout(monWidget);
	monLayout->setContentsMargins(2, 0, 2, 0);
	auto *monCombo = new QComboBox();
	monCombo->addItem("Off", 0);
	monCombo->addItem("Mon", 1);
	monCombo->addItem("Both", 2);
	monCombo->setCurrentIndex(src.monitoringType);
	monLayout->addWidget(monCombo);
	tableWidget->setCellWidget(row, 9, monWidget);

	connect(monCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { saveToConfig(true); });

	// Column 10: Show in Mixer checkbox (audio active)
	auto *mixerWidget = new QWidget();
	auto *mixerLayout = new QHBoxLayout(mixerWidget);
	mixerLayout->setContentsMargins(0, 0, 0, 0);
	mixerLayout->setAlignment(Qt::AlignCenter);
	auto *mixerCheck = new QCheckBox();
	mixerCheck->setChecked(src.audioActive);
	mixerCheck->setToolTip("Show in audio mixer");
	mixerLayout->addWidget(mixerCheck);
	tableWidget->setCellWidget(row, 10, mixerWidget);

	connect(mixerCheck, &QCheckBox::checkStateChanged, this, [this, mixerCheck]() {
		// Get source name from the row containing this checkbox
		for (int r = 0; r < tableWidget->rowCount(); ++r) {
			QWidget *w = tableWidget->cellWidget(r, 10);
			if (w && w->findChild<QCheckBox *>() == mixerCheck) {
				if (auto *item = tableWidget->item(r, 1)) {
					QString sourceUuid = item->data(kTIDS_SourceUuid).toString();
					obs_source_t *source = obs_get_source_by_uuid(sourceUuid.toUtf8().constData());
					if (source) {
						obs_source_set_audio_active(source, mixerCheck->isChecked());
						obs_source_release(source);
					}
				}
				break;
			}
		}
		saveToConfig(false);
	});

	// Column 11: Configure button
	auto *cfgWidget = new QWidget();
	auto *cfgLayout = new QHBoxLayout(cfgWidget);
	cfgLayout->setContentsMargins(2, 0, 2, 0);
	cfgLayout->setAlignment(Qt::AlignCenter);
	auto *cfgBtn = new QPushButton();
	cfgBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	cfgBtn->setIcon(QIcon(":/super/assets/icons/settings.svg"));
	cfgBtn->setProperty("toolButton", true);
	cfgBtn->setFlat(true);
	cfgBtn->setToolTip(obs_module_text("AsioSettings.Configure"));

	connect(cfgBtn, &QPushButton::clicked, this, [this, cfgBtn]() {
		for (int r = 0; r < tableWidget->rowCount(); ++r) {
			QWidget *w = tableWidget->cellWidget(r, 11);
			if (w && w->findChild<QPushButton *>() == cfgBtn) {
				openSourceProperties(r);
				break;
			}
		}
	});

	cfgLayout->addWidget(cfgBtn);
	tableWidget->setCellWidget(row, 11, cfgWidget);

	// Column 12: Filters button
	auto *filterWidget = new QWidget();
	auto *filterLayout = new QHBoxLayout(filterWidget);
	filterLayout->setContentsMargins(2, 0, 2, 0);
	filterLayout->setAlignment(Qt::AlignCenter);
	auto *filterBtn = new QPushButton();
	filterBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	filterBtn->setIcon(QIcon(":/super/assets/icons/sliders.svg"));
	filterBtn->setProperty("toolButton", true);
	filterBtn->setFlat(true);
	filterBtn->setToolTip(obs_module_text("AsioSettings.Filters"));

	connect(filterBtn, &QPushButton::clicked, this, [this, filterBtn]() {
		for (int r = 0; r < tableWidget->rowCount(); ++r) {
			QWidget *w = tableWidget->cellWidget(r, 12);
			if (w && w->findChild<QPushButton *>() == filterBtn) {
				openSourceFilters(r);
				break;
			}
		}
	});

	filterLayout->addWidget(filterBtn);
	tableWidget->setCellWidget(row, 12, filterWidget);

	// Column 13: Edit button
	auto *editWidget = new QWidget();
	auto *editLayout = new QHBoxLayout(editWidget);
	editLayout->setContentsMargins(2, 0, 2, 0);
	editLayout->setAlignment(Qt::AlignCenter);
	auto *editBtn = new QPushButton();
	editBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	editBtn->setIcon(QIcon(":/super/assets/icons/edit.svg"));
	editBtn->setProperty("toolButton", true);
	editBtn->setFlat(true);
	editBtn->setToolTip(obs_module_text("AsioSettings.EditSource"));

	connect(editBtn, &QPushButton::clicked, this, [this, editBtn]() {
		for (int r = 0; r < tableWidget->rowCount(); ++r) {
			QWidget *w = tableWidget->cellWidget(r, 13);
			if (w && w->findChild<QPushButton *>() == editBtn) {
				editSource(r);
				break;
			}
		}
	});

	editLayout->addWidget(editBtn);
	tableWidget->setCellWidget(row, 13, editWidget);

	// Column 14: Delete button
	auto *delWidget = new QWidget();
	auto *delLayout = new QHBoxLayout(delWidget);
	delLayout->setContentsMargins(2, 0, 2, 0);
	delLayout->setAlignment(Qt::AlignCenter);
	auto *delBtn = new QPushButton();
	delBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	delBtn->setIcon(QIcon(":/super/assets/icons/trash-2.svg"));
	delBtn->setProperty("toolButton", true);
	delBtn->setFlat(true);
	delBtn->setToolTip(obs_module_text("AsioSettings.RemoveSource"));

	connect(delBtn, &QPushButton::clicked, this, [this, delBtn]() {
		for (int r = 0; r < tableWidget->rowCount(); ++r) {
			QWidget *w = tableWidget->cellWidget(r, 14);
			if (w && w->findChild<QPushButton *>() == delBtn) {
				deleteSource(r);
				break;
			}
		}
	});

	delLayout->addWidget(delBtn);
	tableWidget->setCellWidget(row, 14, delWidget);

	// Update tooltip for this row
	updateRowTooltip(row);
}

void AsioSettingsDialog::loadFromConfig()
{
	// Block signals during population to prevent itemChanged from triggering saves
	tableWidget->blockSignals(true);
	
	tableWidget->setRowCount(0);

	const auto &sources = AsioConfig::get()->getSources();
	for (const auto &src : sources) {
		int row = tableWidget->rowCount();
		tableWidget->insertRow(row);
		addRowWidgets(row, src);
	}
	
	tableWidget->blockSignals(false);

	updateRemoveButtonState();
	updateAddButtonState();
}

void AsioSettingsDialog::saveToConfig(bool doRefresh)
{
	auto &sources = AsioConfig::get()->getSources();
	sources.clear();

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		AsioSourceConfig cfg;

		// Name and stored settings
		if (auto *item = tableWidget->item(i, 1)) {
			cfg.name = item->text();
			// Retrieve preserved settings and filters
			cfg.sourceSettings = item->data(kTIDS_SourceSettings).toJsonObject();
			cfg.sourceFilters = item->data(kTIDS_SourceFilters).toJsonArray();
			// Read channel from item data
			cfg.outputChannel = item->data(kTIDS_OutputChannel).toInt();
			// Read canvas UUID from item data
			cfg.canvas = item->data(kTIDS_Canvas).toString();
			// Read source type from item data
			cfg.sourceType = item->data(kTIDS_SourceType).toString();
			// Read source UUID for stable matching
			cfg.sourceUuid = item->data(kTIDS_SourceUuid).toString();
			// Read audio mixers
			cfg.audioMixers = item->data(kTIDS_AudioMixers).toUInt();
		}

		// Volume (column 5)
		if (auto *w = tableWidget->cellWidget(i, 5)) {
			if (auto *slider = w->findChild<QSlider *>()) {
				cfg.volume = slider->value() / 100.0f;
			}
		}

		// Balance (column 6)
		if (auto *w = tableWidget->cellWidget(i, 6)) {
			if (auto *slider = w->findChild<QSlider *>()) {
				cfg.balance = slider->value() / 100.0f;
			}
		}

		// Mute (column 7)
		if (auto *w = tableWidget->cellWidget(i, 7)) {
			if (auto *check = w->findChild<QCheckBox *>()) {
				cfg.muted = check->isChecked();
			}
		}

		// Mono (column 8)
		if (auto *w = tableWidget->cellWidget(i, 8)) {
			if (auto *check = w->findChild<QCheckBox *>()) {
				cfg.forceMono = check->isChecked();
			}
		}

		// Monitoring (column 9)
		if (auto *w = tableWidget->cellWidget(i, 9)) {
			if (auto *combo = w->findChild<QComboBox *>()) {
				cfg.monitoringType = combo->currentIndex();
			}
		}

		// Audio Active / Show in Mixer (column 10)
		if (auto *w = tableWidget->cellWidget(i, 10)) {
			if (auto *check = w->findChild<QCheckBox *>()) {
				cfg.audioActive = check->isChecked();
			}
		}

		cfg.enabled = true;
		sources.append(cfg);
	}

	AsioConfig::get()->save();
	updateAddButtonState();

	// Refresh running sources to match new settings (only when requested)
	if (doRefresh) {
		refreshAsioSources();
	}
}

int AsioSettingsDialog::findNextAvailableChannel(const QString &canvasUuid) const
{
	// Get canvas from UUID (OBS is source of truth)
	obs_canvas_t *canvas = canvasUuid.isEmpty() ? obs_get_main_canvas()
		: obs_get_canvas_by_uuid(canvasUuid.toUtf8().constData());
	if (!canvas) canvas = obs_get_main_canvas();
	
	for (int ch = 1; ch <= MAX_CHANNELS; ch++) {
		obs_source_t *existing = obs_canvas_get_channel(canvas, ch - 1); // 0-indexed
		if (!existing) {
			return ch;
		}
		obs_source_release(existing);
	}

	// All channels used - cycle back to 1
	return 1;
}

void AsioSettingsDialog::addSource()
{
	if (tableWidget->rowCount() >= MAX_CHANNELS) {
		QMessageBox::warning(this,
			obs_module_text("AsioSettings.Title"),
			obs_module_text("AsioSettings.MaxSourcesReached"));
		return;
	}

	// Create and configure dialog
	AsioSourceDialog dlg(AsioSourceDialog::AddMode, this);
	
	// Pre-fill with default unique name
	AsioSourceConfig defaultCfg;
	defaultCfg.name = generateUniqueName("Audio");
	defaultCfg.outputChannel = findNextAvailableChannel();
	dlg.setConfig(defaultCfg);
	
	if (dlg.exec() != QDialog::Accepted) {
		return;
	}

	// Capture whether to open properties before dialog is destroyed
	bool openProps = dlg.shouldOpenProperties();
	
	int row = tableWidget->rowCount();
	tableWidget->insertRow(row);

	AsioSourceConfig newCfg;
	newCfg.name = dlg.getName();
	newCfg.sourceType = dlg.getSourceType();
	newCfg.canvas = dlg.getCanvas();
	newCfg.outputChannel = dlg.getChannel();
	newCfg.enabled = true;
	newCfg.muted = dlg.shouldStartMuted();
	newCfg.audioMixers = dlg.getAudioMixers();

	addRowWidgets(row, newCfg);

	saveToConfig();
	updateRemoveButtonState();
	updateAddButtonState();
	
	// Update active indicator after source may be created
	updateActiveIndicator(row);
	updateSpeakerLayout(row);
	
	// Open properties dialog if checkbox was checked
	if (openProps) {
		openSourceProperties(row);
	}
}

void AsioSettingsDialog::editSource(int row)
{
	if (row < 0 || row >= tableWidget->rowCount()) return;
	
	auto *item = tableWidget->item(row, 1);  // Name is in column 1
	if (!item) return;
	
	QString currentName = item->text();
	int currentChannel = item->data(kTIDS_OutputChannel).toInt();
	
	// Build config from current row data
	AsioSourceConfig cfg;
	cfg.name = currentName;
	cfg.sourceType = item->data(kTIDS_SourceType).toString();
	cfg.canvas = item->data(kTIDS_Canvas).toString();
	cfg.outputChannel = currentChannel;
	cfg.sourceSettings = item->data(kTIDS_SourceSettings).toJsonObject();
	cfg.sourceFilters = item->data(kTIDS_SourceFilters).toJsonArray();
	cfg.audioMixers = item->data(kTIDS_AudioMixers).toUInt();
	
	// Create dialog in edit mode
	AsioSourceDialog dlg(AsioSourceDialog::EditMode, this);
	dlg.setConfig(cfg);
	
	if (dlg.exec() != QDialog::Accepted) {
		return;
	}
	
	// Update name
	item->setText(dlg.getName());
	
	// Update source type
	item->setData(kTIDS_SourceType, dlg.getSourceType());
	
	// Update channel in item data and label
	int newChannel = dlg.getChannel();
	item->setData(kTIDS_OutputChannel, newChannel);
	if (auto *w = tableWidget->cellWidget(row, 2)) {  // Channel is in column 2
		if (auto *lbl = w->findChild<QLabel *>()) {
			QString channelText = (newChannel == -1) ? "" : QString::number(newChannel);
			lbl->setText(channelText);
		}
	}
	
	// Update canvas in item data and label
	QString newCanvas = dlg.getCanvas();
	item->setData(kTIDS_Canvas, newCanvas);
	if (auto *w = tableWidget->cellWidget(row, 4)) {  // Canvas is in column 4
		if (auto *lbl = w->findChild<QLabel *>()) {
			// Look up canvas name from UUID
			QString canvasName = obs_module_text("AsioSettings.MainCanvas");
			if (!newCanvas.isEmpty()) {
				obs_canvas_t *mainCanvas = obs_get_main_canvas();
				obs_canvas_t *canvas = obs_get_canvas_by_uuid(newCanvas.toUtf8().constData());
				if (canvas) {
					if (canvas == mainCanvas) {
						canvasName = obs_module_text("AsioSettings.MainCanvas");
					} else {
						const char *name = obs_canvas_get_name(canvas);
						canvasName = name ? QString::fromUtf8(name) : newCanvas.left(8);
					}
				} else {
					canvasName = QString("? %1").arg(newCanvas.left(6));
				}
			}
			lbl->setText(canvasName);
			lbl->setToolTip(newCanvas.isEmpty() ? "Main Canvas" : newCanvas);
		}
	}
	
	// Update audio mixers
	item->setData(kTIDS_AudioMixers, dlg.getAudioMixers());
	
	saveToConfig();
	updateRowTooltip(row);
	updateActiveIndicator(row);
	updateSpeakerLayout(row);
}

void AsioSettingsDialog::duplicateSource(int row)
{
	if (row < 0 || row >= tableWidget->rowCount()) return;
	if (tableWidget->rowCount() >= MAX_CHANNELS) {
		QMessageBox::warning(this,
			obs_module_text("AsioSettings.Title"),
			obs_module_text("AsioSettings.MaxSourcesReached"));
		return;
	}
	
	auto *item = tableWidget->item(row, 1);  // Name is in column 1
	if (!item) return;
	
	QString baseName = item->text();
	QString suggestedName = generateUniqueName(baseName + " Copy");
	
	// Copy source type and canvas from original
	QString originalSourceType = item->data(kTIDS_SourceType).toString();
	QString originalCanvas = item->data(kTIDS_Canvas).toString();
	
	// Create dialog pre-filled with original's properties
	AsioSourceDialog dlg(AsioSourceDialog::DuplicateMode, this);
	
	AsioSourceConfig cfg;
	cfg.name = suggestedName;
	cfg.sourceType = originalSourceType;
	cfg.canvas = originalCanvas;
	cfg.outputChannel = findNextAvailableChannel(originalCanvas);
	dlg.setConfig(cfg);
	dlg.setOpenProperties(false); // Don't open properties on duplicate
	
	if (dlg.exec() != QDialog::Accepted) {
		return;
	}
	
	// Capture whether to open properties
	bool openProps = dlg.shouldOpenProperties();
	
	int newRow = tableWidget->rowCount();
	tableWidget->insertRow(newRow);
	
	AsioSourceConfig newCfg;
	newCfg.name = dlg.getName();
	newCfg.sourceType = originalSourceType; // Keep original source type
	newCfg.canvas = dlg.getCanvas();
	newCfg.outputChannel = dlg.getChannel();
	newCfg.enabled = true;
	newCfg.muted = dlg.shouldStartMuted();
	
	// Copy source settings and filters from original
	newCfg.sourceSettings = item->data(kTIDS_SourceSettings).toJsonObject();
	newCfg.sourceFilters = item->data(kTIDS_SourceFilters).toJsonArray();
	
	// Copy audio control settings from original
	newCfg.volume = item->data(kTIDS_AudioVolume).toFloat();
	newCfg.balance = item->data(kTIDS_AudioBalance).toFloat();
	newCfg.monitoringType = item->data(kTIDS_AudioMonitoringType).toInt();
	newCfg.forceMono = item->data(kTIDS_AudioForceMono).toBool();
	newCfg.audioMixers = item->data(kTIDS_AudioMixers).toUInt();
	
	addRowWidgets(newRow, newCfg);
	
	saveToConfig();
	updateRemoveButtonState();
	updateAddButtonState();
	
	// Update active indicator after source may be created
	updateActiveIndicator(newRow);
	updateSpeakerLayout(newRow);
	
	// Open properties dialog if checkbox was checked
	if (openProps) {
		openSourceProperties(newRow);
	}
}

void AsioSettingsDialog::deleteSource(int row)
{
	if (row < 0 || row >= tableWidget->rowCount()) return;
	
	auto *item = tableWidget->item(row, 1);  // Name is in column 1
	QString name = item ? item->text() : QString("Source");
	
	// Confirmation dialog
	auto result = QMessageBox::question(this,
		obs_module_text("AsioSettings.ConfirmDelete"),
		QString(obs_module_text("AsioSettings.ConfirmDeleteMsg")).arg(name),
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No);
	
	if (result != QMessageBox::Yes) {
		return;
	}
	
	tableWidget->removeRow(row);
	saveToConfig(true);
	updateRemoveButtonState();
	updateAddButtonState();
}

void AsioSettingsDialog::removeSelectedSource()
{
	auto selected = tableWidget->selectedItems();
	if (!selected.isEmpty()) {
		deleteSource(selected.first()->row());
	}
}

void AsioSettingsDialog::showContextMenu(const QPoint &pos)
{
	QModelIndex index = tableWidget->indexAt(pos);
	if (!index.isValid()) return;
	
	int row = index.row();
	
	QMenu menu(this);
	
	QAction *editAction = menu.addAction(obs_module_text("AsioSettings.EditSource"));
	QAction *duplicateAction = menu.addAction(obs_module_text("AsioSettings.Duplicate"));
	menu.addSeparator();
	QAction *deleteAction = menu.addAction(obs_module_text("AsioSettings.RemoveSource"));
	deleteAction->setIcon(QIcon()); // No icon, but red text would be nice
	
	QAction *selected = menu.exec(tableWidget->viewport()->mapToGlobal(pos));
	
	if (selected == editAction) {
		editSource(row);
	} else if (selected == duplicateAction) {
		duplicateSource(row);
	} else if (selected == deleteAction) {
		deleteSource(row);
	}
}

void AsioSettingsDialog::keyPressEvent(QKeyEvent *event)
{
	if (!tableWidget->hasFocus()) {
		QDialog::keyPressEvent(event);
		return;
	}
	
	auto selected = tableWidget->selectedItems();
	if (selected.isEmpty()) {
		QDialog::keyPressEvent(event);
		return;
	}
	
	int row = selected.first()->row();
	
	if (event->key() == Qt::Key_Delete) {
		deleteSource(row);
	} else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
		editSource(row);
	} else if (event->key() == Qt::Key_D && (event->modifiers() & Qt::ControlModifier)) {
		duplicateSource(row);
	} else {
		QDialog::keyPressEvent(event);
	}
}

void AsioSettingsDialog::updateRowTooltip(int row)
{
	auto *item = tableWidget->item(row, 1);  // Name is in column 1
	if (!item) return;
	
	int channel = item->data(kTIDS_OutputChannel).toInt();
	QJsonArray filters = item->data(kTIDS_SourceFilters).toJsonArray();
	QJsonObject settings = item->data(kTIDS_SourceSettings).toJsonObject();
	
	QString deviceName = settings.value("device_id").toString();
	if (deviceName.isEmpty()) {
		deviceName = "Not configured";
	}
	
	QString tooltip = QString("Channel: %1 | Device: %2 | Filters: %3")
		.arg(channel)
		.arg(deviceName)
		.arg(filters.size());
	
	item->setToolTip(tooltip);
}

void AsioSettingsDialog::updateActiveIndicator(int row)
{
	if (row < 0 || row >= tableWidget->rowCount()) return;
	
	// Get name from item in column 1
	auto *item = tableWidget->item(row, 1);  // Name is in column 1
	if (!item) return;
	
	QString sourceUuid = item->data(kTIDS_SourceUuid).toString();
	
	// Find the active indicator label in column 0
	if (auto *w = tableWidget->cellWidget(row, 0)) {
		if (auto *lbl = w->findChild<QLabel *>()) {
			obs_source_t *source = obs_get_source_by_uuid(sourceUuid.toUtf8().constData());
			if (source) {
				bool active = obs_source_active(source);
				lbl->setText(active ? QString::fromUtf8("\u25cf") : QString::fromUtf8("\u25cb"));
				lbl->setToolTip(active ? "Active" : "Inactive");
				obs_source_release(source);
			} else {
				lbl->setText("-");
				lbl->setToolTip("Source not created yet");
			}
		}
	}
}

void AsioSettingsDialog::updateSpeakerLayout(int row)
{
	if (row < 0 || row >= tableWidget->rowCount()) return;
	
	// Get name from item in column 1
	auto *item = tableWidget->item(row, 1);  // Name is in column 1
	if (!item) return;
	
	QString sourceUuid = item->data(kTIDS_SourceUuid).toString();
	
	// Find the speaker layout label in column 3
	if (auto *w = tableWidget->cellWidget(row, 3)) {
		if (auto *lbl = w->findChild<QLabel *>()) {
			obs_source_t *source = obs_get_source_by_uuid(sourceUuid.toUtf8().constData());
			if (source) {
				enum speaker_layout layout = obs_source_get_speaker_layout(source);
				lbl->setText(speakerLayoutToString(layout));
				obs_source_release(source);
			} else {
				lbl->setText("-");
			}
		}
	}
}

void AsioSettingsDialog::updateSpeakerLayoutByUuid(const QString &sourceUuid)
{
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, sourceUuid]() {
			updateSpeakerLayoutByUuid(sourceUuid);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 1)) {  // Name is in column 1
			if (item->data(kTIDS_SourceUuid).toString() == sourceUuid) {
				updateSpeakerLayout(i);
				break;
			}
		}
	}
}

void AsioSettingsDialog::updateSourceUuid(int configIndex, const QString &uuid)
{
	if (configIndex < 0 || configIndex >= tableWidget->rowCount()) {
		return;
	}
	
	auto *item = tableWidget->item(configIndex, 1);  // Name is in column 1
	if (item) {
		item->setData(kTIDS_SourceUuid, uuid);
	}
}

QString AsioSettingsDialog::generateUniqueName(const QString &baseName) const
{
	const auto &sources = AsioConfig::get()->getSources();
	QString name = baseName;
	int counter = 2;
	
	bool exists = true;
	while (exists) {
		exists = false;
		for (const auto &src : sources) {
			if (src.name == name) {
				exists = true;
				name = QString("%1 %2").arg(baseName).arg(counter++);
				break;
			}
		}
	}
	
	return name;
}

void AsioSettingsDialog::openSourceProperties(int row)
{
	if (row < 0 || row >= tableWidget->rowCount()) {
		return;
	}

	// Get source name from table
	auto *item = tableWidget->item(row, 1);  // Name is in column 1
	if (!item) return;

	QString sourceUuid = item->data(kTIDS_SourceUuid).toString();
	obs_source_t *source = obs_get_source_by_uuid(sourceUuid.toUtf8().constData());
	if (!source) {
		QMessageBox::warning(this, obs_module_text("Error"),
			obs_module_text("Error.CreateAudioSource"));
		return;
	}

	// Open the properties dialog for the source
	obs_frontend_open_source_properties(source);

	// After dialog closes, save the updated settings to config
	auto &sources = AsioConfig::get()->getSources();
	if (row < sources.size()) {
		AsioSourceConfig &cfg = sources[row];
		obs_data_t *newSettings = obs_source_get_settings(source);
		if (newSettings) {
			const char *json = obs_data_get_json(newSettings);
			if (json) {
				QJsonParseError error;
				QJsonDocument doc = QJsonDocument::fromJson(QByteArray(json), &error);
				if (error.error == QJsonParseError::NoError && doc.isObject()) {
					cfg.sourceSettings = doc.object();
					
					// Update the table item data too so it stays in sync if we save again
					if (auto *item = tableWidget->item(row, 1)) {  // Name is in column 1
						item->setData(kTIDS_SourceSettings, cfg.sourceSettings);
					}

					AsioConfig::get()->save();
					auto sourceName = item->text();
					obs_log(LOG_INFO, "Saved source settings for '%s'", sourceName.toUtf8().constData());
				}
			}
			obs_data_release(newSettings);
		}
	}
	obs_source_release(source);
}

void AsioSettingsDialog::openSourceFilters(int row)
{
	if (row < 0 || row >= tableWidget->rowCount()) {
		return;
	}

	// Get source name from table
	auto *item = tableWidget->item(row, 1);  // Name is in column 1
	if (!item) return;
	
	QString sourceName = item->text();
	obs_source_t *source = obs_get_source_by_name(sourceName.toUtf8().constData());
	if (!source) {
		QMessageBox::warning(this, obs_module_text("Error"),
			obs_module_text("Error.CreateAudioSource"));
		return;
	}

	// Open the filters dialog for the source
	obs_frontend_open_source_filters(source);
	obs_source_release(source);
}

void AsioSettingsDialog::updateRemoveButtonState()
{
	btnRemove->setEnabled(tableWidget->rowCount() > 0 && !tableWidget->selectedItems().isEmpty());
}

void AsioSettingsDialog::updateAddButtonState()
{
	// Allow adding sources even when all channels are used (they'll cycle back)
	bool canAdd = tableWidget->rowCount() < MAX_CHANNELS;
	btnAdd->setEnabled(canAdd);

	if (!canAdd) {
		btnAdd->setToolTip(obs_module_text("AsioSettings.MaxSourcesReached"));
	} else {
		btnAdd->setToolTip("");
	}
}

void AsioSettingsDialog::toggle_show_hide()
{
	if (!isVisible()) {
		show();
		raise();
		activateWindow();
	} else {
		hide();
	}
}

void AsioSettingsDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	loadFromConfig();
}

void AsioSettingsDialog::closeEvent(QCloseEvent *event)
{
	// Config is already saved on every change, no need to save again
	QDialog::closeEvent(event);
}

void AsioSettingsDialog::hideEvent(QHideEvent *event)
{
	// Config is already saved on every change, no need to save again
	QDialog::hideEvent(event);
}

bool AsioSettingsDialog::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::MouseButtonDblClick) {
		if (auto *slider = qobject_cast<QSlider *>(obj)) {
			QVariant resetVal = slider->property("resetValue");
			if (resetVal.isValid()) {
				slider->setValue(resetVal.toInt());
				return true; // Event handled
			}
		}
	}
	return QDialog::eventFilter(obj, event);
}

void AsioSettingsDialog::updateSourceName(const QString &sourceUuid, const QString &name)
{
	// Called from OBS signal thread? ensure UI thread
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, sourceUuid, name]() {
			updateSourceName(sourceUuid, name);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 1)) {  // Name is in column 1
			if (item->data(kTIDS_SourceUuid).toString() == sourceUuid) {
				// Block signals to prevent itemChanged from triggering save
				tableWidget->blockSignals(true);
				item->setText(name);
				tableWidget->blockSignals(false);
				break;
			}
		}
	}
}

void AsioSettingsDialog::updateSourceNameByIndex(int configIndex, const QString &name)
{
	// Called from source creation - update by row index
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, configIndex, name]() {
			updateSourceNameByIndex(configIndex, name);
		});
		return;
	}

	if (configIndex >= 0 && configIndex < tableWidget->rowCount()) {
		if (auto *item = tableWidget->item(configIndex, 1)) {  // Name is in column 1
			tableWidget->blockSignals(true);
			item->setText(name);
			tableWidget->blockSignals(false);
		}
	}
}

void AsioSettingsDialog::updateSourceMuted(const QString &sourceUuid, bool muted)
{
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, sourceUuid, muted]() {
			updateSourceMuted(sourceUuid, muted);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 1)) {  // Name is in column 1
			if (item->data(kTIDS_SourceUuid).toString() == sourceUuid) {
				if (auto *muteW = tableWidget->cellWidget(i, 7)) {  // Mute is in column 7
					if (auto *check = muteW->findChild<QCheckBox *>()) {
						check->blockSignals(true);
						check->setChecked(muted);
						check->blockSignals(false);
					}
				}
				break;
			}
		}
	}
}

void AsioSettingsDialog::updateSourceVolume(const QString &sourceUuid, float volume)
{
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, sourceUuid, volume]() {
			updateSourceVolume(sourceUuid, volume);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 1)) {  // Name is in column 1
			if (item->data(kTIDS_SourceUuid).toString() == sourceUuid) {
				if (auto *volW = tableWidget->cellWidget(i, 5)) {  // Volume is in column 5
					if (auto *slider = volW->findChild<QSlider *>()) {
						slider->blockSignals(true);
						slider->setValue((int)(volume * 100));
						slider->blockSignals(false);
					}
				}
				break;
			}
		}
	}
}

void AsioSettingsDialog::updateSourceBalance(const QString &sourceUuid, float balance)
{
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, sourceUuid, balance]() {
			updateSourceBalance(sourceUuid, balance);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 1)) {  // Name is in column 1
			if (item->data(kTIDS_SourceUuid).toString() == sourceUuid) {
				if (auto *balW = tableWidget->cellWidget(i, 6)) {  // Balance is in column 6
					if (auto *slider = balW->findChild<QSlider *>()) {
						slider->blockSignals(true);
						slider->setValue((int)(balance * 100));
						slider->blockSignals(false);
					}
				}
				break;
			}
		}
	}
}

void AsioSettingsDialog::updateSourceMonitoring(const QString &sourceUuid, int type)
{
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, sourceUuid, type]() {
			updateSourceMonitoring(sourceUuid, type);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 1)) {  // Name is in column 1
			if (item->data(kTIDS_SourceUuid).toString() == sourceUuid) {
				if (auto *monW = tableWidget->cellWidget(i, 9)) {  // Monitoring is in column 9
					if (auto *combo = monW->findChild<QComboBox *>()) {
						combo->blockSignals(true);
						combo->setCurrentIndex(type);
						combo->blockSignals(false);
					}
				}
				break;
			}
		}
	}
}

void AsioSettingsDialog::updateSourceMono(const QString &sourceUuid, bool mono)
{
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, sourceUuid, mono]() {
			updateSourceMono(sourceUuid, mono);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 1)) {  // Name is in column 1
			if (item->data(kTIDS_SourceUuid).toString() == sourceUuid) {
				if (auto *monoW = tableWidget->cellWidget(i, 8)) {  // Mono is in column 8
					if (auto *check = monoW->findChild<QCheckBox *>()) {
						check->blockSignals(true);
						check->setChecked(mono);
						check->blockSignals(false);
					}
				}
				break;
			}
		}
	}
}

void AsioSettingsDialog::updateSourceAudioMixers(const QString &sourceUuid, uint32_t mixers)
{
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, sourceUuid, mixers]() {
			updateSourceAudioMixers(sourceUuid, mixers);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 1)) {  // Name is in column 1
			if (item->data(kTIDS_SourceUuid).toString() == sourceUuid) {
				item->setData(kTIDS_AudioMixers, mixers);
				break;
			}
		}
	}
}

void AsioSettingsDialog::updateSourceAudioActive(const QString &sourceUuid, bool active)
{
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, sourceUuid, active]() {
			updateSourceAudioActive(sourceUuid, active);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 1)) {  // Name is in column 1
			if (item->data(kTIDS_SourceUuid).toString() == sourceUuid) {
				if (auto *mixerW = tableWidget->cellWidget(i, 10)) {  // Mixer is in column 10
					if (auto *check = mixerW->findChild<QCheckBox *>()) {
						check->blockSignals(true);
						check->setChecked(active);
						check->blockSignals(false);
					}
				}
				break;
			}
		}
	}
}

void AsioSettingsDialog::updateSourceSettings(const QString &sourceUuid, const QJsonObject &settings)
{
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, sourceUuid, settings]() {
			updateSourceSettings(sourceUuid, settings);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 1)) {  // Name is in column 1
			if (item->data(kTIDS_SourceUuid).toString() == sourceUuid) {
				item->setData(kTIDS_SourceSettings, settings);
				break;
			}
		}
	}
}

void AsioSettingsDialog::updateSourceFilters(const QString &sourceUuid, const QJsonArray &filters)
{
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, sourceUuid, filters]() {
			updateSourceFilters(sourceUuid, filters);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 1)) {  // Name is in column 1
			if (item->data(kTIDS_SourceUuid).toString() == sourceUuid) {
				item->setData(kTIDS_SourceFilters, filters);
				break;
			}
		}
	}
}
