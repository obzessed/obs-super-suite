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
};

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
	infoLabel->setStyleSheet("color: #888; margin-bottom: 8px;");
	mainLayout->addWidget(infoLabel);

	// Table - 11 columns: Name, Ch, Vol, Bal, Mute, Mono, Monitor, Props, Filters, Edit, Delete
	tableWidget = new QTableWidget(this);
	tableWidget->setColumnCount(11);
	tableWidget->setHorizontalHeaderLabels({
		obs_module_text("AsioSettings.SourceName"),
		"Ch",       // Channel (read-only)
		"Vol",      // Volume
		"Bal",      // Balance
		"M",        // Mute
		"Mo",       // Mono
		"Mon",      // Monitoring
		"",         // Configure button
		"",         // Filters button
		"",         // Edit button
		""          // Delete button column
	});

	tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
	tableWidget->setColumnWidth(1, 35);   // Channel
	tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
	tableWidget->setColumnWidth(2, 60);   // Volume
	tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
	tableWidget->setColumnWidth(3, 60);   // Balance
	tableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
	tableWidget->setColumnWidth(4, 30);   // Mute
	tableWidget->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
	tableWidget->setColumnWidth(5, 30);   // Mono
	tableWidget->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
	tableWidget->setColumnWidth(6, 70);   // Monitoring
	tableWidget->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Fixed);
	tableWidget->setColumnWidth(7, 30);   // Configure
	tableWidget->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Fixed);
	tableWidget->setColumnWidth(8, 30);   // Filters
	tableWidget->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Fixed);
	tableWidget->setColumnWidth(9, 30);   // Edit
	tableWidget->horizontalHeader()->setSectionResizeMode(10, QHeaderView::Fixed);
	tableWidget->setColumnWidth(10, 30);  // Delete

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

	tableWidget->setStyleSheet(
		"QTableWidget { border: none; background-color: #2b2b2b; color: #e0e0e0; outline: none; }"
		"QHeaderView::section { background-color: #3b3b3b; padding: 6px; border: none; font-weight: bold; }"
		"QTableWidget::item { padding: 4px; outline: none; }"
		"QTableWidget::item:selected { background-color: #4a4a4a; }"
	);

	mainLayout->addWidget(tableWidget);

	// Buttons
	auto *btnLayout = new QHBoxLayout();

	btnAdd = new QPushButton(obs_module_text("AsioSettings.AddSource"), this);
	btnAdd->setStyleSheet(
		"QPushButton { background-color: #4a5568; color: #e2e8f0; border-radius: 4px; padding: 6px 14px; }"
		"QPushButton:hover { background-color: #5a6578; color: white; }"
		"QPushButton:disabled { background-color: #2a2a2a; color: #555; }"
	);

	btnRemove = new QPushButton(obs_module_text("AsioSettings.RemoveSource"), this);
	btnRemove->setStyleSheet(
		"QPushButton { background-color: #363636; color: #999; border-radius: 4px; padding: 6px 14px; }"
		"QPushButton:hover { background-color: #c62828; color: white; }"
		"QPushButton:disabled { background-color: #2a2a2a; color: #555; }"
	);

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
	// Column 0: Source name (read-only, edited via dialog)
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
	tableWidget->blockSignals(true);
	tableWidget->setItem(row, 0, nameItem);
	tableWidget->blockSignals(false);

	// Column 1: Output channel (read-only label)
	auto *channelWidget = new QWidget();
	auto *channelLayout = new QHBoxLayout(channelWidget);
	channelLayout->setContentsMargins(4, 0, 4, 0);
	channelLayout->setAlignment(Qt::AlignCenter);
	auto *channelLabel = new QLabel(QString::number(src.outputChannel));
	channelLabel->setStyleSheet("QLabel { color: #aaa; }");
	channelLayout->addWidget(channelLabel);
	tableWidget->setCellWidget(row, 1, channelWidget);

	// Column 2: Volume slider (0-100, representing 0.0-1.0)
	auto *volWidget = new QWidget();
	auto *volLayout = new QHBoxLayout(volWidget);
	volLayout->setContentsMargins(2, 0, 2, 0);
	auto *volSlider = new QSlider(Qt::Horizontal);
	volSlider->setRange(0, 100);
	volSlider->setValue((int)(src.volume * 100));
	volSlider->setStyleSheet("QSlider::groove:horizontal { background: #3b3b3b; height: 4px; } "
		"QSlider::handle:horizontal { background: #5a6578; width: 10px; margin: -3px 0; border-radius: 5px; }");
	volSlider->setToolTip("Double-click to reset to 100%");
	volLayout->addWidget(volSlider);
	tableWidget->setCellWidget(row, 2, volWidget);

	connect(volSlider, &QSlider::valueChanged, this, [this]() { saveToConfig(true); });
	
	// Double-click to reset volume to 100
	volSlider->installEventFilter(this);
	volSlider->setProperty("resetValue", 100);

	// Column 3: Balance slider (0-100, representing 0.0-1.0, 50=center)
	auto *balWidget = new QWidget();
	auto *balLayout = new QHBoxLayout(balWidget);
	balLayout->setContentsMargins(2, 0, 2, 0);
	auto *balSlider = new QSlider(Qt::Horizontal);
	balSlider->setRange(0, 100);
	balSlider->setValue((int)(src.balance * 100));
	balSlider->setStyleSheet("QSlider::groove:horizontal { background: #3b3b3b; height: 4px; } "
		"QSlider::handle:horizontal { background: #5a6578; width: 10px; margin: -3px 0; border-radius: 5px; }");
	balSlider->setToolTip("Double-click to reset to center");
	balLayout->addWidget(balSlider);
	tableWidget->setCellWidget(row, 3, balWidget);

	connect(balSlider, &QSlider::valueChanged, this, [this]() { saveToConfig(true); });
	
	// Double-click to reset balance to 50 (center)
	balSlider->installEventFilter(this);
	balSlider->setProperty("resetValue", 50);

	// Column 4: Mute checkbox with custom icons
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
	tableWidget->setCellWidget(row, 4, muteWidget);

	connect(muteCheck, &QCheckBox::checkStateChanged, this, [this, muteCheck](int state) {
		muteCheck->setToolTip(state == Qt::Checked ? "Unmute" : "Mute");
		saveToConfig(true);
	});

	// Column 5: Mono checkbox
	auto *monoWidget = new QWidget();
	auto *monoLayout = new QHBoxLayout(monoWidget);
	monoLayout->setContentsMargins(0, 0, 0, 0);
	monoLayout->setAlignment(Qt::AlignCenter);
	auto *monoCheck = new QCheckBox();
	monoCheck->setChecked(src.forceMono);
	monoLayout->addWidget(monoCheck);
	tableWidget->setCellWidget(row, 5, monoWidget);

	connect(monoCheck, &QCheckBox::checkStateChanged, this, [this] { saveToConfig(true); });

	// Column 6: Monitoring dropdown
	auto *monWidget = new QWidget();
	auto *monLayout = new QHBoxLayout(monWidget);
	monLayout->setContentsMargins(2, 0, 2, 0);
	auto *monCombo = new QComboBox();
	monCombo->addItem("Off", 0);
	monCombo->addItem("Mon", 1);
	monCombo->addItem("Both", 2);
	monCombo->setCurrentIndex(src.monitoringType);
	monCombo->setStyleSheet("QComboBox { background-color: #3b3b3b; border: 1px solid #555; border-radius: 3px; padding: 2px; font-size: 10px; }");
	monLayout->addWidget(monCombo);
	tableWidget->setCellWidget(row, 6, monWidget);

	connect(monCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { saveToConfig(true); });

	// Column 7: Configure button
	auto *cfgWidget = new QWidget();
	auto *cfgLayout = new QHBoxLayout(cfgWidget);
	cfgLayout->setContentsMargins(2, 0, 2, 0);
	cfgLayout->setAlignment(Qt::AlignCenter);
	auto *cfgBtn = new QPushButton();
	cfgBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	cfgBtn->setIcon(QIcon(":/super/assets/icons/settings.svg"));
	cfgBtn->setStyleSheet("QPushButton { color: #888; background: transparent; border: none; }");
	cfgBtn->setToolTip(obs_module_text("AsioSettings.Configure"));

	connect(cfgBtn, &QPushButton::clicked, this, [this, cfgBtn]() {
		for (int r = 0; r < tableWidget->rowCount(); ++r) {
			QWidget *w = tableWidget->cellWidget(r, 7);
			if (w && w->findChild<QPushButton *>() == cfgBtn) {
				openSourceProperties(r);
				break;
			}
		}
	});

	cfgLayout->addWidget(cfgBtn);
	tableWidget->setCellWidget(row, 7, cfgWidget);

	// Column 8: Filters button
	auto *filterWidget = new QWidget();
	auto *filterLayout = new QHBoxLayout(filterWidget);
	filterLayout->setContentsMargins(2, 0, 2, 0);
	filterLayout->setAlignment(Qt::AlignCenter);
	auto *filterBtn = new QPushButton();
	filterBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	filterBtn->setIcon(QIcon(":/super/assets/icons/sliders.svg"));
	filterBtn->setStyleSheet("QPushButton { color: #fff; background: transparent; border: none; }");
	filterBtn->setToolTip(obs_module_text("AsioSettings.Filters"));

	connect(filterBtn, &QPushButton::clicked, this, [this, filterBtn]() {
		for (int r = 0; r < tableWidget->rowCount(); ++r) {
			QWidget *w = tableWidget->cellWidget(r, 8);
			if (w && w->findChild<QPushButton *>() == filterBtn) {
				openSourceFilters(r);
				break;
			}
		}
	});

	filterLayout->addWidget(filterBtn);
	tableWidget->setCellWidget(row, 8, filterWidget);

	// Column 9: Edit button
	auto *editWidget = new QWidget();
	auto *editLayout = new QHBoxLayout(editWidget);
	editLayout->setContentsMargins(2, 0, 2, 0);
	editLayout->setAlignment(Qt::AlignCenter);
	auto *editBtn = new QPushButton();
	editBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	editBtn->setIcon(QIcon(":/super/assets/icons/edit.svg"));
	editBtn->setStyleSheet("QPushButton { color: #fff; background: transparent; border: none; }");
	editBtn->setToolTip(obs_module_text("AsioSettings.EditSource"));

	connect(editBtn, &QPushButton::clicked, this, [this, editBtn]() {
		for (int r = 0; r < tableWidget->rowCount(); ++r) {
			QWidget *w = tableWidget->cellWidget(r, 9);
			if (w && w->findChild<QPushButton *>() == editBtn) {
				editSource(r);
				break;
			}
		}
	});

	editLayout->addWidget(editBtn);
	tableWidget->setCellWidget(row, 9, editWidget);

	// Column 10: Delete button
	auto *delWidget = new QWidget();
	auto *delLayout = new QHBoxLayout(delWidget);
	delLayout->setContentsMargins(2, 0, 2, 0);
	delLayout->setAlignment(Qt::AlignCenter);
	auto *delBtn = new QPushButton();
	delBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	delBtn->setIcon(QIcon(":/super/assets/icons/trash-2.svg"));
	delBtn->setStyleSheet(
		"QPushButton { color: #fff; background: transparent; border: none; }"
		"QPushButton:hover { color: #ff5555; }"
	);
	delBtn->setToolTip(obs_module_text("AsioSettings.RemoveSource"));

	connect(delBtn, &QPushButton::clicked, this, [this, delBtn]() {
		for (int r = 0; r < tableWidget->rowCount(); ++r) {
			QWidget *w = tableWidget->cellWidget(r, 10);
			if (w && w->findChild<QPushButton *>() == delBtn) {
				deleteSource(r);
				break;
			}
		}
	});

	delLayout->addWidget(delBtn);
	tableWidget->setCellWidget(row, 10, delWidget);

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
		if (auto *item = tableWidget->item(i, 0)) {
			cfg.name = item->text();
			// Retrieve preserved settings and filters
			cfg.sourceSettings = item->data(kTIDS_SourceSettings).toJsonObject();
			cfg.sourceFilters = item->data(kTIDS_SourceFilters).toJsonArray();
			// Read channel from item data (stored at UserRole + 10)
			cfg.outputChannel = item->data(kTIDS_OutputChannel).toInt();
		}

		// Volume (column 2)
		if (auto *w = tableWidget->cellWidget(i, 2)) {
			if (auto *slider = w->findChild<QSlider *>()) {
				cfg.volume = slider->value() / 100.0f;
			}
		}

		// Balance (column 3)
		if (auto *w = tableWidget->cellWidget(i, 3)) {
			if (auto *slider = w->findChild<QSlider *>()) {
				cfg.balance = slider->value() / 100.0f;
			}
		}

		// Mute (column 4)
		if (auto *w = tableWidget->cellWidget(i, 4)) {
			if (auto *check = w->findChild<QCheckBox *>()) {
				cfg.muted = check->isChecked();
			}
		}

		// Mono (column 5)
		if (auto *w = tableWidget->cellWidget(i, 5)) {
			if (auto *check = w->findChild<QCheckBox *>()) {
				cfg.forceMono = check->isChecked();
			}
		}

		// Monitoring (column 6)
		if (auto *w = tableWidget->cellWidget(i, 6)) {
			if (auto *combo = w->findChild<QComboBox *>()) {
				cfg.monitoringType = combo->currentIndex();
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

int AsioSettingsDialog::findNextAvailableChannel() const
{
	QSet<int> usedChannels;
	for (int i = 0; i < tableWidget->rowCount(); i++) {
		auto *item = tableWidget->item(i, 0);
		if (item) {
			int ch = item->data(kTIDS_OutputChannel).toInt();
			if (ch > 0) {
				usedChannels.insert(ch);
			}
		}
	}

	for (int ch = 1; ch <= MAX_CHANNELS; ch++) {
		if (!usedChannels.contains(ch)) {
			return ch;
		}
	}

	return -1; // No available channel
}

// bool AsioSettingsDialog::isChannelOccupied(int channel, int excludeRow) const
// {
// 	for (int i = 0; i < tableWidget->rowCount(); i++) {
// 		if (i == excludeRow) continue;
//
// 		if (auto *w = tableWidget->cellWidget(i, 1)) {
// 			if (auto *spin = w->findChild<QSpinBox *>()) {
// 				if (spin->value() == channel) {
// 					return true;
// 				}
// 			}
// 		}
// 	}
// 	return false;
// }

// void AsioSettingsDialog::onChannelChanged(int row)
// {
// 	if (auto *w = tableWidget->cellWidget(row, 1)) {
// 		if (auto *spin = w->findChild<QSpinBox *>()) {
// 			int channel = spin->value();
//
// 			if (isChannelOccupied(channel, row)) {
// 				// Show warning
// 				spin->setStyleSheet(
// 					"QSpinBox { background-color: #5c2323; border: 1px solid #c62828; border-radius: 3px; padding: 2px; color: #ffcccc; }"
// 				);
// 				spin->setToolTip(obs_module_text("AsioSettings.ChannelOccupied"));
// 			} else {
// 				// Normal style
// 				spin->setStyleSheet(
// 					"QSpinBox { background-color: #3b3b3b; border: 1px solid #555; border-radius: 3px; padding: 2px; }"
// 				);
// 				spin->setToolTip("");
// 			}
// 		}
// 	}
//
// 	saveToConfig();
// }

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
	dlg.setOccupiedChannels(getOccupiedChannels());
	
	// Pre-fill with default unique name
	AsioSourceConfig defaultCfg;
	defaultCfg.name = generateUniqueName("ASIO Audio");
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
	newCfg.outputChannel = dlg.getChannel();
	newCfg.enabled = true;
	newCfg.muted = dlg.shouldStartMuted();

	addRowWidgets(row, newCfg);

	saveToConfig();
	updateRemoveButtonState();
	updateAddButtonState();
	
	// Open properties dialog if checkbox was checked
	if (openProps) {
		openSourceProperties(row);
	}
}

void AsioSettingsDialog::editSource(int row)
{
	if (row < 0 || row >= tableWidget->rowCount()) return;
	
	auto *item = tableWidget->item(row, 0);
	if (!item) return;
	
	QString currentName = item->text();
	int currentChannel = item->data(kTIDS_OutputChannel).toInt();
	
	// Build config from current row data
	AsioSourceConfig cfg;
	cfg.name = currentName;
	cfg.outputChannel = currentChannel;
	cfg.sourceSettings = item->data(kTIDS_SourceSettings).toJsonObject();
	cfg.sourceFilters = item->data(kTIDS_SourceFilters).toJsonArray();
	
	// Create dialog in edit mode
	AsioSourceDialog dlg(AsioSourceDialog::EditMode, this);
	dlg.setOccupiedChannels(getOccupiedChannels(row));
	dlg.setConfig(cfg);
	
	if (dlg.exec() != QDialog::Accepted) {
		return;
	}
	
	// Update name
	item->setText(dlg.getName());
	
	// Update channel in item data and label
	int newChannel = dlg.getChannel();
	item->setData(kTIDS_OutputChannel, newChannel);
	if (auto *w = tableWidget->cellWidget(row, 1)) {
		if (auto *lbl = w->findChild<QLabel *>()) {
			lbl->setText(QString::number(newChannel));
		}
	}
	
	saveToConfig();
	updateRowTooltip(row);
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
	
	auto *item = tableWidget->item(row, 0);
	if (!item) return;
	
	QString baseName = item->text();
	QString suggestedName = generateUniqueName(baseName + " Copy");
	
	// Create dialog pre-filled
	AsioSourceDialog dlg(AsioSourceDialog::AddMode, this);
	dlg.setOccupiedChannels(getOccupiedChannels());
	
	AsioSourceConfig cfg;
	cfg.name = suggestedName;
	cfg.outputChannel = findNextAvailableChannel();
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
	
	addRowWidgets(newRow, newCfg);
	
	saveToConfig();
	updateRemoveButtonState();
	updateAddButtonState();
	
	// Open properties dialog if checkbox was checked
	if (openProps) {
		openSourceProperties(newRow);
	}
}

void AsioSettingsDialog::deleteSource(int row)
{
	if (row < 0 || row >= tableWidget->rowCount()) return;
	
	auto *item = tableWidget->item(row, 0);
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
	saveToConfig();
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
	auto *item = tableWidget->item(row, 0);
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

QSet<int> AsioSettingsDialog::getOccupiedChannels(int excludeRow) const
{
	QSet<int> occupied;
	
	// Check our managed sources
	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (i == excludeRow) continue;
		
		auto *item = tableWidget->item(i, 0);
		if (item) {
			int ch = item->data(kTIDS_OutputChannel).toInt();
			if (ch > 0) {
				occupied.insert(ch);
			}
		}
	}
	
	return occupied;
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

	// Get channel from item data
	int channel = 1;
	if (auto *item = tableWidget->item(row, 0)) {
		channel = item->data(kTIDS_OutputChannel).toInt();
	}

	// Get the actual running source from the output channel
	obs_source_t *source = obs_get_output_source(channel - 1); // OBS uses 0-indexed
	if (!source) {
		QMessageBox::warning(this, obs_module_text("Error"),
			obs_module_text("Error.CreateAsioSource"));
		return;
	}

	// Open the properties dialog for the actual running source
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
					if (auto *item = tableWidget->item(row, 0)) {
						item->setData(kTIDS_SourceSettings, cfg.sourceSettings);
					}

					AsioConfig::get()->save();
					obs_log(LOG_INFO, "Saved ASIO source settings for channel %d", channel);
				}
			}
			obs_data_release(newSettings);
		}
	}

	obs_source_release(source); // Release ref from obs_get_output_source
}

void AsioSettingsDialog::openSourceFilters(int row)
{
	if (row < 0 || row >= tableWidget->rowCount()) {
		return;
	}

	// Get channel from item data
	int channel = 1;
	if (auto *item = tableWidget->item(row, 0)) {
		channel = item->data(kTIDS_OutputChannel).toInt();
	}

	// Get the actual running source from the output channel
	obs_source_t *source = obs_get_output_source(channel - 1); // OBS uses 0-indexed
	if (!source) {
		QMessageBox::warning(this, obs_module_text("Error"),
			obs_module_text("Error.CreateAsioSource"));
		return;
	}

	// Open the filters dialog for the actual running source
	obs_frontend_open_source_filters(source);

	obs_source_release(source); // Release ref from obs_get_output_source
}

void AsioSettingsDialog::updateRemoveButtonState()
{
	btnRemove->setEnabled(tableWidget->rowCount() > 0 && !tableWidget->selectedItems().isEmpty());
}

void AsioSettingsDialog::updateAddButtonState()
{
	bool canAdd = tableWidget->rowCount() < MAX_CHANNELS && findNextAvailableChannel() >= 0;
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

void AsioSettingsDialog::updateSourceName(int channel, const QString &name)
{
	// Called from OBS signal thread? ensure UI thread
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, channel, name]() {
			updateSourceName(channel, name);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 0)) {
			if (item->data(kTIDS_OutputChannel).toInt() == channel) {
				// Block signals to prevent itemChanged from triggering save
				tableWidget->blockSignals(true);
				item->setText(name);
				tableWidget->blockSignals(false);
				break;
			}
		}
	}
}

void AsioSettingsDialog::updateSourceMuted(int channel, bool muted)
{
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, channel, muted]() {
			updateSourceMuted(channel, muted);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 0)) {
			if (item->data(kTIDS_OutputChannel).toInt() == channel) {
				if (auto *muteW = tableWidget->cellWidget(i, 4)) {
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

void AsioSettingsDialog::updateSourceVolume(int channel, float volume)
{
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, channel, volume]() {
			updateSourceVolume(channel, volume);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 0)) {
			if (item->data(kTIDS_OutputChannel).toInt() == channel) {
				if (auto *volW = tableWidget->cellWidget(i, 2)) {
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

void AsioSettingsDialog::updateSourceBalance(int channel, float balance)
{
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, channel, balance]() {
			updateSourceBalance(channel, balance);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 0)) {
			if (item->data(kTIDS_OutputChannel).toInt() == channel) {
				if (auto *balW = tableWidget->cellWidget(i, 3)) {
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

void AsioSettingsDialog::updateSourceMonitoring(int channel, int type)
{
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, channel, type]() {
			updateSourceMonitoring(channel, type);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 0)) {
			if (item->data(kTIDS_OutputChannel).toInt() == channel) {
				if (auto *monW = tableWidget->cellWidget(i, 6)) {
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

void AsioSettingsDialog::updateSourceMono(int channel, bool mono)
{
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, channel, mono]() {
			updateSourceMono(channel, mono);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 0)) {
			if (item->data(kTIDS_OutputChannel).toInt() == channel) {
				if (auto *monoW = tableWidget->cellWidget(i, 5)) {
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

void AsioSettingsDialog::updateSourceSettings(int channel, const QJsonObject &settings)
{
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, channel, settings]() {
			updateSourceSettings(channel, settings);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 0)) {
			if (item->data(kTIDS_OutputChannel).toInt() == channel) {
				item->setData(kTIDS_SourceSettings, settings);
				break;
			}
		}
	}
}

void AsioSettingsDialog::updateSourceFilters(int channel, const QJsonArray &filters)
{
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, channel, filters]() {
			updateSourceFilters(channel, filters);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *item = tableWidget->item(i, 0)) {
			if (item->data(kTIDS_OutputChannel).toInt() == channel) {
				item->setData(kTIDS_SourceFilters, filters);
				break;
			}
		}
	}
}
