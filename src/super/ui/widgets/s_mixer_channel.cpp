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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTimer>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <cmath>
#include <algorithm>

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
	disconnectSource();
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

	// 5. dB value label
	m_db_label = new SMixerDbLabel(strip);
	connect(m_db_label, &SMixerDbLabel::resetRequested, this, &SMixerChannel::onDbResetRequested);
	root->addWidget(m_db_label);

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
	m_expand_btn = new QPushButton(">", strip);
	m_expand_btn->setFixedHeight(14);
	m_expand_btn->setFlat(true);
	m_expand_btn->setStyleSheet(
		"QPushButton {"
		"  color: #666; font-size: 10px; border: none; font-weight: bold;"
		"  font-family: 'Segoe UI', sans-serif;"
		"}"
		"QPushButton:hover { color: #00e5ff; }"
	);
	connect(m_expand_btn, &QPushButton::clicked, this, &SMixerChannel::toggleExpand);
	root->addWidget(m_expand_btn);

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
	m_source = source;

	if (m_source) {
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
	m_expand_btn->setText(expanded ? "<" : ">");

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
	});
	timer->start(33); // ~30 fps
}

// =====================================================================
// OBS Source Connection
// =====================================================================

void SMixerChannel::connectSource()
{
	if (!m_source)
		return;

	signal_handler_t *sh = obs_source_get_signal_handler(m_source);
	if (sh) {
		signal_handler_connect(sh, "volume", obsVolumeChangedCb, this);
		signal_handler_connect(sh, "mute", obsMuteChangedCb, this);
		signal_handler_connect(sh, "rename", obsRenamedCb, this);
		signal_handler_connect(sh, "filter_add", obsFilterAddedCb, this);
		signal_handler_connect(sh, "filter_remove", obsFilterRemovedCb, this);
		signal_handler_connect(sh, "reorder_filters", obsFilterAddedCb, this);
	}

	m_volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_attach_source(m_volmeter, m_source);
	obs_volmeter_add_callback(m_volmeter, obsVolmeterCb, this);
}

void SMixerChannel::disconnectSource()
{
	if (m_volmeter) {
		obs_volmeter_remove_callback(m_volmeter, obsVolmeterCb, this);
		obs_volmeter_detach_source(m_volmeter);
		obs_volmeter_destroy(m_volmeter);
		m_volmeter = nullptr;
	}

	if (m_source) {
		signal_handler_t *sh = obs_source_get_signal_handler(m_source);
		if (sh) {
			signal_handler_disconnect(sh, "volume", obsVolumeChangedCb, this);
			signal_handler_disconnect(sh, "mute", obsMuteChangedCb, this);
			signal_handler_disconnect(sh, "rename", obsRenamedCb, this);
			signal_handler_disconnect(sh, "filter_add", obsFilterAddedCb, this);
			signal_handler_disconnect(sh, "filter_remove", obsFilterRemovedCb, this);
			signal_handler_disconnect(sh, "reorder_filters", obsFilterAddedCb, this);
		}
	}
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

} // namespace super
