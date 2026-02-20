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
	setFixedWidth(8); // Slightly wider for visibility
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
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
	// Simple linear map for now, can match fader taper if needed
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

	// Background: Dark gutter
	p.fillRect(rect(), QColor(0x11, 0x11, 0x11));

	// Map peak_db to height
	float level_pos = map_db(peak_db);
	int level_h = (int)(level_pos * h);

	if (level_h <= 0)
		return;

	// Segmented look parameters
	int segment_h = 2;
	int gap = 1;

	// Background of the meter (dark gutter)
	p.fillRect(rect(), QColor(0x18, 0x18, 0x18));

	// Calculate active segments
	int active_h = (int)(level_pos * h);

	for (int y_inv = 0; y_inv < h; y_inv += (segment_h + gap)) {
		int y = h - y_inv - segment_h;
		bool active = (y_inv < active_h);

		QColor c;
		if (!active) {
			// Inactive segments (dimmed)
			c = QColor(0x28, 0x28, 0x28);
		} else {
			// Active segments: Cyan -> White gradient emulation
			// Top (-6dB to 0dB) gets brighter/white
			// Lower is Cyan (#00e5ff)
			float ratio = (float)y_inv / (float)h; // 0.0 (bottom) to 1.0 (top)
			
			if (ratio > 0.9f) // Top ~10% (Clip/Limit)
				c = QColor(0xff, 0xff, 0xff); // White
			else if (ratio > 0.75f) 
				c = QColor(0xb2, 0xeb, 0xf2); // Very light cyan
			else
				c = QColor(0x00, 0xe5, 0xff); // Bright Cyan
		}

		p.fillRect(0, y, w, segment_h, c);
	}
}

// =====================================================================
// DawMixerChannel
// =====================================================================

// dB scale marks to draw
static const int DB_MARKS[] = {6, 3, 0, -3, -6, -9, -12, -24, -48, -60};
static const int DB_MARKS_COUNT = 10;

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
	setFixedWidth(110); // Wider to accommodate the layout
	setMinimumHeight(450);
	setAttribute(Qt::WA_StyledBackground, true);
	setObjectName("DawMixerChannel");
	// Main background
	setStyleSheet("#DawMixerChannel { background: #232323; border-radius: 6px; }");

	auto *main_layout = new QHBoxLayout(this);
	main_layout->setContentsMargins(0, 0, 0, 0);
	main_layout->setSpacing(0);

	// --- Left Strip (The original mixer channel) ---
	auto *strip_widget = new QWidget(this);
	strip_widget->setFixedWidth(110);
	// strip_widget->setStyleSheet("background: #232323; border-radius: 6px;"); // Main background moved here? No, keep on main widget
	
	auto *root = new QVBoxLayout(strip_widget);
	root->setContentsMargins(6, 6, 6, 6);
	root->setSpacing(6);
	
	main_layout->addWidget(strip_widget);

	// --- Right Panel (Expandable) ---
	side_panel = new QWidget(this);
	side_panel->setFixedWidth(160);
	side_panel->setStyleSheet("background: #1e1e1e; border-left: 1px solid #333; border-radius: 0 6px 6px 0;");
	side_panel->setVisible(false); // Hidden by default

	auto *side_layout = new QVBoxLayout(side_panel);
	side_layout->setContentsMargins(8, 8, 8, 8);
	side_layout->setSpacing(8);

	// EFFECTS Group
	auto *effects_header = new QHBoxLayout();
	auto *eff_lbl = new QLabel("EFFECTS", this);
	eff_lbl->setStyleSheet("color: #ddd; font-weight: bold; font-size: 10px;");
	effects_header->addWidget(eff_lbl);
	auto *add_eff_btn = new QPushButton("+", this);
	add_eff_btn->setFixedSize(16, 16);
	add_eff_btn->setStyleSheet("border: 1px solid #555; border-radius: 8px; color: #aaa; padding-bottom: 2px;");
	connect(add_eff_btn, &QPushButton::clicked, this, [this]() {
		if (source) obs_frontend_open_source_interaction(source); // Closest easy thing? Or properties
		// Actually open filters dialog is usually properties -> filters.
		obs_frontend_open_source_filters(source);
	});
	effects_header->addWidget(add_eff_btn);
	side_layout->addLayout(effects_header);

	effects_layout = new QVBoxLayout(); // Items added dynamically
	effects_layout->setSpacing(2);
	side_layout->addLayout(effects_layout);
	
	side_layout->addSpacing(10);
	
	// SENDS Group (Tracks)
	auto *sends_header = new QHBoxLayout();
	auto *sends_lbl = new QLabel("SENDS", this);
	sends_lbl->setStyleSheet("color: #ddd; font-weight: bold; font-size: 10px;");
	sends_header->addWidget(sends_lbl);
	auto *add_send_btn = new QPushButton("+", this); // Maybe unneeded for fixed tracks
	add_send_btn->setFixedSize(16, 16); 
	add_send_btn->setVisible(false);
	sends_header->addWidget(add_send_btn);
	side_layout->addLayout(sends_header);
	
	sends_layout = new QVBoxLayout(); // Track checkboxes
	sends_layout->setSpacing(2);
	side_layout->addLayout(sends_layout);

	side_layout->addStretch();
	
	main_layout->addWidget(side_panel);

	// --- 1. Color Strip ---
	color_strip = new QWidget(this);
	color_strip->setFixedHeight(4);
	color_strip->setStyleSheet("background: #00fa9a; border-radius: 2px;"); // Default green
	root->addWidget(color_strip);

	// --- 2. Track Name ---
	name_label = new QLabel("TRACK", this);
	name_label->setFixedHeight(24);
	name_label->setAlignment(Qt::AlignCenter);
	name_label->setStyleSheet(
		"background: #2b2b2b; color: #ddd; font-family: 'Segoe UI', sans-serif; font-size: 11px; border-radius: 3px; border: 1px solid #333;");
	root->addWidget(name_label);

	// --- 3. Mute / Solo / Rec Buttons ---
	auto *btn_row = new QHBoxLayout();
	btn_row->setSpacing(2);
	btn_row->setContentsMargins(0,0,0,0);

	mute_btn = new QPushButton("M", this);
	mute_btn->setFixedSize(24, 24);
	mute_btn->setCheckable(true);
	mute_btn->setStyleSheet(
		"QPushButton { background: #2b2b2b; color: #888; border: 1px solid #333; border-radius: 3px; font-weight: bold; }"
		"QPushButton:checked { background: #ff4c4c; color: white; border: 1px solid #ff0000; }"
		"QPushButton:hover { border: 1px solid #555; }");
	connect(mute_btn, &QPushButton::clicked, this, &DawMixerChannel::on_mute_clicked);
	btn_row->addWidget(mute_btn);

	solo_btn = new QPushButton("S", this);
	solo_btn->setFixedSize(24, 24);
	solo_btn->setCheckable(true);
	solo_btn->setStyleSheet(
		"QPushButton { background: #2b2b2b; color: #888; border: 1px solid #333; border-radius: 3px; font-weight: bold; }"
		"QPushButton:checked { background: #ffcc00; color: black; border: 1px solid #ffaa00; }"
		"QPushButton:hover { border: 1px solid #555; }");
	btn_row->addWidget(solo_btn);

	auto *rec_btn = new QPushButton("•", this);
	rec_btn->setFixedSize(24, 24);
	rec_btn->setCheckable(true);
	rec_btn->setStyleSheet(
		"QPushButton { background: #2b2b2b; color: #888; border: 1px solid #333; border-radius: 3px; font-weight: bold; }"
		"QPushButton:checked { background: #ff0000; color: white; border: 1px solid #aa0000; }"
		"QPushButton:hover { border: 1px solid #555; }");
	btn_row->addWidget(rec_btn);

	root->addLayout(btn_row);

	// --- 4. Bus/Output Button ---
	bus_btn = new QPushButton("Master", this);
	bus_btn->setFixedHeight(22);
	bus_btn->setStyleSheet(
		"QPushButton { background: #2b2b2b; color: #aaa; border: 1px solid #333; border-radius: 3px; font-size: 10px; }"
		"QPushButton:hover { color: #fff; border: 1px solid #555; }");
	connect(bus_btn, &QPushButton::clicked, this, [this]() {
		if (source) obs_frontend_open_source_properties(source);
		emit settingsRequested();
	});
	root->addWidget(bus_btn);

	// --- 5. Pan Slider ---
	// Small horizontal slider
	pan_slider = new QSlider(Qt::Horizontal, this);
	pan_slider->setRange(-100, 100);
	pan_slider->setValue(0);
	pan_slider->setFixedHeight(16);
	pan_slider->setStyleSheet(
		"QSlider::groove:horizontal { height: 2px; background: #444; }"
		"QSlider::handle:horizontal { width: 8px; height: 8px; margin: -3px 0; background: #ccc; border-radius: 4px; }"
		"QSlider::sub-page:horizontal { background: #444; }"
		"QSlider::add-page:horizontal { background: #444; }");
	connect(pan_slider, &QSlider::valueChanged, this, &DawMixerChannel::on_pan_changed);
	root->addWidget(pan_slider);

	// --- 6. Main Fader Section ---
	// Layout: [Labels] [Fader] [Meter L] [Meter R]
	auto *fader_area = new QHBoxLayout();
	fader_area->setSpacing(4);
	fader_area->setContentsMargins(0, 4, 0, 4);

	// dB Labels handled in paintEvent, but we need spacer/margin
	fader_area->addSpacing(22); // Space for labels

	// Fader
	fader = new QSlider(Qt::Vertical, this);
	fader->setRange(0, 1000); 
	fader->setValue(800);
	fader->setFixedWidth(24);
	fader->setStyleSheet(
		"QSlider::groove:vertical {"
		"  background: #111; width: 4px; border-radius: 2px;"
		"  border: 1px solid #222;"
		"}"
		"QSlider::handle:vertical {"
		"  background: #ffffff;" // Pure white handle
		"  height: 28px; margin: 0 -10px; border-radius: 2px;"
		"  border: 1px solid #ccc;"
		"  box-shadow: 0 1px 3px rgba(0,0,0,0.5);"
		"}"
		"QSlider::add-page:vertical { background: #181818; }"
		"QSlider::sub-page:vertical { background: #181818; }"
	);
	connect(fader, &QSlider::valueChanged, this, &DawMixerChannel::on_fader_changed);
	fader_area->addWidget(fader);

	// Meters
	meter_l = new DawMixerMeter(this);
	meter_r = new DawMixerMeter(this);
	fader_area->addWidget(meter_l);
	fader_area->addWidget(meter_r);

	root->addLayout(fader_area, 1);

	// --- 7. Value Label ---
	val_label = new QLabel("- inf", this);
	val_label->setAlignment(Qt::AlignCenter);
	val_label->setFixedHeight(18);
	val_label->setStyleSheet("color: #aaa; font-size: 10px; font-weight: bold; background: #2b2b2b; border-radius: 2px;");
	root->addWidget(val_label);

	// Use arrow for extra options?
	expand_btn = new QPushButton(">", this);
	expand_btn->setFixedHeight(14);
	expand_btn->setFlat(true);
	expand_btn->setStyleSheet("color: #666; font-size: 10px; border: none; font-weight: bold;");
	connect(expand_btn, &QPushButton::clicked, this, &DawMixerChannel::toggle_expand);
	root->addWidget(expand_btn);

	// --- Timer ---
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
			
			peak_l = -60.0f; peak_r = -60.0f;
			mag_l = -60.0f; mag_r = -60.0f;
		}

		// Decay
		const float decay = 0.8f;
		disp_peak_l = std::max((cur_peak_l > disp_peak_l ? cur_peak_l : disp_peak_l - decay), -60.0f);
		disp_peak_r = std::max((cur_peak_r > disp_peak_r ? cur_peak_r : disp_peak_r - decay), -60.0f);
		disp_mag_l = std::max((cur_mag_l > disp_mag_l ? cur_mag_l : disp_mag_l - decay), -60.0f);
		disp_mag_r = std::max((cur_mag_r > disp_mag_r ? cur_mag_r : disp_mag_r - decay), -60.0f);

		meter_l->set_level(disp_peak_l, disp_mag_l);
		meter_r->set_level(disp_peak_r, disp_mag_r);
	});
	meter_timer->start(33);
}

void DawMixerChannel::paintEvent(QPaintEvent *event)
{
	QWidget::paintEvent(event);
	
	if (!fader) return;

	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing, false);

	// Draw dB scale next to fader
	// Fader is in layout, get its rect relative to this
	QRect fr = fader->geometry(); // relative to widget thanks to layout? No, child coords? 
	// The layout handles positioning. QWidget::geometry() is relative to parent.
	// fader is child of this. rect is correct.

	int top = fr.top();
	int bottom = fr.bottom();
	int h = bottom - top;
	int left = fr.left() - 24; // Left of fader

	QFont f = font();
	f.setPixelSize(9);
	f.setFamily("Segoe UI");
	p.setFont(f);
	p.setPen(QColor(0x88, 0x88, 0x88));
	QFontMetrics metrics(f);
	for (int i = 0; i < DB_MARKS_COUNT; i++) {
		int db = DB_MARKS[i];
		// Map (needs to match fader mapping logic approximately)
		// Standard log fader: 0 is around 70-80%? 
		// Let's use linear mapping of dB to pixels for visual scale? 
		// Or match the cube root taper?
		// OBS fader is cubic: vol = (val/1000)^3.
		// dB = 20 log10(vol).
		// vol = 10^(dB/20).
		// val = 1000 * vol^(1/3) = 1000 * 10^(dB/60).
		
		// If db < -60, val -> 0.
		
		int val = 0;
		if (db <= -60) val = 0;
		else val = (int)(1000.0f * powf(10.0f, db / 60.0f));

		// val is 0..1000. 
		// Slider is inverted in appearance (top is max), so y needs to be calculated from top
		// BUT QSlider value 1000 is top. 
		// QSlider visualizes value relative to height.
		
		// Slider Groove is usually padded. Let's approximate.
		// val 1000 -> top. val 0 -> bottom.
		
		float ratio = (float)val / 1000.0f;
		int y = bottom - (int)(ratio * h); // 0 is bottom

		// Clamp
		if (y < top) y = top;
		if (y > bottom) y = bottom;

		QString text = (db > 0) ? QString("+%1").arg(db) : QString::number(db);
		int textW = metrics.horizontalAdvance(text);
		p.drawText(left + 20 - textW, y + metrics.capHeight() / 2, text);
		
		// Tick
		p.drawLine(fr.left() - 4, y, fr.left() - 2, y);
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
		name_label->setText(name ? QString::fromUtf8(name) : "Channel");

		float vol = obs_source_get_volume(source);
		// Update value label
		float db = 20.0f * log10f(fmaxf(vol, 0.0001f));
		val_label->setText(QString::asprintf("%.1f", db));

		updating_from_source = true;
		float norm = cbrtf(vol);
		fader->setValue((int)(norm * 1000.0f));
		
		float bal = obs_source_get_balance_value(source);
		pan_slider->setValue((int)((bal * 200.0f) - 100.0f));
		
		bool muted = obs_source_muted(source);
		mute_btn->setChecked(muted);
		

		updating_from_source = false;

		refresh_filters();
		refresh_tracks();
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
		signal_handler_connect(sh, "filter_add", obs_source_filter_add_cb, this);
		signal_handler_connect(sh, "filter_remove", obs_source_filter_remove_cb, this);
		signal_handler_connect(sh, "reorder_filters", obs_source_filter_add_cb, this); // Re-use refresh
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
			signal_handler_disconnect(sh, "filter_add", obs_source_filter_add_cb, this);
			signal_handler_disconnect(sh, "filter_remove", obs_source_filter_remove_cb, this);
			signal_handler_disconnect(sh, "reorder_filters", obs_source_filter_add_cb, this);
		}
	}
}

void DawMixerChannel::toggle_expand()
{
	expanded = !expanded;
	side_panel->setVisible(expanded);
	expand_btn->setText(expanded ? "<" : ">");
	
	// Adjust width
	int w = 110 + (expanded ? 160 : 0);
	setFixedWidth(w);
	
	if (expanded) {
		refresh_filters();
		refresh_tracks();
	}
}

void DawMixerChannel::refresh_filters()
{
	if (!source || !effects_layout) return;
	
	// Clear existing
	QLayoutItem *item;
	while ((item = effects_layout->takeAt(0)) != nullptr) {
		delete item->widget();
		delete item;
	}
	
	obs_source_enum_filters(source, [](obs_source_t *parent, obs_source_t *filter, void *param) {
		auto *layout = static_cast<QVBoxLayout*>(param);
		const char *name = obs_source_get_name(filter);
		bool enabled = obs_source_enabled(filter);
		
		auto *row = new QWidget();
		auto *h = new QHBoxLayout(row);
		h->setContentsMargins(0, 0, 0, 0);
		h->setSpacing(4);
		
		auto *lbl = new QLabel(name);
		lbl->setStyleSheet("color: #aaa; font-size: 11px;");
		h->addWidget(lbl);
		
		// TODO: Add enable toggle?
		
		layout->addWidget(row);
	}, effects_layout);
	
	if (effects_layout->count() == 0) {
		auto *lbl = new QLabel("No Filters", side_panel);
		lbl->setStyleSheet("color: #555; font-style: italic; font-size: 10px;");
		effects_layout->addWidget(lbl);
	}
}

void DawMixerChannel::refresh_tracks()
{
	if (!source || !sends_layout) return;
	
	// Clear existing
	QLayoutItem *item;
	while ((item = sends_layout->takeAt(0)) != nullptr) {
		delete item->widget();
		delete item;
	}
	
	uint32_t mixers = obs_source_get_audio_mixers(source);
	
	for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
		int track = i + 1;
		auto *chk = new QCheckBox(QString("Track %1").arg(track), side_panel);
		chk->setStyleSheet("QCheckBox { color: #aaa; font-size: 11px; } QCheckBox::indicator { width: 10px; height: 10px; }");
		chk->setChecked((mixers & (1 << i)) != 0);
		
		connect(chk, &QCheckBox::toggled, this, [this, i](bool checked) {
			if (!source) return;
			uint32_t current = obs_source_get_audio_mixers(source);
			if (checked) 
				current |= (1 << i);
			else 
				current &= ~(1 << i);
			obs_source_set_audio_mixers(source, current);
		});
		
		sends_layout->addWidget(chk);
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
	mute_btn->setChecked(!muted);
	emit muteChanged(!muted);
}

void DawMixerChannel::on_pan_changed(int value)
{
	if (updating_from_source || !source)
		return;
	
	float bal = ((float)value + 100.0f) / 200.0f;
	obs_source_set_balance_value(source, bal);
}

void DawMixerChannel::update_db_label()
{
	if (!source)
		return;
	float vol = obs_source_get_volume(source);
	float db = 20.0f * log10f(fmaxf(vol, 0.0001f));
	if (db < -99.0f) 
		val_label->setText("-inf");
	else
		val_label->setText(QString::asprintf("%.1f", db));
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
		self->update_db_label();
		self->updating_from_source = false;
	});
}

void DawMixerChannel::obs_source_mute_cb(void *data, calldata_t *cd)
{
	DawMixerChannel *self = static_cast<DawMixerChannel *>(data);
	bool muted = calldata_bool(cd, "muted");
	QMetaObject::invokeMethod(self, [self, muted]() {
		self->mute_btn->setChecked(muted);
	});
}

void DawMixerChannel::obs_source_rename_cb(void *data, calldata_t *cd)
{
	DawMixerChannel *self = static_cast<DawMixerChannel *>(data);
	const char *name = calldata_string(cd, "new_name");
	QString qname = name ? QString::fromUtf8(name) : "";
	QMetaObject::invokeMethod(self, [self, qname]() { self->name_label->setText(qname); });
}

void DawMixerChannel::obs_source_filter_add_cb(void *data, calldata_t *)
{
	DawMixerChannel *self = static_cast<DawMixerChannel *>(data);
	QMetaObject::invokeMethod(self, [self]() { self->refresh_filters(); });
}

void DawMixerChannel::obs_source_filter_remove_cb(void *data, calldata_t *)
{
	DawMixerChannel *self = static_cast<DawMixerChannel *>(data);
	QMetaObject::invokeMethod(self, [self]() { self->refresh_filters(); });
}
