#include "mixer_channel.h"
#include <plugin-support.h>

#include <QFrame>

MixerChannel::MixerChannel(obs_source_t *source, QWidget *parent)
	: QWidget(parent), m_source(nullptr)
{
	setupUi();
	setSource(source);
}

MixerChannel::~MixerChannel()
{
	disconnectSource();
}

void MixerChannel::setupUi()
{
	setFixedWidth(70);
	setMinimumHeight(200);
	
	m_layout = new QVBoxLayout(this);
	m_layout->setContentsMargins(4, 4, 4, 4);
	m_layout->setSpacing(4);

	// Level meter (placeholder - just a colored bar for now)
	m_levelMeter = new QLabel(this);
	m_levelMeter->setFixedSize(20, 100);
	m_levelMeter->setStyleSheet("background: qlineargradient(y1:0, y2:1, stop:0 #00ff00, stop:0.7 #ffff00, stop:1 #ff0000); border: 1px solid #333;");
	
	// Volume slider (vertical)
	m_volumeSlider = new QSlider(Qt::Vertical, this);
	m_volumeSlider->setRange(0, 100);
	m_volumeSlider->setValue(100);
	m_volumeSlider->setToolTip("Volume");
	m_volumeSlider->setMinimumHeight(100);
	
	// Horizontal layout for meter + fader
	auto *faderLayout = new QHBoxLayout();
	faderLayout->setSpacing(2);
	faderLayout->addWidget(m_levelMeter);
	faderLayout->addWidget(m_volumeSlider);
	m_layout->addLayout(faderLayout, 1);

	// Balance/Pan slider (horizontal)
	m_balanceSlider = new QSlider(Qt::Horizontal, this);
	m_balanceSlider->setRange(0, 100);
	m_balanceSlider->setValue(50); // Center
	m_balanceSlider->setToolTip("Pan (L/R)");
	m_balanceSlider->setFixedHeight(20);
	m_layout->addWidget(m_balanceSlider);
	
	// Mute button
	m_muteBtn = new QPushButton("M", this);
	m_muteBtn->setCheckable(true);
	m_muteBtn->setFixedSize(30, 24);
	m_muteBtn->setToolTip("Mute");
	m_muteBtn->setStyleSheet(
		"QPushButton { background: #444; border: 1px solid #666; border-radius: 3px; }"
		"QPushButton:checked { background: #cc4444; color: white; }"
	);
	
	auto *btnLayout = new QHBoxLayout();
	btnLayout->addStretch();
	btnLayout->addWidget(m_muteBtn);
	btnLayout->addStretch();
	m_layout->addLayout(btnLayout);
	
	// Name label
	m_nameLabel = new QLabel("---", this);
	m_nameLabel->setAlignment(Qt::AlignCenter);
	m_nameLabel->setStyleSheet("font-size: 10px; color: #ccc;");
	m_nameLabel->setWordWrap(true);
	m_nameLabel->setMaximumHeight(30);
	m_layout->addWidget(m_nameLabel);
	
	// Connections
	connect(m_volumeSlider, &QSlider::valueChanged, this, &MixerChannel::onVolumeSliderChanged);
	connect(m_balanceSlider, &QSlider::valueChanged, this, &MixerChannel::onBalanceSliderChanged);
	connect(m_muteBtn, &QPushButton::clicked, this, &MixerChannel::onMuteClicked);
	
	// Style the channel strip
	setStyleSheet("MixerChannel { background: #2a2a2a; border: 1px solid #444; border-radius: 4px; }");
}

void MixerChannel::setSource(obs_source_t *source)
{
	if (m_source == source) return;
	
	disconnectSource();
	m_source = source;
	
	if (m_source) {
		// Get source name
		const char *name = obs_source_get_name(m_source);
		m_nameLabel->setText(name ? QString::fromUtf8(name) : "---");
		
		// Get current values
		float volume = obs_source_get_volume(m_source);
		bool muted = obs_source_muted(m_source);
		float balance = obs_source_get_balance_value(m_source);
		
		m_updatingFromSource = true;
		m_volumeSlider->setValue((int)(volume * 100));
		m_balanceSlider->setValue((int)(balance * 100));
		m_muteBtn->setChecked(muted);
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
	// Signal connections would go here if we add source signal handling
}

void MixerChannel::disconnectSource()
{
	// Disconnect signals
}

void MixerChannel::onVolumeSliderChanged(int value)
{
	if (m_updatingFromSource || !m_source) return;
	
	float volume = value / 100.0f;
	obs_source_set_volume(m_source, volume);
	emit volumeChanged(volume);
}

void MixerChannel::onBalanceSliderChanged(int value)
{
	if (m_updatingFromSource || !m_source) return;
	
	float balance = value / 100.0f;
	obs_source_set_balance_value(m_source, balance);
	emit balanceChanged(balance);
}

void MixerChannel::onMuteClicked()
{
	if (!m_source) return;
	
	bool muted = m_muteBtn->isChecked();
	obs_source_set_muted(m_source, muted);
	emit muteChanged(muted);
}

void MixerChannel::updateVolume(float volume)
{
	m_updatingFromSource = true;
	m_volumeSlider->setValue((int)(volume * 100));
	m_updatingFromSource = false;
}

void MixerChannel::updateMute(bool muted)
{
	m_updatingFromSource = true;
	m_muteBtn->setChecked(muted);
	m_updatingFromSource = false;
}

void MixerChannel::updateBalance(float balance)
{
	m_updatingFromSource = true;
	m_balanceSlider->setValue((int)(balance * 100));
	m_updatingFromSource = false;
}

void MixerChannel::updateLevelMeter(float level)
{
	// For now just update the meter bar height/color based on level
	// A proper implementation would use a custom painted widget
	Q_UNUSED(level);
}
