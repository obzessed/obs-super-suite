#include "./audio_source_dialog.h"

#include <obs-module.h>

#include <QFormLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QGroupBox>
#include <QStandardItemModel>

#include "../models/audio_channel_source_config.h"

AudioSourceDialog::AudioSourceDialog(Mode mode, QWidget *parent)
	: QDialog(parent), m_mode(mode)
{
	setupUi();
	
	switch (mode) {
		case AddMode:
			setWindowTitle(obs_module_text("AsioSettings.AddSource"));
			break;
		case EditMode:
			setWindowTitle(obs_module_text("AsioSettings.EditSource"));
			break;
		case DuplicateMode:
			setWindowTitle(obs_module_text("AsioSettings.DuplicateSource"));
			break;
	}
	
	setMinimumWidth(300);
}

void AudioSourceDialog::setupUi()
{
	auto *mainLayout = new QVBoxLayout(this);
	auto *formLayout = new QFormLayout();
	
	// Name input
	m_nameEdit = new QLineEdit(this);
	m_nameEdit->setPlaceholderText(obs_module_text("AsioSettings.EnterSourceName"));
	formLayout->addRow(obs_module_text("AsioSettings.SourceName"), m_nameEdit);
	
	// Source type dropdown - dynamically discover audio input types
	m_typeCombo = new QComboBox(this);
	
	// Loop over all input source types and add those with audio capability
	size_t idx = 0;
	const char *typeId = nullptr;
	while (obs_enum_input_types(idx++, &typeId)) {
		if (!typeId) continue;

		const uint32_t outputFlags = obs_get_source_output_flags(typeId);

		// Skip if source is internal
		if (outputFlags & OBS_SOURCE_CAP_DISABLED) continue;

		// Check if it has audio capability
		if (!(outputFlags & OBS_SOURCE_AUDIO)) continue;

		// Add to combo with display name
		const char *displayName = obs_source_get_display_name(typeId);
		m_typeCombo->addItem(displayName ? displayName : typeId, typeId);
	}

	m_typeCombo->setCurrentIndex(0); // Default to first available
	formLayout->addRow(obs_module_text("AsioSettings.SourceType"), m_typeCombo);
	
	// Disable source type selection for edit/duplicate modes (can't change type of existing source)
	if (m_mode != AddMode) {
		m_typeCombo->setEnabled(false);
		m_typeCombo->setToolTip("Source type cannot be changed after creation");
	}
	
	// Canvas dropdown (above channel)
	m_canvasCombo = new QComboBox(this);
	populateCanvases();
	formLayout->addRow(obs_module_text("AsioSettings.Canvas"), m_canvasCombo);
	
	// Channel dropdown
	m_channelCombo = new QComboBox(this);
	formLayout->addRow(obs_module_text("AsioSettings.OutputChannel"), m_channelCombo);
	
	mainLayout->addLayout(formLayout);
	
	// Error label (hidden by default)
	m_errorLabel = new QLabel(this);
	m_errorLabel->setStyleSheet("QLabel { color: #ff6666; }");
	m_errorLabel->hide();
	mainLayout->addWidget(m_errorLabel);
	
	// Reserved channel warning label (hidden by default)
	m_reservedWarningLabel = new QLabel(this);
	m_reservedWarningLabel->setText(obs_module_text("AsioSettings.ReservedChannelWarning"));
	m_reservedWarningLabel->setStyleSheet("QLabel { color: #ffaa00; font-size: 11px; }");
	m_reservedWarningLabel->setWordWrap(true);
	m_reservedWarningLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	m_reservedWarningLabel->hide();
	mainLayout->addWidget(m_reservedWarningLabel);
	
	mainLayout->addSpacing(10);
	
	// Start muted checkbox (only in Add mode)
	m_mutedCheck = new QCheckBox(obs_module_text("AsioSettings.StartMuted"), this);
	m_mutedCheck->setChecked(true); // Default to checked
	if (m_mode == EditMode) {
		m_mutedCheck->hide();
	}
	mainLayout->addWidget(m_mutedCheck);
	
	// Tracks checkboxes (audio mixers)
	auto *tracksGroup = new QGroupBox("Tracks", this);
	tracksGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	auto *tracksLayout = new QHBoxLayout(tracksGroup);
	tracksLayout->setContentsMargins(8, 4, 8, 4);
	for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
		m_trackChecks[i] = new QCheckBox(QString::number(i + 1), tracksGroup);
		m_trackChecks[i]->setChecked(true); // Default all tracks enabled
		tracksLayout->addWidget(m_trackChecks[i]);
	}
	mainLayout->addWidget(tracksGroup);
	
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
	connect(m_nameEdit, &QLineEdit::textChanged, this, &AudioSourceDialog::validateInput);
	connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &AudioSourceDialog::onTypeChanged);
	connect(m_canvasCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &AudioSourceDialog::onCanvasChanged);
	connect(m_channelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
		this, &AudioSourceDialog::validateInput);
	connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
	connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
	
	// Populate channels and update initial state
	populateChannels();
	onTypeChanged(); // Update open properties checkbox based on selected type
	validateInput();
}

// Callback context for canvas enumeration
struct CanvasEnumContext {
	QComboBox *combo;
	obs_canvas_t *mainCanvas;
	int mainIndex;
};

// Callback for obs_enum_canvases
static bool canvas_enum_cb(void *param, obs_canvas_t *canvas)
{
	auto *ctx = static_cast<CanvasEnumContext*>(param);
	const char *name = obs_canvas_get_name(canvas);
	const char *uuid = obs_canvas_get_uuid(canvas);
	
	// Check if this is the main canvas
	bool isMain = (canvas == ctx->mainCanvas);
	QString displayName;
	if (isMain) {
		displayName = obs_module_text("AsioSettings.MainCanvas");
		ctx->mainIndex = ctx->combo->count();
	} else {
		displayName = name ? QString::fromUtf8(name) : QString("Canvas %1").arg(ctx->combo->count() + 1);
	}
	
	ctx->combo->addItem(displayName, QString::fromUtf8(uuid ? uuid : ""));
	return true;
}

void AudioSourceDialog::populateCanvases()
{
	m_canvasCombo->clear();
	
	// Enumerate all canvases, identifying main canvas
	CanvasEnumContext ctx;
	ctx.combo = m_canvasCombo;
	ctx.mainCanvas = obs_get_main_canvas();
	ctx.mainIndex = 0;
	
	obs_enum_canvases(canvas_enum_cb, &ctx);
	
	// Select main canvas by default
	m_canvasCombo->setCurrentIndex(ctx.mainIndex);
}

void AudioSourceDialog::onCanvasChanged()
{
	// Refresh channel list for the selected canvas
	populateChannels();
	validateInput();
}

void AudioSourceDialog::onTypeChanged()
{
	// Update open properties checkbox based on whether selected source type is configurable
	QString typeId = m_typeCombo->currentData().toString();
	bool configurable = obs_is_source_configurable(typeId.toUtf8().constData());
	
	m_openPropertiesCheck->setEnabled(configurable);
	if (configurable) {
		m_openPropertiesCheck->setChecked(true);
		m_openPropertiesCheck->setToolTip("");
	} else {
		m_openPropertiesCheck->setChecked(false);
		m_openPropertiesCheck->setToolTip("This source type has no configurable properties");
	}
}

void AudioSourceDialog::populateChannels()
{
	m_channelCombo->clear();
	
	// Get the currently selected canvas
	QString selectedCanvasUuid = m_canvasCombo->currentData().toString();
	obs_canvas_t *canvas = selectedCanvasUuid.isEmpty() ? obs_get_main_canvas()
		: obs_get_canvas_by_uuid(selectedCanvasUuid.toUtf8().constData());
	if (!canvas) canvas = obs_get_main_canvas();
	
	// Add (none) option for sources not bound to any channel
	m_channelCombo->addItem("(none)", -1);
	
	for (int ch = 1; ch <= MAX_CHANNELS; ch++) {
		// Show OBS channel reservation names for channels 1-7
		QString channelName;
		switch (ch) {
			case 1: channelName = "1 - Scene Transition"; break;
			case 2: channelName = "2 - Desktop Audio 1"; break;
			case 3: channelName = "3 - Desktop Audio 2"; break;
			case 4: channelName = "4 - Mic/Aux 1"; break;
			case 5: channelName = "5 - Mic/Aux 2"; break;
			case 6: channelName = "6 - Mic/Aux 3"; break;
			case 7: channelName = "7 - Mic/Aux 4"; break;
			default: channelName = QString::number(ch); break;
		}
		m_channelCombo->addItem(channelName, ch);
		
		// Check if channel is occupied by querying OBS directly
		obs_source_t *existingSource = obs_canvas_get_channel(canvas, ch - 1); // 0-indexed
		bool occupied = (existingSource != nullptr);
		if (existingSource) obs_source_release(existingSource);
		
		// In edit mode, current channel on current canvas is always enabled (it's our own)
		if (m_mode == EditMode && ch == m_currentChannel && selectedCanvasUuid == m_currentCanvas) {
			occupied = false;
		}
		
		// Grey out occupied channels
		if (occupied) {
			if (auto *model = qobject_cast<QStandardItemModel *>(m_channelCombo->model())) {
				QStandardItem *item = model->item(m_channelCombo->count() - 1);
				item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
				item->setText(QString("%1 (in use)").arg(channelName));
			}
		}
	}
	
	// Select first available channel
	for (int i = 0; i < m_channelCombo->count(); i++) {
		if (auto *model = qobject_cast<QStandardItemModel *>(m_channelCombo->model());
		    model && model->item(i)->flags() & Qt::ItemIsEnabled) {
			m_channelCombo->setCurrentIndex(i);
			break;
		}
	}

	if (canvas) obs_canvas_release(canvas);
}

void AudioSourceDialog::setCurrentChannel(int channel)
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

void AudioSourceDialog::setConfig(const AsioSourceConfig &cfg)
{
	// Set source type
	for (int i = 0; i < m_typeCombo->count(); i++) {
		if (m_typeCombo->itemData(i).toString() == cfg.sourceType) {
			m_typeCombo->setCurrentIndex(i);
			break;
		}
	}
	// Set canvas and store for edit mode
	m_currentCanvas = cfg.canvas;
	for (int i = 0; i < m_canvasCombo->count(); i++) {
		if (m_canvasCombo->itemData(i).toString() == cfg.canvas) {
			m_canvasCombo->setCurrentIndex(i);
			break;
		}
	}
	// Set channel - because setText triggers validateInput which needs m_currentChannel
	setCurrentChannel(cfg.outputChannel);
	m_nameEdit->setText(cfg.name);
	
	// Set audio mixers (tracks)
	setAudioMixers(cfg.audioMixers);
}

QString AudioSourceDialog::getName() const
{
	return m_nameEdit->text().trimmed();
}

QString AudioSourceDialog::getSourceType() const
{
	return m_typeCombo->currentData().toString();
}

QString AudioSourceDialog::getCanvas() const
{
	return m_canvasCombo->currentData().toString();
}

int AudioSourceDialog::getChannel() const
{
	return m_channelCombo->currentData().toInt();
}

bool AudioSourceDialog::shouldOpenProperties() const
{
	return m_openPropertiesCheck->isChecked();
}

void AudioSourceDialog::setOpenProperties(bool open)
{
	m_openPropertiesCheck->setChecked(open);
}

bool AudioSourceDialog::shouldStartMuted() const
{
	return m_mutedCheck->isChecked();
}

void AudioSourceDialog::setStartMuted(bool muted)
{
	m_mutedCheck->setChecked(muted);
}

void AudioSourceDialog::validateInput()
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
		const auto &sources = AudioChSrcConfig::get()->getSources();
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
	
	// Check channel is not occupied by another source on this canvas
	if (valid && channel > 0) {
		QString selectedCanvasUuid = m_canvasCombo->currentData().toString();
		obs_canvas_t *canvas = selectedCanvasUuid.isEmpty() ? obs_get_main_canvas()
			: obs_get_canvas_by_uuid(selectedCanvasUuid.toUtf8().constData());
		if (!canvas) canvas = obs_get_main_canvas();
		
		// Query OBS directly for channel occupancy
		obs_source_t *existingSource = obs_canvas_get_channel(canvas, channel - 1); // 0-indexed
		bool isOccupied = (existingSource != nullptr);
		if (existingSource) obs_source_release(existingSource);
		
		// In edit mode, current channel on current canvas is always valid (it's our own)
		if (m_mode == EditMode && channel == m_currentChannel && selectedCanvasUuid == m_currentCanvas) {
			isOccupied = false;
		}
		if (isOccupied) {
			valid = false;
			error = obs_module_text("AsioSettings.ErrorChannelInUse");
		}
		if (canvas) obs_canvas_release(canvas);
	}
	
	m_okButton->setEnabled(valid);
	if (!valid && !error.isEmpty()) {
		m_errorLabel->setText(error);
		m_errorLabel->show();
	} else {
		m_errorLabel->hide();
	}
	
	// Show warning for reserved OBS channels (1-6)
	// These can be overridden by OBS built-in sources like Desktop Audio, Mic/Aux, etc.
	if (channel >= 1 && channel <= 6) {
		m_reservedWarningLabel->show();
	} else {
		m_reservedWarningLabel->hide();
	}
}

uint32_t AudioSourceDialog::getAudioMixers() const
{
	uint32_t mixers = 0;
	for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
		if (m_trackChecks[i]->isChecked()) {
			mixers |= (1 << i);
		}
	}
	return mixers;
}

void AudioSourceDialog::setAudioMixers(uint32_t mixers)
{
	for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
		m_trackChecks[i]->setChecked((mixers & (1 << i)) != 0);
	}
}
