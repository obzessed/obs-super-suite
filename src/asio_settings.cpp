#include <obs-module.h>
#include <obs-frontend-api.h>

#include <plugin-support.h>

#include "asio_settings.h"
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
	resize(600, 380);

	auto *mainLayout = new QVBoxLayout(this);

	// Info label
	auto *infoLabel = new QLabel(obs_module_text("AsioSettings.Info"), this);
	infoLabel->setWordWrap(true);
	infoLabel->setStyleSheet("color: #888; margin-bottom: 8px;");
	mainLayout->addWidget(infoLabel);

	// Table - 5 columns: Name, Channel, Configure, Filters, Delete
	tableWidget = new QTableWidget(this);
	tableWidget->setColumnCount(5);
	tableWidget->setHorizontalHeaderLabels({
		obs_module_text("AsioSettings.SourceName"),
		obs_module_text("AsioSettings.OutputChannel"),
		"",  // Configure button
		"",  // Filters button
		""   // Delete button column
	});

	tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
	tableWidget->setColumnWidth(2, 80);
	tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
	tableWidget->setColumnWidth(3, 60);
	tableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
	tableWidget->setColumnWidth(4, 40);

	tableWidget->verticalHeader()->setVisible(false);
	tableWidget->verticalHeader()->setDefaultSectionSize(36);
	tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
	tableWidget->setAlternatingRowColors(true);
	tableWidget->setShowGrid(false);
	tableWidget->setEditTriggers(QAbstractItemView::DoubleClicked);

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

	btnLayout->addWidget(btnAdd);
	btnLayout->addStretch();
	btnLayout->addWidget(btnRemove);

	mainLayout->addLayout(btnLayout);

	updateRemoveButtonState();
	updateAddButtonState();
}

void AsioSettingsDialog::addRowWidgets(int row, const AsioSourceConfig &src)
{
	// Source name (editable)
	auto *nameItem = new QTableWidgetItem(src.name);
	// Store settings (UserRole) and filters (UserRole + 1) in the item to preserve them
	nameItem->setData(Qt::UserRole, src.sourceSettings);
	nameItem->setData(Qt::UserRole + 1, src.sourceFilters);
	tableWidget->setItem(row, 0, nameItem);

	// Output channel (spin box) - range 41-50
	auto *channelWidget = new QWidget();
	auto *channelLayout = new QHBoxLayout(channelWidget);
	channelLayout->setContentsMargins(4, 0, 4, 0);
	auto *spinBox = new QSpinBox();
	spinBox->setRange(ASIO_START_CHANNEL, ASIO_END_CHANNEL);
	spinBox->setValue(src.outputChannel);
	spinBox->setStyleSheet("QSpinBox { background-color: #3b3b3b; border: 1px solid #555; border-radius: 3px; padding: 2px; }");
	channelLayout->addWidget(spinBox);
	tableWidget->setCellWidget(row, 1, channelWidget);

	// Connect channel change to validation
	connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, spinBox]() {
		// Find which row this spinbox belongs to
		for (int r = 0; r < tableWidget->rowCount(); ++r) {
			QWidget *w = tableWidget->cellWidget(r, 1);
			if (w && w->findChild<QSpinBox *>() == spinBox) {
				onChannelChanged(r);
				break;
			}
		}
	});

	// Configure button
	auto *cfgWidget = new QWidget();
	auto *cfgLayout = new QHBoxLayout(cfgWidget);
	cfgLayout->setContentsMargins(2, 0, 2, 0);
	cfgLayout->setAlignment(Qt::AlignCenter);
	auto *cfgBtn = new QPushButton(obs_module_text("AsioSettings.Configure"));
	cfgBtn->setStyleSheet(
		"QPushButton { background-color: #4a5568; color: #e2e8f0; border-radius: 3px; padding: 4px 8px; font-size: 11px; }"
		"QPushButton:hover { background-color: #5a6578; color: white; }"
	);

	connect(cfgBtn, &QPushButton::clicked, this, [this, cfgBtn]() {
		for (int r = 0; r < tableWidget->rowCount(); ++r) {
			QWidget *w = tableWidget->cellWidget(r, 2);
			if (w && w->findChild<QPushButton *>() == cfgBtn) {
				openSourceProperties(r);
				break;
			}
		}
	});

	cfgLayout->addWidget(cfgBtn);
	tableWidget->setCellWidget(row, 2, cfgWidget);

	// Filters button
	auto *filterWidget = new QWidget();
	auto *filterLayout = new QHBoxLayout(filterWidget);
	filterLayout->setContentsMargins(2, 0, 2, 0);
	filterLayout->setAlignment(Qt::AlignCenter);
	auto *filterBtn = new QPushButton(obs_module_text("AsioSettings.Filters"));
	filterBtn->setStyleSheet(
		"QPushButton { background-color: #3d5a80; color: #e2e8f0; border-radius: 3px; padding: 4px 8px; font-size: 11px; }"
		"QPushButton:hover { background-color: #4d6a90; color: white; }"
	);

	connect(filterBtn, &QPushButton::clicked, this, [this, filterBtn]() {
		for (int r = 0; r < tableWidget->rowCount(); ++r) {
			QWidget *w = tableWidget->cellWidget(r, 3);
			if (w && w->findChild<QPushButton *>() == filterBtn) {
				openSourceFilters(r);
				break;
			}
		}
	});

	filterLayout->addWidget(filterBtn);
	tableWidget->setCellWidget(row, 3, filterWidget);

	// Delete button
	auto *delWidget = new QWidget();
	auto *delLayout = new QHBoxLayout(delWidget);
	delLayout->setContentsMargins(0, 0, 0, 0);
	delLayout->setAlignment(Qt::AlignCenter);
	auto *delBtn = new QPushButton();
	delBtn->setText("✕");
	delBtn->setMaximumWidth(30);
	delBtn->setStyleSheet(
		"QPushButton { color: #888; background: transparent; border: none; font-size: 14px; }"
		"QPushButton:hover { color: #ff5555; }"
	);
	delBtn->setToolTip(obs_module_text("AsioSettings.RemoveSource"));

	connect(delBtn, &QPushButton::clicked, this, [this, delBtn]() {
		for (int r = 0; r < tableWidget->rowCount(); ++r) {
			QWidget *w = tableWidget->cellWidget(r, 4);
			if (w && w->findChild<QPushButton *>() == delBtn) {
				tableWidget->removeRow(r);
				saveToConfig();
				updateRemoveButtonState();
				updateAddButtonState();
				break;
			}
		}
	});

	delLayout->addWidget(delBtn);
	tableWidget->setCellWidget(row, 4, delWidget);
}

void AsioSettingsDialog::loadFromConfig()
{
	tableWidget->setRowCount(0);

	const auto &sources = AsioConfig::get()->getSources();
	for (const auto &src : sources) {
		int row = tableWidget->rowCount();
		tableWidget->insertRow(row);
		addRowWidgets(row, src);
	}

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
			cfg.sourceSettings = item->data(Qt::UserRole).toJsonObject();
			cfg.sourceFilters = item->data(Qt::UserRole + 1).toJsonArray();
		}

		// Output channel
		if (auto *w = tableWidget->cellWidget(i, 1)) {
			if (auto *spin = w->findChild<QSpinBox *>()) {
				cfg.outputChannel = spin->value();
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
		if (auto *w = tableWidget->cellWidget(i, 1)) {
			if (auto *spin = w->findChild<QSpinBox *>()) {
				usedChannels.insert(spin->value());
			}
		}
	}

	for (int ch = ASIO_START_CHANNEL; ch <= ASIO_END_CHANNEL; ch++) {
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
//
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
	if (tableWidget->rowCount() >= ASIO_MAX_SOURCES) {
		QMessageBox::warning(this,
			obs_module_text("AsioSettings.Title"),
			obs_module_text("AsioSettings.MaxSourcesReached"));
		return;
	}

	int nextChannel = findNextAvailableChannel();
	if (nextChannel < 0) {
		QMessageBox::warning(this,
			obs_module_text("AsioSettings.Title"),
			obs_module_text("AsioSettings.NoChannelsAvailable"));
		return;
	}

	int row = tableWidget->rowCount();
	tableWidget->insertRow(row);

	AsioSourceConfig newCfg;
	newCfg.name = QString("ASIO Audio %1").arg(row + 1);
	newCfg.outputChannel = nextChannel;
	newCfg.enabled = true;

	addRowWidgets(row, newCfg);

	saveToConfig();
	updateRemoveButtonState();
	updateAddButtonState();
}

void AsioSettingsDialog::removeSelectedSource()
{
	auto selected = tableWidget->selectedItems();
	if (!selected.isEmpty()) {
		int row = selected.first()->row();
		tableWidget->removeRow(row);
		saveToConfig();
		updateRemoveButtonState();
		updateAddButtonState();
	}
}

void AsioSettingsDialog::openSourceProperties(int row)
{
	if (row < 0 || row >= tableWidget->rowCount()) {
		return;
	}

	// Get channel from the spinbox
	int channel = ASIO_START_CHANNEL;
	if (auto *w = tableWidget->cellWidget(row, 1)) {
		if (auto *spin = w->findChild<QSpinBox *>()) {
			channel = spin->value();
		}
	}

	// Get the actual running source from the output channel
	obs_source_t *source = obs_get_output_source(channel);
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
						item->setData(Qt::UserRole, cfg.sourceSettings);
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

	// Get channel from the spinbox
	int channel = ASIO_START_CHANNEL;
	if (auto *w = tableWidget->cellWidget(row, 1)) {
		if (auto *spin = w->findChild<QSpinBox *>()) {
			channel = spin->value();
		}
	}

	// Get the actual running source from the output channel
	obs_source_t *source = obs_get_output_source(channel);
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
	bool canAdd = tableWidget->rowCount() < ASIO_MAX_SOURCES && findNextAvailableChannel() >= 0;
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

// // Check if a channel is already selected by another row
// bool AsioSettingsDialog::isChannelOccupied(int channel, int excludeRow) const
// {
// 	for (int i = 0; i < tableWidget->rowCount(); i++) {
// 		if (i == excludeRow) continue;
// 		if (auto *w = tableWidget->cellWidget(i, 1)) {
// 			if (auto *spin = w->findChild<QSpinBox *>()) {
// 				if (spin->value() == channel) return true;
// 			}
// 		}
// 	}
// 	return false;
// }

// void AsioSettingsDialog::onChannelChanged(int row)
// {
// 	// Check for duplicates
// 	int channel = -1;
// 	QSpinBox *spinBox = nullptr;
// 	if (auto *w = tableWidget->cellWidget(row, 1)) {
// 		spinBox = w->findChild<QSpinBox *>();
// 		if (spinBox) channel = spinBox->value();
// 	}
//
// 	if (spinBox) {
// 		if (channel != -1 && isChannelOccupied(channel, row)) {
// 			// Highlight duplicate channel
// 			spinBox->setStyleSheet("QSpinBox { background-color: #552222; border: 1px solid #ff5555; color: white; padding: 2px; }");
// 			spinBox->setToolTip(obs_module_text("AsioSettings.ChannelInUse"));
// 		} else {
// 			// Normal style
// 			spinBox->setStyleSheet("QSpinBox { background-color: #3b3b3b; border: 1px solid #555; border-radius: 3px; padding: 2px; }");
// 			spinBox->setToolTip("");
// 		}
// 	}
//
// 	updateAddButtonState();
// 	// Immediate save and refresh to reflect channel change
// 	saveToConfig(true);
// }

void AsioSettingsDialog::updateSourceSettings(int channel, const QJsonObject &settings)
{
	// Called from OBS signal thread? ensure UI thread
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, channel, settings]() {
			updateSourceSettings(channel, settings);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *w = tableWidget->cellWidget(i, 1)) {
			if (auto *spin = w->findChild<QSpinBox *>()) {
				if (spin->value() == channel) {
					if (auto *item = tableWidget->item(i, 0)) {
						item->setData(Qt::UserRole, settings);
					}
					break;
				}
			}
		}
	}
}

void AsioSettingsDialog::updateSourceFilters(int channel, const QJsonArray &filters)
{
	// Called from OBS signal thread? ensure UI thread
	if (QThread::currentThread() != this->thread()) {
		QMetaObject::invokeMethod(this, [this, channel, filters]() {
			updateSourceFilters(channel, filters);
		});
		return;
	}

	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (auto *w = tableWidget->cellWidget(i, 1)) {
			if (auto *spin = w->findChild<QSpinBox *>()) {
				if (spin->value() == channel) {
					if (auto *item = tableWidget->item(i, 0)) {
						item->setData(Qt::UserRole + 1, filters);
					}
					break;
				}
			}
		}
	}
}

// Check if a channel is already selected by another row
bool AsioSettingsDialog::isChannelOccupied(int channel, int excludeRow) const
{
	for (int i = 0; i < tableWidget->rowCount(); i++) {
		if (i == excludeRow) continue;
		if (auto *w = tableWidget->cellWidget(i, 1)) {
			if (auto *spin = w->findChild<QSpinBox *>()) {
				if (spin->value() == channel) return true;
			}
		}
	}
	return false;
}

void AsioSettingsDialog::onChannelChanged(int row)
{
	// Check for duplicates
	int channel = -1;
	QSpinBox *spinBox = nullptr;
	if (auto *w = tableWidget->cellWidget(row, 1)) {
		spinBox = w->findChild<QSpinBox *>();
		if (spinBox) channel = spinBox->value();
	}

	if (spinBox) {
		if (channel != -1 && isChannelOccupied(channel, row)) {
			// Highlight duplicate channel
			spinBox->setStyleSheet("QSpinBox { background-color: #552222; border: 1px solid #ff5555; color: white; padding: 2px; }");
			spinBox->setToolTip(obs_module_text("AsioSettings.ChannelInUse"));
		} else {
			// Normal style
			spinBox->setStyleSheet("QSpinBox { background-color: #3b3b3b; border: 1px solid #555; border-radius: 3px; padding: 2px; }");
			spinBox->setToolTip("");
		}
	}

	updateAddButtonState();
	// Immediate save and refresh to reflect channel change
	saveToConfig(true);
}
