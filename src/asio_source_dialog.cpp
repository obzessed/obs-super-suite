#include "asio_source_dialog.h"
#include "asio_config.h"
#include <obs-module.h>
#include <QMessageBox>
#include <QStandardItemModel>

AsioSourceDialog::AsioSourceDialog(Mode mode, QWidget *parent)
	: QDialog(parent), m_mode(mode)
{
	setupUi();
	
	setWindowTitle(mode == AddMode 
		? obs_module_text("AsioSettings.AddSource")
		: obs_module_text("AsioSettings.EditSource"));
	
	setMinimumWidth(300);
}

void AsioSourceDialog::setupUi()
{
	auto *mainLayout = new QVBoxLayout(this);
	auto *formLayout = new QFormLayout();
	
	// Name input
	m_nameEdit = new QLineEdit(this);
	m_nameEdit->setPlaceholderText(obs_module_text("AsioSettings.EnterSourceName"));
	formLayout->addRow(obs_module_text("AsioSettings.SourceName"), m_nameEdit);
	
	// Channel dropdown
	m_channelCombo = new QComboBox(this);
	formLayout->addRow(obs_module_text("AsioSettings.OutputChannel"), m_channelCombo);
	
	mainLayout->addLayout(formLayout);
	
	// Error label (hidden by default)
	m_errorLabel = new QLabel(this);
	m_errorLabel->setStyleSheet("QLabel { color: #ff6666; }");
	m_errorLabel->hide();
	mainLayout->addWidget(m_errorLabel);
	
	mainLayout->addSpacing(10);
	
	// Start muted checkbox (only in Add mode)
	m_mutedCheck = new QCheckBox(obs_module_text("AsioSettings.StartMuted"), this);
	m_mutedCheck->setChecked(true); // Default to checked
	if (m_mode == EditMode) {
		m_mutedCheck->hide();
	}
	mainLayout->addWidget(m_mutedCheck);
	
	// Open properties checkbox (only in Add mode)
	m_openPropertiesCheck = new QCheckBox(obs_module_text("AsioSettings.OpenPropertiesAfter"), this);
	m_openPropertiesCheck->setChecked(true); // Default to checked
	if (m_mode == EditMode) {
		m_openPropertiesCheck->hide();
	}
	mainLayout->addWidget(m_openPropertiesCheck);
	
	mainLayout->addSpacing(5);
	
	// Buttons
	auto *buttonLayout = new QHBoxLayout();
	buttonLayout->addStretch();
	
	m_cancelButton = new QPushButton(obs_module_text("Cancel"), this);
	m_okButton = new QPushButton(obs_module_text("OK"), this);
	m_okButton->setDefault(true);
	
	buttonLayout->addWidget(m_cancelButton);
	buttonLayout->addWidget(m_okButton);
	mainLayout->addLayout(buttonLayout);
	
	// Connections
	connect(m_nameEdit, &QLineEdit::textChanged, this, &AsioSourceDialog::validateInput);
	connect(m_channelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
		this, &AsioSourceDialog::validateInput);
	connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
	connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
	
	// Populate channels
	populateChannels();
	validateInput();
}

void AsioSourceDialog::populateChannels()
{
	m_channelCombo->clear();
	
	for (int ch = 1; ch <= MAX_CHANNELS; ch++) {
		m_channelCombo->addItem(QString::number(ch), ch);
		
		// Check if channel is occupied
		bool occupied = m_occupiedChannels.contains(ch);
		
		// In edit mode, current channel is always enabled
		if (m_mode == EditMode && ch == m_currentChannel) {
			occupied = false;
		}
		
		// Grey out occupied channels
		if (occupied) {
			QStandardItemModel *model = qobject_cast<QStandardItemModel*>(m_channelCombo->model());
			if (model) {
				QStandardItem *item = model->item(m_channelCombo->count() - 1);
				item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
				item->setText(QString("%1 (in use)").arg(ch));
			}
		}
	}
	
	// Select first available channel
	for (int i = 0; i < m_channelCombo->count(); i++) {
		QStandardItemModel *model = qobject_cast<QStandardItemModel*>(m_channelCombo->model());
		if (model && model->item(i)->flags() & Qt::ItemIsEnabled) {
			m_channelCombo->setCurrentIndex(i);
			break;
		}
	}
}

void AsioSourceDialog::setOccupiedChannels(const QSet<int> &channels)
{
	m_occupiedChannels = channels;
	populateChannels();
}

void AsioSourceDialog::setCurrentChannel(int channel)
{
	m_currentChannel = channel;
	populateChannels();
	
	// Select the current channel
	for (int i = 0; i < m_channelCombo->count(); i++) {
		if (m_channelCombo->itemData(i).toInt() == channel) {
			m_channelCombo->setCurrentIndex(i);
			break;
		}
	}
}

void AsioSourceDialog::setConfig(const AsioSourceConfig &cfg)
{
	// Set channel FIRST - because setText triggers validateInput which needs m_currentChannel
	setCurrentChannel(cfg.outputChannel);
	m_nameEdit->setText(cfg.name);
}

QString AsioSourceDialog::getName() const
{
	return m_nameEdit->text().trimmed();
}

int AsioSourceDialog::getChannel() const
{
	return m_channelCombo->currentData().toInt();
}

bool AsioSourceDialog::shouldOpenProperties() const
{
	return m_openPropertiesCheck->isChecked();
}

void AsioSourceDialog::setOpenProperties(bool open)
{
	m_openPropertiesCheck->setChecked(open);
}

bool AsioSourceDialog::shouldStartMuted() const
{
	return m_mutedCheck->isChecked();
}

void AsioSourceDialog::setStartMuted(bool muted)
{
	m_mutedCheck->setChecked(muted);
}

void AsioSourceDialog::validateInput()
{
	QString name = getName();
	int channel = getChannel();
	bool valid = true;
	QString error;
	
	// Check empty name
	if (name.isEmpty()) {
		valid = false;
		error = obs_module_text("AsioSettings.ErrorEmptyName");
	}
	
	// Check duplicate name (against existing sources)
	if (valid) {
		const auto &sources = AsioConfig::get()->getSources();
		for (const auto &src : sources) {
			// In edit mode, skip self
			if (m_mode == EditMode && src.outputChannel == m_currentChannel) {
				continue;
			}
			if (src.name == name) {
				valid = false;
				error = obs_module_text("AsioSettings.ErrorDuplicateName");
				break;
			}
		}
	}
	
	// Check channel is not occupied by another ASIO source
	if (valid && channel > 0) {
		// In edit mode, current channel is always valid (it's our own)
		bool isOccupied = m_occupiedChannels.contains(channel);
		if (m_mode == EditMode && channel == m_currentChannel) {
			isOccupied = false;
		}
		if (isOccupied) {
			valid = false;
			error = obs_module_text("AsioSettings.ErrorChannelInUse");
		}
	}
	
	m_okButton->setEnabled(valid);
	if (!valid && !error.isEmpty()) {
		m_errorLabel->setText(error);
		m_errorLabel->show();
	} else {
		m_errorLabel->hide();
	}
}
