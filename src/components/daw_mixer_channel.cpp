#include "daw_mixer_channel.hpp"

#include <QPainter>
#include <QPaintEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QApplication>
#include <QStyle>
#include <QStyleOption>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <cmath>
#include <algorithm>

// =====================================================================
// DawMixerMeter
// =====================================================================

DawMixerMeter::DawMixerMeter(QWidget *parent) : QWidget(parent)
{
	setFixedWidth(6);
	setMinimumHeight(100);
}

float DawMixerMeter::map_db(float db) const
{
	// Map dB to 0.0..1.0 position.  -60 dB -> 0,  0 dB -> 1
	const float min_db = -60.0f;
	const float max_db = 0.0f;
	if (db <= min_db)
		return 0.0f;
	if (db >= max_db)
		return 1.0f;
	return (db - min_db) / (max_db - min_db);
}

void DawMixerMeter::set_level(float peak, float mag)
{
	peak_db = peak;
	mag_db = mag;
	update();
}

void DawMixerMeter::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	int w = width();
	int h = height();

	// Background
	p.fillRect(rect(), QColor(0x18, 0x18, 0x18));

	float level_pos = map_db(peak_db);
	int level_h = (int)(level_pos * h);

	if (level_h <= 0)
		return;

	// dB thresholds -> pixel positions (from bottom)
	int warn_y = (int)(map_db(-20.0f) * h); // green -> yellow boundary
	int err_y = (int)(map_db(-9.0f) * h);   // yellow -> red boundary

	// Draw from bottom up, line by line, choosing color per zone
	for (int i = 0; i < level_h; i++) {
		int y = h - 1 - i;
		QColor c;
		if (i >= err_y)
			c = QColor(0xff, 0x4c, 0x4c); // Red
		else if (i >= warn_y)
			c = QColor(0xff, 0xff, 0x4c); // Yellow
		else
			c = QColor(0x4c, 0xff, 0x4c); // Green

		p.fillRect(0, y, w, 1, c);
	}
}

// =====================================================================
// DawMixerChannel
// =====================================================================

// dB scale marks to draw
static const int DB_MARKS[] = {10, 0, -10, -20, -30, -40, -50};
static const int DB_MARKS_COUNT = 7;

DawMixerChannel::DawMixerChannel(QWidget *parent, obs_source_t *src) : QWidget(parent)
{
	setup_ui();
	if (src)
		setSource(src);
}

DawMixerChannel::~DawMixerChannel()
{
	disconnect_source();
}

void DawMixerChannel::setup_ui()
{
	setFixedWidth(90);
	setMinimumHeight(400);
	setAttribute(Qt::WA_StyledBackground, true);
	setObjectName("DawMixerChannel");
	setStyleSheet("#DawMixerChannel { background: #2a2a2a; border: 1px solid #3a3a3a; border-radius: 4px; }");

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(4, 6, 4, 6);
	root->setSpacing(4);

	// --- Header: name + settings gear ---
	auto *header = new QHBoxLayout();
	header->setContentsMargins(0, 0, 0, 0);
	header->setSpacing(2);

	name_label = new QLabel("MEDIA", this);
	name_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	name_label->setStyleSheet("font-weight: bold; color: #ddd; font-size: 11px; background: transparent;");
	header->addWidget(name_label, 1);

	settings_btn = new QPushButton(this);
	settings_btn->setFixedSize(16, 16);
	settings_btn->setFlat(true);
	settings_btn->setText(QString::fromUtf8("\xe2\x9a\x99")); // ⚙
	settings_btn->setStyleSheet(
		"QPushButton { color: #888; font-size: 13px; background: transparent; border: none; }"
		"QPushButton:hover { color: #fff; }");
	connect(settings_btn, &QPushButton::clicked, this, [this]() {
		if (source)
			obs_frontend_open_source_properties(source);
		emit settingsRequested();
	});
	header->addWidget(settings_btn);
	root->addLayout(header);

	// --- Pan/Bal section ---
	auto *pan_row = new QHBoxLayout();
	pan_row->setContentsMargins(0, 0, 0, 0);
	pan_row->setSpacing(2);

	pan_label = new QLabel("PAN", this);
	pan_label->setAlignment(Qt::AlignCenter);
	pan_label->setStyleSheet("font-size: 8px; color: #888; font-weight: bold; background: transparent;");
	pan_row->addWidget(pan_label);

	pan_dial = new QDial(this);
	pan_dial->setFixedSize(28, 28);
	pan_dial->setRange(0, 100);
	pan_dial->setValue(50);
	pan_dial->setNotchesVisible(true);
	pan_dial->setStyleSheet("QDial { background: #333; border: 1px solid #555; }");
	connect(pan_dial, &QDial::valueChanged, this, &DawMixerChannel::on_pan_changed);
	pan_row->addWidget(pan_dial);

	// Pan L/R labels
	auto *pan_scale = new QVBoxLayout();
	pan_scale->setSpacing(0);
	pan_scale->setContentsMargins(0, 0, 0, 0);
	auto *pl = new QLabel("L", this);
	pl->setStyleSheet("font-size: 7px; color: #666; background: transparent;");
	auto *pr = new QLabel("R", this);
	pr->setStyleSheet("font-size: 7px; color: #666; background: transparent;");
	pan_scale->addWidget(pl, 0, Qt::AlignTop);
	pan_scale->addWidget(pr, 0, Qt::AlignBottom);
	pan_row->addLayout(pan_scale);

	root->addLayout(pan_row);

	// --- Clip LED + OVR ---
	auto *clip_row = new QHBoxLayout();
	clip_row->setContentsMargins(0, 0, 0, 0);
	clip_row->setSpacing(4);
	clip_row->addStretch();

	clip_led = new QPushButton(this);
	clip_led->setFixedSize(8, 8);
	clip_led->setCheckable(false);
	clip_led->setStyleSheet("QPushButton { background: #444; border: 1px solid #555; border-radius: 4px; }"
				"QPushButton[clipping=\"true\"] { background: #ff0000; border: 1px solid #ff0000; }");
	clip_row->addWidget(clip_led);

	ovr_label = new QLabel("OVR", this);
	ovr_label->setStyleSheet("font-size: 8px; font-weight: bold; color: #666; background: transparent;");
	clip_row->addWidget(ovr_label);
	clip_row->addStretch();
	root->addLayout(clip_row);

	// --- Main area: dB scale | fader | meters ---
	auto *main_area = new QHBoxLayout();
	main_area->setContentsMargins(0, 0, 0, 0);
	main_area->setSpacing(2);

	// dB scale labels (drawn in paintEvent instead for pixel-perfect placement)
	// We still need the fader and meters as widgets
	main_area->addSpacing(20); // Reserve space for dB labels (painted)

	// Fader
	fader = new QSlider(Qt::Vertical, this);
	fader->setRange(0, 1000); // Fine resolution
	fader->setValue(800);     // ~0 dB with cubic taper
	fader->setFixedWidth(20);
	fader->setStyleSheet("QSlider::groove:vertical {"
			     "  background: #1a1a1a; width: 4px; border-radius: 2px;"
			     "  border: 1px solid #444;"
			     "}"
			     "QSlider::handle:vertical {"
			     "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
			     "    stop:0 #777, stop:0.5 #eee, stop:1 #777);"
			     "  height: 24px; margin: 0 -8px; border-radius: 2px;"
			     "  border: 1px solid #555;"
			     "}"
			     "QSlider::add-page:vertical { background: #1a1a1a; }"
			     "QSlider::sub-page:vertical { background: #1a1a1a; }");
	connect(fader, &QSlider::valueChanged, this, &DawMixerChannel::on_fader_changed);
	main_area->addWidget(fader, 0, Qt::AlignHCenter);

	// Meters (left + right channel)
	meter_l = new DawMixerMeter(this);
	meter_r = new DawMixerMeter(this);
	main_area->addWidget(meter_l);
	main_area->addWidget(meter_r);

	main_area->addSpacing(4);
	root->addLayout(main_area, 1); // stretch

	// --- Timer for meter updates ---
	auto *meter_timer = new QTimer(this);
	meter_timer->setTimerType(Qt::PreciseTimer);
	connect(meter_timer, &QTimer::timeout, this, [this]() {
		float cur_peak_l, cur_peak_r, cur_mag_l, cur_mag_r;

		{
			QMutexLocker lock(&meter_mutex);
			cur_peak_l = peak_l;
			cur_peak_r = peak_r;
			cur_mag_l = mag_l;
			cur_mag_r = mag_r;

			// Reset peaks so we catch new max in next interval
			// (If OBS updates faster than 30fps, we want the max in that interval)
			peak_l = -60.0f;
			peak_r = -60.0f;
			mag_l = -60.0f;
			mag_r = -60.0f;
		}

		// Decay logic (simulate VU/peak meter falloff)
		// Falloff speed: ~20dB per second -> ~0.6dB per frame (33ms)
		const float decay = 0.6f;

		if (cur_peak_l > disp_peak_l)
			disp_peak_l = cur_peak_l;
		else
			disp_peak_l = std::max(-60.0f, disp_peak_l - decay);

		if (cur_peak_r > disp_peak_r)
			disp_peak_r = cur_peak_r;
		else
			disp_peak_r = std::max(-60.0f, disp_peak_r - decay);

		if (cur_mag_l > disp_mag_l)
			disp_mag_l = cur_mag_l;
		else
			disp_mag_l = std::max(-60.0f, disp_mag_l - decay);

		if (cur_mag_r > disp_mag_r)
			disp_mag_r = cur_mag_r;
		else
			disp_mag_r = std::max(-60.0f, disp_mag_r - decay);


		meter_l->set_level(disp_peak_l, disp_mag_l);
		meter_r->set_level(disp_peak_r, disp_mag_r);

		// Clip detection
		bool was_clipping = clipping;
		if (disp_peak_l >= -0.1f || disp_peak_r >= -0.1f) {
			clipping = true;
		}
		if (clipping != was_clipping) {
			clip_led->setProperty("clipping", clipping);
			clip_led->style()->unpolish(clip_led);
			clip_led->style()->polish(clip_led);
			ovr_label->setStyleSheet(
				clipping ? "font-size: 8px; font-weight: bold; color: #ff0000; background: transparent;"
					 : "font-size: 8px; font-weight: bold; color: #666; background: transparent;");
		}
	});
	meter_timer->start(33); // ~30 FPS
}

void DawMixerChannel::paintEvent(QPaintEvent *event)
{
	QWidget::paintEvent(event);

	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing, false);

	// Draw dB scale labels alongside the fader
	// The fader widget is inside the main_area layout.
	// We need to find the fader's geometry to align labels.
	if (!fader)
		return;

	QRect fr = fader->geometry();
	int fader_top = fr.top();
	int fader_bottom = fr.bottom();
	int fader_height = fader_bottom - fader_top;
	int label_x = fr.left() - 20; // To the left of the fader

	QFont f = font();
	f.setPixelSize(9);
	p.setFont(f);
	p.setPen(QColor(0x99, 0x99, 0x99));

	// Draw scale from +10 to -50
	for (int i = 0; i < DB_MARKS_COUNT; i++) {
		int db = DB_MARKS[i];
		// Map db to vertical position
		// +10 at top, -50 at bottom
		float t = (float)(10 - db) / 60.0f; // 0..1
		int y = fader_top + (int)(t * fader_height);

		QString text = (db > 0) ? QString("+%1").arg(db) : QString::number(db);
		QRect text_rect(label_x, y - 6, 18, 12);
		p.drawText(text_rect, Qt::AlignRight | Qt::AlignVCenter, text);

		// Tick mark
		p.setPen(QColor(0x55, 0x55, 0x55));
		p.drawLine(fr.left() - 2, y, fr.left(), y);
		p.setPen(QColor(0x99, 0x99, 0x99));
	}
}

void DawMixerChannel::setSource(obs_source_t *src)
{
	if (source == src)
		return;

	disconnect_source();
	source = src;

	if (source) {
		const char *name = obs_source_get_name(source);
		name_label->setText(name ? QString::fromUtf8(name) : "---");

		// Set fader from current volume
		float vol = obs_source_get_volume(source);
		updating_from_source = true;
		float norm = cbrtf(vol);
		fader->setValue((int)(norm * 1000.0f));

		// Set pan
		float bal = obs_source_get_balance_value(source);
		pan_dial->setValue((int)(bal * 100.0f));

		updating_from_source = false;

		connect_source();
	} else {
		name_label->setText("---");
	}
}

QString DawMixerChannel::sourceName() const
{
	if (!source)
		return QString();
	const char *n = obs_source_get_name(source);
	return n ? QString::fromUtf8(n) : QString();
}

void DawMixerChannel::connect_source()
{
	if (!source)
		return;

	signal_handler_t *sh = obs_source_get_signal_handler(source);
	if (sh) {
		signal_handler_connect(sh, "volume", obs_source_volume_cb, this);
		signal_handler_connect(sh, "mute", obs_source_mute_cb, this);
		signal_handler_connect(sh, "rename", obs_source_rename_cb, this);
	}

	volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_attach_source(volmeter, source);
	obs_volmeter_add_callback(volmeter, obs_volmeter_cb, this);
}

void DawMixerChannel::disconnect_source()
{
	if (volmeter) {
		obs_volmeter_remove_callback(volmeter, obs_volmeter_cb, this);
		obs_volmeter_detach_source(volmeter);
		obs_volmeter_destroy(volmeter);
		volmeter = nullptr;
	}

	if (source) {
		signal_handler_t *sh = obs_source_get_signal_handler(source);
		if (sh) {
			signal_handler_disconnect(sh, "volume", obs_source_volume_cb, this);
			signal_handler_disconnect(sh, "mute", obs_source_mute_cb, this);
			signal_handler_disconnect(sh, "rename", obs_source_rename_cb, this);
		}
	}
}

void DawMixerChannel::on_fader_changed(int value)
{
	if (updating_from_source || !source)
		return;

	// Cubic taper: vol = (value/1000)^3
	float norm = value / 1000.0f;
	float vol = norm * norm * norm;
	obs_source_set_volume(source, vol);

	update_db_label();
}

void DawMixerChannel::on_mute_clicked()
{
	if (!source)
		return;
	bool muted = obs_source_muted(source);
	obs_source_set_muted(source, !muted);
	emit muteChanged(!muted);
}

void DawMixerChannel::on_pan_changed(int value)
{
	if (updating_from_source || !source)
		return;
	
	float bal = (float)value / 100.0f;
	obs_source_set_balance_value(source, bal);
}

void DawMixerChannel::update_db_label()
{
	if (!source)
		return;
	float vol = obs_source_get_volume(source);
	float db = 20.0f * log10f(fmaxf(vol, 0.0001f));
	// Currently no separate dB label, the scale serves as reference
	Q_UNUSED(db);
}

// --- OBS callbacks (called from audio / signal threads) ---

void DawMixerChannel::obs_volmeter_cb(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
				      const float peak[MAX_AUDIO_CHANNELS], const float input_peak[MAX_AUDIO_CHANNELS])
{
	Q_UNUSED(input_peak);
	DawMixerChannel *self = static_cast<DawMixerChannel *>(data);

	// Take first two channels (or mono -> duplicate)
	float pl = peak[0];
	float pr = (MAX_AUDIO_CHANNELS > 1) ? peak[1] : peak[0];
	float ml = magnitude[0];
	float mr = (MAX_AUDIO_CHANNELS > 1) ? magnitude[1] : magnitude[0];

	QMutexLocker lock(&self->meter_mutex);
	
	// Accumulate peak/magnitude (max over interval)
	if (pl > self->peak_l) self->peak_l = pl;
	if (pr > self->peak_r) self->peak_r = pr;
	if (ml > self->mag_l) self->mag_l = ml;
	if (mr > self->mag_r) self->mag_r = mr;
}

void DawMixerChannel::obs_source_volume_cb(void *data, calldata_t *cd)
{
	DawMixerChannel *self = static_cast<DawMixerChannel *>(data);
	double vol = calldata_float(cd, "volume");
	QMetaObject::invokeMethod(self, [self, vol]() {
		self->updating_from_source = true;
		float norm = cbrtf((float)vol);
		self->fader->setValue((int)(norm * 1000.0f));
		self->updating_from_source = false;
	});
}

void DawMixerChannel::obs_source_mute_cb(void *data, calldata_t *cd)
{
	Q_UNUSED(cd);
	// Could update a mute button if added
}

void DawMixerChannel::obs_source_rename_cb(void *data, calldata_t *cd)
{
	DawMixerChannel *self = static_cast<DawMixerChannel *>(data);
	const char *name = calldata_string(cd, "new_name");
	QString qname = name ? QString::fromUtf8(name) : "";
	QMetaObject::invokeMethod(self, [self, qname]() { self->name_label->setText(qname); });
}
