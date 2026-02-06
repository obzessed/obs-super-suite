#include "mixer_dock.h"
#include "mixer_channel.h"
#include "asio_config.h"
#include <plugin-support.h>

#include <QLabel>

MixerDock::MixerDock(QWidget *parent)
	: QWidget(parent)
{
	setWindowTitle(obs_module_text("SuperMixer.Title"));
	setObjectName("SuperMixerDock");
	
	setupUi();
}

MixerDock::~MixerDock()
{
	clearChannels();
}

void MixerDock::setupUi()
{
	// Main container
	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(4, 4, 4, 4);
	mainLayout->setSpacing(4);
	
	// Scroll area for channels
	m_scrollArea = new QScrollArea(this);
	m_scrollArea->setWidgetResizable(true);
	m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_scrollArea->setFrameShape(QFrame::NoFrame);
	
	// Channels container (inside scroll area)
	auto *channelsWidget = new QWidget();
	m_channelsLayout = new QHBoxLayout(channelsWidget);
	m_channelsLayout->setContentsMargins(0, 0, 0, 0);
	m_channelsLayout->setSpacing(4);
	m_channelsLayout->addStretch(); // Push channels to left
	
	m_scrollArea->setWidget(channelsWidget);
	mainLayout->addWidget(m_scrollArea);
	
	// Style
	this->setStyleSheet("background: #1e1e1e;");
}

void MixerDock::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	refresh();
}

void MixerDock::refresh()
{
	clearChannels();
	
	// Get sources from config
	auto &configs = AsioConfig::get()->getSources();
	
	for (const auto &cfg : configs) {
		if (!cfg.enabled) continue;
		if (cfg.sourceUuid.isEmpty()) continue;
		
		// Get source by UUID
		obs_source_t *source = obs_get_source_by_uuid(cfg.sourceUuid.toUtf8().constData());
		if (!source) continue;
		
		// Create channel strip
		auto *channel = new MixerChannel(source, this);
		m_channels.append(channel);
		
		// Insert before the stretch
		m_channelsLayout->insertWidget(m_channelsLayout->count() - 1, channel);
		
		obs_source_release(source); // Channel doesn't hold ref, just uses pointer
	}
	
	// Show placeholder if no channels
	if (m_channels.isEmpty()) {
		auto *placeholder = new QLabel(obs_module_text("SuperMixer.NoSources"), this);
		placeholder->setAlignment(Qt::AlignCenter);
		placeholder->setStyleSheet("color: #666; font-style: italic;");
		m_channelsLayout->insertWidget(0, placeholder);
	}
}

void MixerDock::clearChannels()
{
	for (auto *channel : m_channels) {
		m_channelsLayout->removeWidget(channel);
		delete channel;
	}
	m_channels.clear();
	
	// Remove placeholder if exists
	while (m_channelsLayout->count() > 1) {
		auto *item = m_channelsLayout->takeAt(0);
		if (item->widget()) {
			delete item->widget();
		}
		delete item;
	}
}

MixerChannel *MixerDock::findChannelByUuid(const QString &uuid)
{
	for (auto *channel : m_channels) {
		if (channel->getSourceUuid() == uuid) {
			return channel;
		}
	}
	return nullptr;
}

void MixerDock::updateSourceVolume(const QString &sourceUuid, float volume)
{
	if (auto *channel = findChannelByUuid(sourceUuid)) {
		channel->updateVolume(volume);
	}
}

void MixerDock::updateSourceMute(const QString &sourceUuid, bool muted)
{
	if (auto *channel = findChannelByUuid(sourceUuid)) {
		channel->updateMute(muted);
	}
}

void MixerDock::updateSourceBalance(const QString &sourceUuid, float balance)
{
	if (auto *channel = findChannelByUuid(sourceUuid)) {
		channel->updateBalance(balance);
	}
}
