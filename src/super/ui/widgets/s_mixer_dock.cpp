#include "s_mixer_dock.hpp"
#include "s_mixer_channel.hpp"

#include <QLabel>
#include <QScrollBar>
#include <QTimer>
#include <obs.hpp>
#include <obs-source.h>
#include <obs-frontend-api.h>

namespace super {

// =====================================================================
// Construction / Destruction
// =====================================================================

SMixerDock::SMixerDock(QWidget *parent) : QWidget(parent)
{
	setupUi();
	populateSources();

	// Auto-refresh when scene collection changes
	obs_frontend_add_event_callback(obsEventCallback, this);
}

SMixerDock::~SMixerDock()
{
	obs_frontend_remove_event_callback(obsEventCallback, this);
	clearChannels();
	m_combo_sources.clear();
	if (m_source_combo)
		m_source_combo->clear();
}

// =====================================================================
// UI Setup
// =====================================================================

void SMixerDock::setupUi()
{
	setWindowTitle("Super Mixer");
	setMinimumWidth(400);
	setMinimumHeight(300);
	setAttribute(Qt::WA_StyledBackground, true);
	setStyleSheet(
		"SMixerDock { background: #1a1a1a; }"
	);

	auto *main_layout = new QVBoxLayout(this);
	main_layout->setContentsMargins(4, 4, 4, 4);
	main_layout->setSpacing(4);

	// ── Toolbar ────────────────────────────────────────────────────────
	auto *toolbar = new QWidget(this);
	toolbar->setStyleSheet(
		"background: #222; border-radius: 4px; border: 1px solid #333;"
	);
	auto *toolbar_layout = new QHBoxLayout(toolbar);
	toolbar_layout->setContentsMargins(8, 4, 8, 4);
	toolbar_layout->setSpacing(6);

	// Group label
	auto *group_label = new QLabel("GROUPS", toolbar);
	group_label->setStyleSheet(
		"color: #888; font-weight: bold; font-size: 10px;"
		"font-family: 'Segoe UI', sans-serif;"
		"border: none; background: transparent;"
	);
	toolbar_layout->addWidget(group_label);

	// Source selector
	auto *source_label = new QLabel("Audio Source:", toolbar);
	source_label->setStyleSheet(
		"color: #aaa; font-size: 11px;"
		"font-family: 'Segoe UI', sans-serif;"
		"border: none; background: transparent;"
	);
	toolbar_layout->addWidget(source_label);

	m_source_combo = new QComboBox(toolbar);
	m_source_combo->setMinimumWidth(160);
	m_source_combo->setStyleSheet(
		"QComboBox {"
		"  background: #2b2b2b; color: #ddd;"
		"  border: 1px solid #444; border-radius: 3px;"
		"  padding: 2px 8px; font-size: 11px;"
		"  font-family: 'Segoe UI', sans-serif;"
		"}"
		"QComboBox::drop-down {"
		"  border: none; width: 20px;"
		"}"
		"QComboBox::down-arrow {"
		"  image: none; border-left: 4px solid transparent;"
		"  border-right: 4px solid transparent;"
		"  border-top: 5px solid #888; margin-right: 6px;"
		"}"
		"QComboBox QAbstractItemView {"
		"  background: #2b2b2b; color: #ddd;"
		"  border: 1px solid #444; selection-background-color: #00e5ff;"
		"  selection-color: #000;"
		"}"
	);
	toolbar_layout->addWidget(m_source_combo);

	// Add button
	m_add_btn = new QPushButton("+ Add", toolbar);
	m_add_btn->setStyleSheet(
		"QPushButton {"
		"  background: #00897b; color: #fff;"
		"  border: 1px solid #00695c; border-radius: 3px;"
		"  padding: 3px 12px; font-weight: bold; font-size: 11px;"
		"  font-family: 'Segoe UI', sans-serif;"
		"}"
		"QPushButton:hover { background: #009688; }"
		"QPushButton:pressed { background: #00695c; }"
	);
	connect(m_add_btn, &QPushButton::clicked, this, &SMixerDock::onAddChannelClicked);
	toolbar_layout->addWidget(m_add_btn);

	// Auto-populate button
	m_auto_btn = new QPushButton("Auto", toolbar);
	m_auto_btn->setToolTip("Auto-populate all audio sources");
	m_auto_btn->setStyleSheet(
		"QPushButton {"
		"  background: #2b2b2b; color: #aaa;"
		"  border: 1px solid #444; border-radius: 3px;"
		"  padding: 3px 10px; font-size: 11px;"
		"  font-family: 'Segoe UI', sans-serif;"
		"}"
		"QPushButton:hover { background: #333; color: #fff; }"
	);
	connect(m_auto_btn, &QPushButton::clicked, this, &SMixerDock::onAutoPopulateClicked);
	toolbar_layout->addWidget(m_auto_btn);

	// Refresh button
	m_refresh_btn = new QPushButton("Refresh", toolbar);
	m_refresh_btn->setStyleSheet(
		"QPushButton {"
		"  background: #2b2b2b; color: #aaa;"
		"  border: 1px solid #444; border-radius: 3px;"
		"  padding: 3px 10px; font-size: 11px;"
		"  font-family: 'Segoe UI', sans-serif;"
		"}"
		"QPushButton:hover { background: #333; color: #fff; }"
	);
	connect(m_refresh_btn, &QPushButton::clicked, this, &SMixerDock::onRefreshClicked);
	toolbar_layout->addWidget(m_refresh_btn);

	toolbar_layout->addStretch();
	main_layout->addWidget(toolbar);

	// ── Channels Scroll Area ───────────────────────────────────────────
	m_scroll_area = new QScrollArea(this);
	m_scroll_area->setWidgetResizable(true);
	m_scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_scroll_area->setStyleSheet(
		"QScrollArea { border: none; background: transparent; }"
		"QScrollBar:horizontal {"
		"  background: #1a1a1a; height: 8px; border: none;"
		"}"
		"QScrollBar::handle:horizontal {"
		"  background: #444; border-radius: 4px; min-width: 40px;"
		"}"
		"QScrollBar::handle:horizontal:hover { background: #555; }"
		"QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
		"  width: 0px;"
		"}"
		"QScrollBar:vertical {"
		"  background: #1a1a1a; width: 8px; border: none;"
		"}"
		"QScrollBar::handle:vertical {"
		"  background: #444; border-radius: 4px; min-height: 40px;"
		"}"
		"QScrollBar::handle:vertical:hover { background: #555; }"
		"QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
		"  height: 0px;"
		"}"
	);

	auto *channels_widget = new QWidget();
	channels_widget->setStyleSheet("background: transparent;");
	m_channels_layout = new QHBoxLayout(channels_widget);
	m_channels_layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	m_channels_layout->setSpacing(4);
	m_channels_layout->setContentsMargins(4, 4, 4, 4);

	m_scroll_area->setWidget(channels_widget);
	main_layout->addWidget(m_scroll_area, 1);
}

// =====================================================================
// Channel Management
// =====================================================================

SMixerChannel *SMixerDock::addChannel(obs_source_t *source)
{
	auto *channel = new SMixerChannel();
	channel->setSource(source);

	m_channels_layout->addWidget(channel);
	m_channels.push_back(channel);

	// Scroll to the new channel
	QTimer::singleShot(0, [this]() {
		m_scroll_area->horizontalScrollBar()->setValue(
			m_scroll_area->horizontalScrollBar()->maximum());
	});

	emit channelAdded(channel);
	return channel;
}

void SMixerDock::removeChannel(int index)
{
	if (index < 0 || index >= static_cast<int>(m_channels.size()))
		return;

	auto &channel = m_channels[index];
	if (channel) {
		m_channels_layout->removeWidget(channel);
		delete channel;
	}
	m_channels.erase(m_channels.begin() + index);

	emit channelRemoved(index);
}

void SMixerDock::clearChannels()
{
	for (auto &channel : m_channels) {
		if (channel) {
			m_channels_layout->removeWidget(channel);
			delete channel;
		}
	}
	m_channels.clear();
}

int SMixerDock::channelCount() const
{
	return static_cast<int>(m_channels.size());
}

SMixerChannel *SMixerDock::channelAt(int index) const
{
	if (index < 0 || index >= static_cast<int>(m_channels.size()))
		return nullptr;
	return m_channels[index];
}

// =====================================================================
// Source Population
// =====================================================================

void SMixerDock::populateSources()
{
	m_combo_sources.clear();
	m_source_combo->clear();
	m_source_combo->addItem("Select Source", QVariant());
	obs_enum_sources(enumAudioSourcesCb, this);
}

bool SMixerDock::enumAudioSourcesCb(void *param, obs_source_t *source)
{
	auto *dock = static_cast<SMixerDock *>(param);
	if (!source)
		return true;

	uint32_t flags = obs_source_get_output_flags(source);
	if ((flags & OBS_SOURCE_AUDIO) == 0) {
		return true;
	}

	// Skip if source is internal
	if (flags & OBS_SOURCE_CAP_DISABLED) {
		return true;
	}

	const char *name = obs_source_get_name(source);
	dock->m_combo_sources.push_back(OBSGetWeakRef(source));
	int vecIndex = static_cast<int>(dock->m_combo_sources.size()) - 1;
	dock->m_source_combo->addItem(name, QVariant::fromValue(vecIndex));

	return true;
}

void SMixerDock::autoPopulateAudioSources()
{
	clearChannels();

	obs_enum_sources([](void *param, obs_source_t *source) -> bool {
		if (!source)
			return true;

		uint32_t flags = obs_source_get_output_flags(source);
		if ((flags & OBS_SOURCE_AUDIO) == 0)
			return true;

		// Skip if source is internal
		if (flags & OBS_SOURCE_CAP_DISABLED) {
			return true;
		}

		auto *dock = static_cast<SMixerDock *>(param);
		dock->addChannel(source);
		return true;
	}, this);
}

// =====================================================================
// Slots
// =====================================================================

void SMixerDock::onAddChannelClicked()
{
	int index = m_source_combo->currentIndex();
	if (index <= 0)
		return;

	int vecIndex = m_source_combo->itemData(index).toInt();
	if (vecIndex < 0 || vecIndex >= static_cast<int>(m_combo_sources.size()))
		return;

	OBSSource source = OBSGetStrongRef(m_combo_sources[vecIndex]);
	if (!source)
		return;

	uint32_t flags = obs_source_get_output_flags(source);
	if ((flags & OBS_SOURCE_AUDIO) == 0) {
		return;
	}

	// Skip if source is internal
	if (flags & OBS_SOURCE_CAP_DISABLED) {
		return;
	}

	addChannel(source);
}

void SMixerDock::onRefreshClicked()
{
	populateSources();
}

void SMixerDock::onAutoPopulateClicked()
{
	autoPopulateAudioSources();
}

// =====================================================================
// OBS Event Callback
// =====================================================================

void SMixerDock::obsEventCallback(obs_frontend_event event, void *data)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING ||
	    event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
		auto *dock = static_cast<SMixerDock *>(data);
		QMetaObject::invokeMethod(dock, [dock]() {
			dock->populateSources();
		});
	}
}

} // namespace super
