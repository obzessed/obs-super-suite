#include "s_mixer_channel.hpp"

// Components
#include "super/ui/components/s_mixer_meter.hpp"
#include "super/ui/components/s_mixer_fader.hpp"
#include "super/ui/components/s_mixer_pan_slider.hpp"
#include "super/ui/components/s_mixer_name_bar.hpp"
#include "super/ui/components/s_mixer_control_bar.hpp"
#include "super/ui/components/s_mixer_props_selector.hpp"
#include "super/ui/components/s_mixer_db_label.hpp"
#include "super/ui/components/s_mixer_side_panel.hpp"
#include "super/ui/components/s_mixer_sidebar_toggle.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTimer>
#include <QEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QColorDialog>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <cmath>
#include <algorithm>

#include "super/ui/components/s_mixer_effects_rack.hpp"

namespace super {

// =====================================================================
// Construction / Destruction
// =====================================================================

SMixerChannel::SMixerChannel(QWidget *parent) : QWidget(parent)
{
	setupUi();
	startMeterTimer();
}

SMixerChannel::~SMixerChannel()
{
	blog(LOG_INFO, "[SMixerChannel] ~SMixerChannel() start (source='%s')",
	     m_source ? obs_source_get_name(m_source) : "(null)");
	disconnectSource();
	blog(LOG_INFO, "[SMixerChannel] ~SMixerChannel() done");
}

// =====================================================================
// UI Setup — Assemble components into the channel strip layout
// =====================================================================

void SMixerChannel::setupUi()
{
	setFixedWidth(STRIP_WIDTH);
	setMinimumHeight(400);
	setAttribute(Qt::WA_StyledBackground, true);
	setObjectName("SMixerChannel");
	setStyleSheet("#SMixerChannel { background: #232323; border-radius: 6px; }");

	auto *main_layout = new QHBoxLayout(this);
	main_layout->setContentsMargins(0, 0, 0, 0);
	main_layout->setSpacing(0);

	// ── Main Strip (left) ──────────────────────────────────────────────
	auto *strip = new QWidget(this);
	strip->setFixedWidth(STRIP_WIDTH);

	auto *root = new QVBoxLayout(strip);
	root->setContentsMargins(6, 6, 6, 6);
	root->setSpacing(6);

	// 1. Color strip + Name bar
	m_name_bar = new SMixerNameBar(strip);
	connect(m_name_bar, &SMixerNameBar::nameChanged, this, [this](const QString &name) {
		if (m_source)
			obs_source_set_name(m_source, name.toUtf8().constData());
	});
	root->addWidget(m_name_bar);

	// 2. Control bar (M / S / R)
	m_control_bar = new SMixerControlBar(strip);
	connect(m_control_bar, &SMixerControlBar::muteToggled, this, &SMixerChannel::onMuteToggled);
	connect(m_control_bar, &SMixerControlBar::soloToggled, this, [this](bool soloed) {
		emit soloChanged(soloed);
	});
	root->addWidget(m_control_bar);

	// 3. Props selector
	m_props_selector = new SMixerPropsSelector(strip);
	root->addWidget(m_props_selector);

	// 6. Pan slider
	m_pan_slider = new SMixerPanSlider(strip);
	m_pan_slider->setShowLabel(false);
	connect(m_pan_slider, &SMixerPanSlider::panChanged, this, &SMixerChannel::onPanChanged);
	root->addWidget(m_pan_slider);

	// 5. dB value label + Peak Meter Label
	auto *labels_layout = new QHBoxLayout();
	labels_layout->setSpacing(4);
	labels_layout->setContentsMargins(0, 0, 0, 0);

	m_peak_label = new QLabel("-inf", strip);
	m_peak_label->setObjectName("peakLabel");
	// Initial style - will be updated by timer
	m_peak_label->setStyleSheet(
		"color: #555; font-size: 10px; font-weight: bold;"
		"background: #2b2b2b; border-radius: 2px;"
		"font-family: 'Segoe UI', sans-serif;"
		"border: 1px solid #333;"
	);
	m_peak_label->setAlignment(Qt::AlignCenter);
	m_peak_label->setFixedHeight(18);
	m_peak_label->setCursor(Qt::PointingHandCursor);
	m_peak_label->installEventFilter(this);
	m_peak_label->setToolTip("Click to reset peak hold");
	labels_layout->addWidget(m_peak_label);

	m_db_label = new SMixerDbLabel(strip);
	connect(m_db_label, &SMixerDbLabel::resetRequested, this, &SMixerChannel::onDbResetRequested);
	labels_layout->addWidget(m_db_label);

	root->addLayout(labels_layout);

	// 4. Fader section: [dB Labels + Meter L&R] [Fader + fader Labels]
	auto *fader_area = new QHBoxLayout();
	fader_area->setSpacing(4);
	fader_area->setContentsMargins(0, 4, 0, 4);

	m_meter = new SMixerStereoMeter(strip);
	fader_area->addWidget(m_meter);

	m_fader = new SMixerFader(strip);
	connect(m_fader, &SMixerFader::volumeChanged, this, &SMixerChannel::onFaderChanged);
	fader_area->addWidget(m_fader);

	root->addLayout(fader_area, 1); // Stretch

	// 7. Expand button
	auto *expand_lyt = new QHBoxLayout();
	expand_lyt->setContentsMargins(0, 0, 0, 0);
	m_expand_btn = new SMixerSidebarToggle(strip);
	connect(m_expand_btn, &QPushButton::clicked, this, &SMixerChannel::toggleExpand);
	expand_lyt->addStretch();
	expand_lyt->addWidget(m_expand_btn);
	root->addLayout(expand_lyt);

	main_layout->addWidget(strip);

	// Separator (right, hidden by default)
	m_side_panel_sep = new QWidget(this);
	m_side_panel_sep->setVisible(false);
	m_side_panel_sep->setFixedWidth(1);
	m_side_panel_sep->setStyleSheet("background: #333;");
	main_layout->addWidget(m_side_panel_sep);

	// ── Side Panel (right, hidden by default) ────
	m_side_panel = new SMixerSidePanel(this);
	m_side_panel->setVisible(false);
	main_layout->addWidget(m_side_panel);
}

// =====================================================================
// Source Binding
// =====================================================================

void SMixerChannel::setSource(obs_source_t *source)
{
	if (m_source == source)
		return;

	disconnectSource();
	
	if (m_weak_source) {
		obs_weak_source_release(m_weak_source);
		m_weak_source = nullptr;
	}

	m_source = source;

	if (m_source) {
		m_weak_source = obs_source_get_weak_source(m_source);

		const char *name = obs_source_get_name(m_source);
		m_name_bar->setName(name ? QString::fromUtf8(name) : "Channel");

		// Sync volume
		m_updating_from_source = true;
		float vol = obs_source_get_volume(m_source);
		m_fader->setVolume(vol);
		updateDbLabel();

		// Sync pan
		float bal = obs_source_get_balance_value(m_source);
		m_pan_slider->setBalance(bal);

		// Sync mute
		bool muted = obs_source_muted(m_source);
		m_control_bar->setMuted(muted);
		if (m_meter) m_meter->setMuted(muted);

		// Sync mono
		if (m_meter) {
			speaker_layout layout = obs_source_get_speaker_layout(m_source);
			m_meter->setMono(layout == SPEAKERS_MONO);
		}

		m_updating_from_source = false;

		// Bind sub-components
		m_props_selector->setSource(m_source);
		m_side_panel->setSource(m_source);

		connectSource();
	} else {
		m_name_bar->setName("---");
	}

	emit sourceChanged(m_source);
}

QString SMixerChannel::sourceName() const
{
	if (!m_source)
		return {};
	const char *n = obs_source_get_name(m_source);
	return n ? QString::fromUtf8(n) : QString();
}

// =====================================================================
// Expand / Collapse
// =====================================================================

void SMixerChannel::setExpanded(bool expanded)
{
	if (m_expanded == expanded)
		return;

	m_expanded = expanded;
	m_side_panel->setVisible(expanded);
	m_side_panel_sep->setVisible(expanded);
	m_expand_btn->setExpanded(expanded);

	int w = STRIP_WIDTH + (expanded ? SIDE_PANEL_WIDTH : 0);
	setFixedWidth(w);

	if (expanded)
		m_side_panel->refresh();

	emit channelExpanded(expanded);
}

void SMixerChannel::toggleExpand()
{
	setExpanded(!m_expanded);
}

// =====================================================================
// Slots — Component Events
// =====================================================================

void SMixerChannel::onFaderChanged(float volume)
{
	if (m_updating_from_source || !m_source)
		return;

	obs_source_set_volume(m_source, volume);
	updateDbLabel();
	emit volumeChanged(volume);
}

void SMixerChannel::onMuteToggled(bool muted)
{
	if (!m_source)
		return;
	obs_source_set_muted(m_source, muted);
	emit muteChanged(muted);
}

void SMixerChannel::onPanChanged(int pan)
{
	if (m_updating_from_source || !m_source)
		return;
	float bal = (static_cast<float>(pan) + 100.0f) / 200.0f;
	obs_source_set_balance_value(m_source, bal);
	emit panChanged(pan);
}

void SMixerChannel::onDbResetRequested()
{
	if (!m_source)
		return;
	// Reset to 0 dB (unity gain)
	obs_source_set_volume(m_source, 1.0f);
	m_updating_from_source = true;
	m_fader->setVolume(1.0f);
	updateDbLabel();
	m_updating_from_source = false;
}

void SMixerChannel::updateDbLabel()
{
	if (!m_source)
		return;
	float vol = obs_source_get_volume(m_source);
	float db = 20.0f * log10f(fmaxf(vol, 0.0001f));
	m_db_label->setDb(db);
}

// =====================================================================
// Meter Timer
// =====================================================================

void SMixerChannel::startMeterTimer()
{
	auto *timer = new QTimer(this);
	timer->setTimerType(Qt::PreciseTimer);
	connect(timer, &QTimer::timeout, this, [this]() {
		float cur_peak_l, cur_peak_r, cur_mag_l, cur_mag_r;
		{
			QMutexLocker lock(&m_meter_mutex);
			cur_peak_l = m_peak_l;
			cur_peak_r = m_peak_r;
			cur_mag_l = m_mag_l;
			cur_mag_r = m_mag_r;

			// Reset for next interval
			m_peak_l = -60.0f;
			m_peak_r = -60.0f;
			m_mag_l = -60.0f;
			m_mag_r = -60.0f;
		}

		// Smooth decay
		const float decay = 0.8f;
		m_disp_peak_l = std::max(cur_peak_l > m_disp_peak_l ? cur_peak_l : m_disp_peak_l - decay, -60.0f);
		m_disp_peak_r = std::max(cur_peak_r > m_disp_peak_r ? cur_peak_r : m_disp_peak_r - decay, -60.0f);
		m_disp_mag_l = std::max(cur_mag_l > m_disp_mag_l ? cur_mag_l : m_disp_mag_l - decay, -60.0f);
		m_disp_mag_r = std::max(cur_mag_r > m_disp_mag_r ? cur_mag_r : m_disp_mag_r - decay, -60.0f);

		m_meter->setLevels(m_disp_peak_l, m_disp_mag_l, m_disp_peak_r, m_disp_mag_r);

		// Update Peak Label (Peak Hold)
		// Use instantaneous peak from this interval (cur_peak_l) to track accurate max
		float interval_peak = std::max(cur_peak_l, cur_peak_r);
		
		if (interval_peak > m_max_peak_hold)
			m_max_peak_hold = interval_peak;
			
		// Display the held peak
		float display_val = m_max_peak_hold;
		
		QString text;
		QColor color;
		
		if (display_val <= -60.0f) {
			text = "-inf";
			color = QColor("#555555");
		} else {
			if (display_val > -0.05f && display_val < 0.0f) display_val = 0.0f;
			text = QString::number(display_val, 'f', 1);
			if (display_val > -0.5f) color = QColor("#ff4444");      // Clip
			else if (display_val > -5.0f) color = QColor("#ffaa00"); // Warning
			else color = QColor("#00ff00");                          // Signal
		}
		
		if (m_peak_label) {
			m_peak_label->setText(text);
			m_peak_label->setStyleSheet(QString(
				"color: %1; font-size: 10px; font-weight: bold;"
				"background: #2b2b2b; border-radius: 2px;"
				"font-family: 'Segoe UI', sans-serif;"
				"border: 1px solid #333;"
			).arg(color.name()));
		}
	});
	timer->start(33); // ~30 fps
}

// =====================================================================
// OBS Source Connection
// =====================================================================

void SMixerChannel::connectSource()
{
	obs_source_t *source = getSource();
	if (!source)
		return;

	signal_handler_t *sh = obs_source_get_signal_handler(source);
	if (sh) {
		signal_handler_connect(sh, "volume", obsVolumeChangedCb, this);
		signal_handler_connect(sh, "mute", obsMuteChangedCb, this);
		signal_handler_connect(sh, "rename", obsRenamedCb, this);
		signal_handler_connect(sh, "filter_add", obsFilterAddedCb, this);
		signal_handler_connect(sh, "filter_remove", obsFilterRemovedCb, this);
		signal_handler_connect(sh, "reorder_filters", obsFilterAddedCb, this);
		signal_handler_connect(sh, "destroy", obsDestroyedCb, this);
	}

	m_volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_attach_source(m_volmeter, source);
	obs_volmeter_add_callback(m_volmeter, obsVolmeterCb, this);

	obs_source_release(source);
}

void SMixerChannel::disconnectSource()
{
	blog(LOG_INFO, "[SMixerChannel] disconnectSource() start (source='%s' m_volmeter=%p)",
	     m_source ? obs_source_get_name(m_source) : "(null)", m_volmeter);

	// Detach volmeter FIRST — this stops the audio thread from calling
	// our obsVolmeterCb, which would race with destruction.
	if (m_volmeter) {
		blog(LOG_INFO, "[SMixerChannel]   removing volmeter callback...");
		obs_volmeter_remove_callback(m_volmeter, obsVolmeterCb, this);
		blog(LOG_INFO, "[SMixerChannel]   detaching volmeter from source...");
		obs_volmeter_detach_source(m_volmeter);
		blog(LOG_INFO, "[SMixerChannel]   destroying volmeter...");
		obs_volmeter_destroy(m_volmeter);
		m_volmeter = nullptr;
		blog(LOG_INFO, "[SMixerChannel]   volmeter cleanup done");
	}

	// Tell sub-components to release their source references BEFORE
	// we null m_source. They need the source to still be valid so they
	// can disconnect their own signal handlers.
	if (m_side_panel) {
		blog(LOG_INFO, "[SMixerChannel]   clearing side_panel source...");
		m_side_panel->setSource(nullptr);
	}
	if (m_props_selector) {
		blog(LOG_INFO, "[SMixerChannel]   clearing props_selector source...");
		m_props_selector->setSource(nullptr);
	}

	// Disconnect signal handlers.
	if (m_source) {
		blog(LOG_INFO, "[SMixerChannel]   disconnecting signal handlers from '%s'...",
		     obs_source_get_name(m_source));
		signal_handler_t *sh = obs_source_get_signal_handler(m_source);
		if (sh) {
			signal_handler_disconnect(sh, "volume", obsVolumeChangedCb, this);
			signal_handler_disconnect(sh, "mute", obsMuteChangedCb, this);
			signal_handler_disconnect(sh, "rename", obsRenamedCb, this);
			signal_handler_disconnect(sh, "filter_add", obsFilterAddedCb, this);
			signal_handler_disconnect(sh, "filter_remove", obsFilterRemovedCb, this);
			signal_handler_disconnect(sh, "reorder_filters", obsFilterAddedCb, this);
			signal_handler_disconnect(sh, "destroy", obsDestroyedCb, this);
		}
		blog(LOG_INFO, "[SMixerChannel]   signal handlers disconnected");
	}

	if (m_weak_source) {
		blog(LOG_INFO, "[SMixerChannel]   releasing weak source ref...");
		obs_weak_source_release(m_weak_source);
		m_weak_source = nullptr;
	}
	m_source = nullptr;
	blog(LOG_INFO, "[SMixerChannel] disconnectSource() done");
}

// =====================================================================
// OBS Callbacks (called from audio / signal threads)
// =====================================================================

void SMixerChannel::obsVolmeterCb(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
                                    const float peak[MAX_AUDIO_CHANNELS],
                                    const float input_peak[MAX_AUDIO_CHANNELS])
{
	Q_UNUSED(input_peak);
	auto *self = static_cast<SMixerChannel *>(data);

	float pl = peak[0];
	float pr = (MAX_AUDIO_CHANNELS > 1) ? peak[1] : peak[0];
	float ml = magnitude[0];
	float mr = (MAX_AUDIO_CHANNELS > 1) ? magnitude[1] : magnitude[0];

	QMutexLocker lock(&self->m_meter_mutex);
	if (pl > self->m_peak_l) self->m_peak_l = pl;
	if (pr > self->m_peak_r) self->m_peak_r = pr;
	if (ml > self->m_mag_l) self->m_mag_l = ml;
	if (mr > self->m_mag_r) self->m_mag_r = mr;
}

void SMixerChannel::obsVolumeChangedCb(void *data, calldata_t *cd)
{
	auto *self = static_cast<SMixerChannel *>(data);
	double vol = calldata_float(cd, "volume");
	QMetaObject::invokeMethod(self, [self, vol]() {
		self->m_updating_from_source = true;
		self->m_fader->setVolume(static_cast<float>(vol));
		self->updateDbLabel();
		self->m_updating_from_source = false;
	});
}

void SMixerChannel::obsMuteChangedCb(void *data, calldata_t *cd)
{
	auto *self = static_cast<SMixerChannel *>(data);
	bool muted = calldata_bool(cd, "muted");
	QMetaObject::invokeMethod(self, [self, muted]() {
		self->m_control_bar->setMuted(muted);
		if (self->m_meter) self->m_meter->setMuted(muted);
	});
}

void SMixerChannel::obsRenamedCb(void *data, calldata_t *cd)
{
	auto *self = static_cast<SMixerChannel *>(data);
	const char *name = calldata_string(cd, "new_name");
	QString qname = name ? QString::fromUtf8(name) : "";
	QMetaObject::invokeMethod(self, [self, qname]() {
		self->m_name_bar->setName(qname);
	});
}

void SMixerChannel::obsFilterAddedCb(void *data, calldata_t *)
{
	auto *self = static_cast<SMixerChannel *>(data);
	QMetaObject::invokeMethod(self, [self]() {
		if (self->m_expanded)
			self->m_side_panel->refresh();
	});
}

void SMixerChannel::obsFilterRemovedCb(void *data, calldata_t *)
{
	auto *self = static_cast<SMixerChannel *>(data);
	QMetaObject::invokeMethod(self, [self]() {
		if (self->m_expanded)
			self->m_side_panel->refresh();
	});
}

void SMixerChannel::obsDestroyedCb(void *data, calldata_t *)
{
	auto *self = static_cast<SMixerChannel *>(data);

	blog(LOG_INFO, "[SMixerChannel] obsDestroyedCb() — source is being destroyed (source='%s')",
	     self->m_source ? obs_source_get_name(self->m_source) : "(null)");

	// SYNCHRONOUSLY detach the volmeter right here in the signal thread.
	if (self->m_volmeter) {
		blog(LOG_INFO, "[SMixerChannel]   sync volmeter detach in destroy callback...");
		obs_volmeter_remove_callback(self->m_volmeter, obsVolmeterCb, self);
		obs_volmeter_detach_source(self->m_volmeter);
		obs_volmeter_destroy(self->m_volmeter);
		self->m_volmeter = nullptr;
	}

	if (self->m_weak_source) {
		obs_weak_source_release(self->m_weak_source);
		self->m_weak_source = nullptr;
	}
	self->m_source = nullptr;

	blog(LOG_INFO, "[SMixerChannel]   obsDestroyedCb() done, deferring UI update");

	// Defer UI updates to the Qt event loop
	QMetaObject::invokeMethod(self, [self]() {
		self->m_name_bar->setName("---");
		self->m_side_panel->setSource(nullptr);
		self->m_props_selector->setSource(nullptr);
	});
}

bool SMixerChannel::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == m_peak_label && event->type() == QEvent::MouseButtonRelease) {
		m_max_peak_hold = -60.0f;
		if (m_peak_label) {
			m_peak_label->setText("-inf");
			m_peak_label->setStyleSheet(
				"color: #555555; font-size: 10px; font-weight: bold;"
				"background: #2b2b2b; border-radius: 2px;"
				"font-family: 'Segoe UI', sans-serif;"
				"border: 1px solid #333;"
			);
		}
		return true;
	}
	return QWidget::eventFilter(obj, event);
}

// =====================================================================
// Channel Context Menu
// =====================================================================

static const char *kChannelMenuStyle =
	"QMenu {"
	"  background: #2a2a2a; border: 1px solid #444;"
	"  color: #ddd; font-size: 11px;"
	"  font-family: 'Segoe UI', sans-serif;"
	"  padding: 4px 0px;"
	"  border-radius: 4px;"
	"}"
	"QMenu::item {"
	"  padding: 5px 20px 5px 12px;"
	"}"
	"QMenu::item:selected {"
	"  background: #00e5ff; color: #111;"
	"}"
	"QMenu::item:disabled {"
	"  color: #666;"
	"}"
	"QMenu::separator {"
	"  height: 1px; background: #444; margin: 4px 8px;"
	"}";

void SMixerChannel::contextMenuEvent(QContextMenuEvent *event)
{
	showChannelContextMenu(event->globalPos());
}

void SMixerChannel::showChannelContextMenu(const QPoint &globalPos)
{
	if (!m_source) return;

	QMenu menu(this);
	menu.setStyleSheet(kChannelMenuStyle);

	// Rename
	auto *renameAct = menu.addAction("Rename");
	connect(renameAct, &QAction::triggered, this, [this]() {
		if (m_name_bar) m_name_bar->startEditing();
	});

	// Color
	auto *colorAct = menu.addAction("Color...");
	connect(colorAct, &QAction::triggered, this, &SMixerChannel::showColorPicker);

	menu.addSeparator();

	// Fader Lock
	auto *faderLockAct = menu.addAction("Fader Lock");
	faderLockAct->setCheckable(true);
	faderLockAct->setChecked(m_fader_locked);
	connect(faderLockAct, &QAction::triggered, this, [this](bool checked) {
		m_fader_locked = checked;
		if (m_fader) m_fader->setEnabled(!checked);
	});

	// Mono
	auto *monoAct = menu.addAction("Mono");
	monoAct->setCheckable(true);
	monoAct->setChecked(m_mono);
	connect(monoAct, &QAction::triggered, this, [this](bool checked) {
		m_mono = checked;
		uint32_t flags = obs_source_get_flags(m_source);
		if (checked)
			flags |= OBS_SOURCE_FLAG_FORCE_MONO;
		else
			flags &= ~OBS_SOURCE_FLAG_FORCE_MONO;
		obs_source_set_flags(m_source, flags);
	});

	menu.addSeparator();

	// Copy Filter(s)
	auto *copyAct = menu.addAction("Copy Filter(s)");
	connect(copyAct, &QAction::triggered, this, [this]() {
		SMixerEffectsRack::copyAllFilters(m_source);
	});

	// Paste Filter(s)
	auto *pasteAct = menu.addAction("Paste Filter(s)");
	pasteAct->setEnabled(SMixerEffectsRack::hasClipboardFilters());
	connect(pasteAct, &QAction::triggered, this, [this]() {
		SMixerEffectsRack::pasteFilters(m_source);
		if (m_expanded) m_side_panel->refresh();
	});

	menu.addSeparator();

	// Properties
	auto *propsAct = menu.addAction("Properties");
	connect(propsAct, &QAction::triggered, this, [this]() {
		obs_frontend_open_source_properties(m_source);
	});

	menu.exec(globalPos);
}

void SMixerChannel::showColorPicker()
{
	QColor current = m_name_bar ? m_name_bar->accentColor() : QColor(0x00, 0xFA, 0x9A);
	QColor color = QColorDialog::getColor(current, this, "Channel Color");
	if (color.isValid() && m_name_bar) {
		m_name_bar->setAccentColor(color);
	}
}

} // namespace super
