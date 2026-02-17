#include "s_mixer_sends_panel.hpp"

#include <QHBoxLayout>
#include <QPushButton>
#include <QEvent>
#include <QMouseEvent>
#include <algorithm>

namespace super {

// ============================================================================
// SMixerSendsPanel Implementation
// ============================================================================

SMixerSendsPanel::SMixerSendsPanel(QWidget *parent) : QWidget(parent)
{
	setupUi();
}

SMixerSendsPanel::~SMixerSendsPanel()
{
	disconnectSource();
}

void SMixerSendsPanel::setupUi()
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 8, 0, 8); // Top/Bottom padding for the whole panel
	layout->setSpacing(8);

	// Header Container
	auto *headerWidget = new QWidget(this);
	headerWidget->setObjectName("headerRow");
	headerWidget->setStyleSheet("#headerRow { border-bottom: 1px solid #333; padding-bottom: 4px; }");
	
	auto *header = new QHBoxLayout(headerWidget);
	header->setContentsMargins(0, 0, 0, 0);
	header->setSpacing(0);

	// Collapse Button (Left)
	m_collapse_btn = new QPushButton("v", headerWidget);
	m_collapse_btn->setFixedSize(28, 16);
	m_collapse_btn->setCursor(Qt::PointingHandCursor);
	m_collapse_btn->setToolTip("Collapse");
	m_collapse_btn->setObjectName("collapseBtn");
	m_collapse_btn->setStyleSheet(
		"#collapseBtn { border: none; background: transparent; color: #888; font-weight: bold; font-family: 'Segoe UI', sans-serif; }"
		"#collapseBtn:hover { color: #fff; }"
	);
	connect(m_collapse_btn, &QPushButton::clicked, this, [this]() {
		setExpanded(!m_is_expanded);
	});
	header->addWidget(m_collapse_btn);

	header->addStretch();

	// Title
	m_header_label = new QLabel("SENDS", headerWidget);
	m_header_label->setAlignment(Qt::AlignCenter);
	m_header_label->setStyleSheet(
		"color: #888; font-weight: bold; font-size: 10px;"
		"font-family: 'Segoe UI', sans-serif;"
		"letter-spacing: 1px;"
		"border: none;"
	);
	header->addWidget(m_header_label);

	header->addStretch();

	// Balancer for visual centering
	auto *spacer = new QWidget(headerWidget);
	spacer->setFixedSize(28, 16);
	header->addWidget(spacer);

	headerWidget->installEventFilter(this);

	layout->addWidget(headerWidget);

	// Content Container
	m_content_container = new QWidget(this);
	m_items_layout = new QVBoxLayout(m_content_container);
	m_items_layout->setContentsMargins(0, 0, 0, 0);
	m_items_layout->setSpacing(2);
	
	layout->addWidget(m_content_container);
}

void SMixerSendsPanel::setSource(obs_source_t *source)
{
	if (m_source == source)
		return;

	disconnectSource();
	m_source = source;
	connectSource();
	refresh();
}

void SMixerSendsPanel::connectSource()
{
	if (!m_source) return;

	m_signal_handler = obs_source_get_signal_handler(m_source);
	if (m_signal_handler) {
		signal_handler_connect(m_signal_handler, "audio_mixers", audioMixersChangedCb, this);
	}
}

void SMixerSendsPanel::disconnectSource()
{
	if (m_signal_handler) {
		signal_handler_disconnect(m_signal_handler, "audio_mixers", audioMixersChangedCb, this);
		m_signal_handler = nullptr;
	}
	m_source = nullptr;
}

void SMixerSendsPanel::audioMixersChangedCb(void *data, calldata_t *cd)
{
	auto *self = static_cast<SMixerSendsPanel *>(data);
	int mixers = static_cast<int>(calldata_int(cd, "mixers"));
	QMetaObject::invokeMethod(self, [self, mixers]() {
		self->updateSwitches(static_cast<uint32_t>(mixers));
	});
}

void SMixerSendsPanel::updateSwitches(uint32_t mixers)
{
	for (int i = 0; i < static_cast<int>(m_switches.size()); i++) {
		if (i >= m_track_count) break;
		
		bool active = (mixers & (1 << i)) != 0;
		auto *sw = m_switches[i];
		if (sw && sw->isChecked() != active) {
			sw->setChecked(active, true); // Animate external change
		}
	}
}

void SMixerSendsPanel::clearItems()
{
	m_switches.clear();
	QLayoutItem *item;
	while ((item = m_items_layout->takeAt(0)) != nullptr) {
		if (item->widget())
			delete item->widget();
		delete item;
	}
}

void SMixerSendsPanel::setTrackCount(int count)
{
	m_track_count = std::clamp(count, 1, 64);
	refresh();
}

void SMixerSendsPanel::refresh()
{
	clearItems();

	if (!m_source) {
		auto *lbl = new QLabel("No Source", this);
		lbl->setAlignment(Qt::AlignCenter);
		lbl->setStyleSheet("color: #555; font-style: italic; font-size: 10px; padding: 10px;");
		m_items_layout->addWidget(lbl);
		return;
	}

	uint32_t mixers = obs_source_get_audio_mixers(m_source);

	for (int i = 0; i < m_track_count; i++) {
		int track_num = i + 1;
		
		auto *row = new QWidget(this);
		auto *rowLayout = new QHBoxLayout(row);
		rowLayout->setContentsMargins(12, 4, 12, 4); // Clean padding: 12px sides, 4px vertical
		rowLayout->setSpacing(0);

		// Label (Left)
		QLabel *lbl = new QLabel(QString("Track %1").arg(track_num), row);
		lbl->setStyleSheet("color: #aaa; font-size: 11px; font-family: 'Segoe UI', sans-serif;");
		rowLayout->addWidget(lbl);

		rowLayout->addStretch();

		// Switch (Right)
		auto *sw = new SMixerSwitch(row);
		m_switches.push_back(sw);
		
		// Set initial state (no animation since not visible yet)
		bool state = (mixers & (1 << i)) != 0;
		sw->setChecked(state, false); 
		
		// Connect toggle -> OBS
		connect(sw, &SMixerSwitch::toggled, this, [this, i](bool checked) {
			if (!m_source)
				return;
			uint32_t current = obs_source_get_audio_mixers(m_source);
			uint32_t mask = (1 << i);
			
			// Only update if changed (to double check)
			bool currentState = (current & mask) != 0;
			if (currentState != checked) {
				if (checked) current |= mask;
				else current &= ~mask;
				obs_source_set_audio_mixers(m_source, current);
				emit trackChanged(i, checked);
			}
		});

		rowLayout->addWidget(sw);

		m_items_layout->addWidget(row);
	}
}

void SMixerSendsPanel::setExpanded(bool expanded)
{
	m_is_expanded = expanded;
	if (m_content_container)
		m_content_container->setVisible(expanded);
		
	if (m_collapse_btn) {
		m_collapse_btn->setText(expanded ? "v" : ">");
		m_collapse_btn->setToolTip(expanded ? "Collapse" : "Expand");
	}
}

bool SMixerSendsPanel::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::MouseButtonDblClick) {
		setExpanded(!m_is_expanded);
		return true;
	}
	return QWidget::eventFilter(obj, event);
}

} // namespace super
