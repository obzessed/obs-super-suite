// control_assign_popup_dialog.cpp — Part 2: BindingPanel + ControlAssignPopup
#include "control_assign_popup.hpp"
#include "../core/control_registry.hpp"
#include "../core/control_port.hpp"
#include "../../utils/midi/midi_backend.hpp"
#include <QApplication>
#include <QScreen>

namespace super {

// ===== BindingPanel =======================================================
BindingPanel::BindingPanel(int index, int map_mode,
	double out_min, double out_max, const QStringList &combo_items, QWidget *parent)
	: QFrame(parent), m_index(index), m_map_mode(map_mode)
	, m_default_out_min(out_min), m_default_out_max(out_max), m_combo_items(combo_items)
{ setFrameShape(QFrame::StyledPanel); setup_ui(); }

void BindingPanel::setup_ui() {
	auto *top = new QVBoxLayout(this); top->setContentsMargins(4,4,4,4); top->setSpacing(2);
	// Header
	auto *hdr = new QHBoxLayout();
	m_header_dot = new ActivityDot(QColor(80,180,255), this);
	m_header_btn = new QPushButton(QString("▶ Binding #%1").arg(m_index+1),this);
	m_header_btn->setFlat(true); m_header_btn->setStyleSheet("text-align:left;font-weight:bold;padding:4px;");
	m_header_enabled = new QCheckBox(this); m_header_enabled->setChecked(true);
	m_header_remove = new QPushButton("✕",this); m_header_remove->setFixedSize(20,20); m_header_remove->setStyleSheet("color:#e74c3c;");
	hdr->addWidget(m_header_dot); hdr->addWidget(m_header_btn,1); hdr->addWidget(m_header_enabled); hdr->addWidget(m_header_remove);
	top->addLayout(hdr);

	m_body = new QWidget(this); m_body->setVisible(false);
	auto *bl = new QVBoxLayout(m_body); bl->setContentsMargins(8,4,8,4); bl->setSpacing(4);

	// 1. MIDI Source
	auto *src = new QGroupBox("MIDI Source", m_body);
	auto *sf = new QFormLayout(src); sf->setContentsMargins(8,4,8,4); sf->setSpacing(3);
	m_device_combo = new QComboBox(src); sf->addRow("Device:",m_device_combo);
	m_channel_spin = new QSpinBox(src); m_channel_spin->setRange(0,15); sf->addRow("Channel:",m_channel_spin);
	m_cc_spin = new QSpinBox(src); m_cc_spin->setRange(0,127); sf->addRow("CC/Note:",m_cc_spin);
	bl->addWidget(src);

	// 2. Pre-Filters (raw domain) — not for Select
	if (m_map_mode != MidiPortBinding::Select) {
		m_pre_filter_group = new QGroupBox("Pre-Filters (Raw MIDI)", m_body);
		auto *pfv = new QVBoxLayout(m_pre_filter_group); pfv->setContentsMargins(4,4,4,4); pfv->setSpacing(2);
		m_pre_filter_layout = new QVBoxLayout(); m_pre_filter_layout->setSpacing(2);
		pfv->addLayout(m_pre_filter_layout);
		auto *pfa = new QPushButton("+ Add Pre-Filter",m_pre_filter_group);
		pfa->setStyleSheet("color:#2ecc71;font-size:10px;");
		pfv->addWidget(pfa);
		bl->addWidget(m_pre_filter_group);
		connect(pfa,&QPushButton::clicked,this,[this]{ add_pre_filter({}); emit changed(); });
	}

	// 3. Value Mapping (Range mode)
	if (m_map_mode == MidiPortBinding::Range) {
		m_range_group = new QGroupBox("Mapping (Input→Output)", m_body);
		auto *rf = new QFormLayout(m_range_group); rf->setContentsMargins(8,4,8,4); rf->setSpacing(3);
		m_input_min_spin = new QSpinBox(m_range_group); m_input_min_spin->setRange(0,127); rf->addRow("In Min:",m_input_min_spin);
		m_input_max_spin = new QSpinBox(m_range_group); m_input_max_spin->setRange(0,127); m_input_max_spin->setValue(127); rf->addRow("In Max:",m_input_max_spin);
		m_output_min_spin = new QDoubleSpinBox(m_range_group); m_output_min_spin->setRange(-9999,9999); m_output_min_spin->setDecimals(2); m_output_min_spin->setValue(m_default_out_min); rf->addRow("Out Min:",m_output_min_spin);
		m_output_max_spin = new QDoubleSpinBox(m_range_group); m_output_max_spin->setRange(-9999,9999); m_output_max_spin->setDecimals(2); m_output_max_spin->setValue(m_default_out_max); rf->addRow("Out Max:",m_output_max_spin);
		bl->addWidget(m_range_group);
	}

	// 4. Interpolation Chain (Range mode)
	if (m_map_mode == MidiPortBinding::Range) {
		m_interp_group = new QGroupBox("Interpolation Chain", m_body);
		auto *iv = new QVBoxLayout(m_interp_group); iv->setContentsMargins(4,4,4,4); iv->setSpacing(2);
		m_interp_layout = new QVBoxLayout(); m_interp_layout->setSpacing(2);
		iv->addLayout(m_interp_layout);
		auto *ia = new QPushButton("+ Add Interpolation",m_interp_group);
		ia->setStyleSheet("color:#3498db;font-size:10px;");
		iv->addWidget(ia);
		bl->addWidget(m_interp_group);
		connect(ia,&QPushButton::clicked,this,[this]{ add_interp_stage({}); emit changed(); });
	}

	// Threshold group (Toggle/Trigger)
	if (m_map_mode == MidiPortBinding::Toggle || m_map_mode == MidiPortBinding::Trigger) {
		m_threshold_group = new QGroupBox("Threshold", m_body);
		auto *tf = new QFormLayout(m_threshold_group); tf->setContentsMargins(8,4,8,4); tf->setSpacing(3);
		m_threshold_spin = new QSpinBox(m_threshold_group); m_threshold_spin->setRange(0,127); m_threshold_spin->setValue(63);
		tf->addRow("Value:",m_threshold_spin);
		if (m_map_mode == MidiPortBinding::Toggle) {
			m_toggle_mode_combo = new QComboBox(m_threshold_group);
			m_toggle_mode_combo->addItem("Toggle", 0);
			m_toggle_mode_combo->addItem("Check (Set On)", 1);
			m_toggle_mode_combo->addItem("Uncheck (Set Off)", 2);
			tf->addRow("Mode:", m_toggle_mode_combo);
			connect(m_toggle_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{ emit changed(); });
		}
		if (m_map_mode == MidiPortBinding::Trigger) {
			m_continuous_check = new QCheckBox("Continuous fire",m_threshold_group); tf->addRow("",m_continuous_check);
			m_continuous_interval_spin = new QSpinBox(m_threshold_group);
			m_continuous_interval_spin->setRange(16,5000); m_continuous_interval_spin->setValue(100); m_continuous_interval_spin->setSuffix(" ms");
			tf->addRow("Interval:",m_continuous_interval_spin);
		}
		bl->addWidget(m_threshold_group);
	}

	// 5. Post-Filters (output domain) — not for Select
	if (m_map_mode != MidiPortBinding::Select) {
		m_post_filter_group = new QGroupBox("Post-Filters (Output)", m_body);
		auto *pov = new QVBoxLayout(m_post_filter_group); pov->setContentsMargins(4,4,4,4); pov->setSpacing(2);
		m_post_filter_layout = new QVBoxLayout(); m_post_filter_layout->setSpacing(2);
		pov->addLayout(m_post_filter_layout);
		auto *poa = new QPushButton("+ Add Post-Filter",m_post_filter_group);
		poa->setStyleSheet("color:#e67e22;font-size:10px;");
		pov->addWidget(poa);
		bl->addWidget(m_post_filter_group);
		connect(poa,&QPushButton::clicked,this,[this]{ add_post_filter({}); emit changed(); });
	}

	// 6. Action — not for Select
	if (m_map_mode != MidiPortBinding::Select) {
		m_action_group = new QGroupBox("Action", m_body);
		auto *af = new QFormLayout(m_action_group); af->setContentsMargins(8,4,8,4); af->setSpacing(3);
		m_action_combo = new QComboBox(m_action_group);
		m_action_combo->addItem("Set Value",0); m_action_combo->addItem("Animate To",1);
		m_action_combo->addItem("Animate From",2); m_action_combo->addItem("Trigger",3);
		af->addRow("Mode:",m_action_combo);
		m_action_p1_label = new QLabel("ms:",m_action_group);
		m_action_p1 = new QDoubleSpinBox(m_action_group); m_action_p1->setRange(10,10000); m_action_p1->setDecimals(0); m_action_p1->setValue(500);
		af->addRow(m_action_p1_label,m_action_p1);
		m_action_p2_label = new QLabel("Easing:",m_action_group);
		m_action_p2 = new QDoubleSpinBox(m_action_group); m_action_p2->setRange(0,40); m_action_p2->setDecimals(0);
		af->addRow(m_action_p2_label,m_action_p2);
		bl->addWidget(m_action_group);
		auto update_action_vis = [this]{
			bool anim = m_action_combo->currentData().toInt() >= 1 && m_action_combo->currentData().toInt() <= 2;
			m_action_p1_label->setVisible(anim); m_action_p1->setVisible(anim);
			m_action_p2_label->setVisible(anim); m_action_p2->setVisible(anim);
			emit changed();
		};
		connect(m_action_combo,QOverload<int>::of(&QComboBox::currentIndexChanged),this,update_action_vis);
		connect(m_action_p1,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this]{emit changed();});
		connect(m_action_p2,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this]{emit changed();});
		update_action_vis();
	}

	// 7. Options
	m_invert_check = new QCheckBox("Invert", m_body);
	bl->addWidget(m_invert_check);
	top->addWidget(m_body);

	// Signals — header
	connect(m_header_btn,&QPushButton::clicked,this,[this]{emit expand_requested(m_index);});
	connect(m_header_remove,&QPushButton::clicked,this,[this]{emit remove_requested(m_index);});
	connect(m_header_enabled,&QCheckBox::toggled,this,[this]{emit changed();});
	connect(m_invert_check,&QCheckBox::toggled,this,[this]{emit changed();});
	// Signals — MIDI source
	connect(m_device_combo,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{emit changed();});
	connect(m_channel_spin,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{emit changed();});
	connect(m_cc_spin,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{emit changed();});
	// Signals — Range mapping
	if (m_input_min_spin)
		connect(m_input_min_spin,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{emit changed();});
	if (m_input_max_spin)
		connect(m_input_max_spin,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{emit changed();});
	if (m_output_min_spin)
		connect(m_output_min_spin,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this]{emit changed();});
	if (m_output_max_spin)
		connect(m_output_max_spin,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this]{emit changed();});
	// Signals — Threshold/Trigger extras
	if (m_threshold_spin)
		connect(m_threshold_spin,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{emit changed();});
	if (m_continuous_check)
		connect(m_continuous_check,&QCheckBox::toggled,this,[this]{emit changed();});
	if (m_continuous_interval_spin)
		connect(m_continuous_interval_spin,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{emit changed();});
}

void BindingPanel::pulse_header_activity() {
	if (m_header_dot) m_header_dot->pulse();
}

void BindingPanel::add_pre_filter(const FilterStage &s) {
	int idx = m_pre_filter_rows.size();
	auto *row = new FilterStageRow(idx, QColor(46,204,113), m_pre_filter_group);
	row->set_title_prefix("Pre-Filter");
	row->m_graph->set_range(0.0, 127.0); // Pre-filters operate on raw MIDI domain
	if (s.type||s.param1||s.param2||!s.enabled) row->load(s);
	row->update_title("Pre-Filter", idx+1);
	m_pre_filter_layout->addWidget(row); m_pre_filter_rows.append(row);
	connect(row,&StageRow::changed,this,[this]{emit changed();});
	connect(row,&StageRow::move_up,this,[this](int i){ if(i>0){std::swap(m_pre_filter_rows[i],m_pre_filter_rows[i-1]); rebuild_indices(m_pre_filter_rows,m_pre_filter_layout); emit changed();}});
	connect(row,&StageRow::move_down,this,[this](int i){ if(i<m_pre_filter_rows.size()-1){std::swap(m_pre_filter_rows[i],m_pre_filter_rows[i+1]); rebuild_indices(m_pre_filter_rows,m_pre_filter_layout); emit changed();}});
	connect(row,&StageRow::remove,this,[this](int i){ if(i>=0&&i<m_pre_filter_rows.size()){auto *w=m_pre_filter_rows.takeAt(i); m_pre_filter_layout->removeWidget(w); w->deleteLater(); rebuild_indices(m_pre_filter_rows,m_pre_filter_layout); emit changed();}});
}
void BindingPanel::add_interp_stage(const InterpStage &s) {
	int idx = m_interp_rows.size();
	auto *row = new InterpStageRow(idx, m_interp_group);
	row->set_title_prefix("Interp");
	if (s.type||s.param1||s.param2||!s.enabled) row->load(s);
	row->update_title("Interp", idx+1);
	m_interp_layout->addWidget(row); m_interp_rows.append(row);
	connect(row,&StageRow::changed,this,[this]{emit changed();});
	connect(row,&StageRow::move_up,this,[this](int i){ if(i>0){std::swap(m_interp_rows[i],m_interp_rows[i-1]); rebuild_indices(m_interp_rows,m_interp_layout); emit changed();}});
	connect(row,&StageRow::move_down,this,[this](int i){ if(i<m_interp_rows.size()-1){std::swap(m_interp_rows[i],m_interp_rows[i+1]); rebuild_indices(m_interp_rows,m_interp_layout); emit changed();}});
	connect(row,&StageRow::remove,this,[this](int i){ if(i>=0&&i<m_interp_rows.size()){auto *w=m_interp_rows.takeAt(i); m_interp_layout->removeWidget(w); w->deleteLater(); rebuild_indices(m_interp_rows,m_interp_layout); emit changed();}});
}
void BindingPanel::add_post_filter(const FilterStage &s) {
	int idx = m_post_filter_rows.size();
	auto *row = new FilterStageRow(idx, QColor(230,126,34), m_post_filter_group);
	row->set_title_prefix("Post-Filter");
	// Post-filters operate on output domain
	double omin = m_output_min_spin ? m_output_min_spin->value() : 0.0;
	double omax = m_output_max_spin ? m_output_max_spin->value() : 1.0;
	row->m_graph->set_range(omin, omax);
	if (s.type||s.param1||s.param2||!s.enabled) row->load(s);
	row->update_title("Post-Filter", idx+1);
	m_post_filter_layout->addWidget(row); m_post_filter_rows.append(row);
	connect(row,&StageRow::changed,this,[this]{emit changed();});
	connect(row,&StageRow::move_up,this,[this](int i){ if(i>0){std::swap(m_post_filter_rows[i],m_post_filter_rows[i-1]); rebuild_indices(m_post_filter_rows,m_post_filter_layout); emit changed();}});
	connect(row,&StageRow::move_down,this,[this](int i){ if(i<m_post_filter_rows.size()-1){std::swap(m_post_filter_rows[i],m_post_filter_rows[i+1]); rebuild_indices(m_post_filter_rows,m_post_filter_layout); emit changed();}});
	connect(row,&StageRow::remove,this,[this](int i){ if(i>=0&&i<m_post_filter_rows.size()){auto *w=m_post_filter_rows.takeAt(i); m_post_filter_layout->removeWidget(w); w->deleteLater(); rebuild_indices(m_post_filter_rows,m_post_filter_layout); emit changed();}});
}

void BindingPanel::rebuild_indices(QVector<StageRow*> &rows, QVBoxLayout *layout) {
	for (int i=0;i<rows.size();i++) {
		layout->removeWidget(rows[i]);
		rows[i]->set_index(i);
		rows[i]->update_title({}, i+1);
	}
	for (auto *r : rows) layout->addWidget(r);
}

void BindingPanel::load_from_binding(const MidiPortBinding &b) {
	m_header_enabled->setChecked(b.enabled);
	// device_index -1 means "any" → combo index 0; otherwise offset by +1 for "(Any)" entry
	int combo_idx = (b.device_index < 0) ? 0 : b.device_index + 1;
	if(combo_idx < m_device_combo->count()) m_device_combo->setCurrentIndex(combo_idx);
	m_channel_spin->setValue(b.channel); m_cc_spin->setValue(b.data1);
	if(m_input_min_spin) m_input_min_spin->setValue(b.input_min);
	if(m_input_max_spin) m_input_max_spin->setValue(b.input_max);
	if(m_output_min_spin) m_output_min_spin->setValue(b.output_min);
	if(m_output_max_spin) m_output_max_spin->setValue(b.output_max);
	if(m_threshold_spin) m_threshold_spin->setValue(b.threshold);
	if(m_toggle_mode_combo) { int ti=m_toggle_mode_combo->findData(b.toggle_mode); if(ti>=0) m_toggle_mode_combo->setCurrentIndex(ti); }
	if(m_continuous_check) m_continuous_check->setChecked(b.continuous_fire);
	if(m_continuous_interval_spin) m_continuous_interval_spin->setValue(b.continuous_fire_interval_ms);
	m_invert_check->setChecked(b.invert);
	m_is_encoder=b.is_encoder; m_encoder_mode=b.encoder_mode; m_encoder_sensitivity=b.encoder_sensitivity;
	if(m_action_combo) { int ai=m_action_combo->findData(static_cast<int>(b.action_mode)); if(ai>=0)m_action_combo->setCurrentIndex(ai); }
	if(m_action_p1) m_action_p1->setValue(b.action_param1);
	if(m_action_p2) m_action_p2->setValue(b.action_param2);
	for(const auto &f : b.pre_filters) add_pre_filter(f);
	for(const auto &s : b.interp_stages) add_interp_stage(s);
	for(const auto &f : b.post_filters) add_post_filter(f);
	update_header();
}

MidiPortBinding BindingPanel::build_binding(const QString &port_id) const {
	MidiPortBinding b;
	b.port_id=port_id; b.enabled=m_header_enabled->isChecked();
	// combo index 0 = "(Any)" → device_index -1; otherwise offset by -1
	b.device_index = m_device_combo->currentIndex() - 1;
	b.channel=m_channel_spin->value(); b.data1=m_cc_spin->value();
	b.map_mode=static_cast<MidiPortBinding::MapMode>(m_map_mode);
	if(m_input_min_spin) b.input_min=m_input_min_spin->value();
	if(m_input_max_spin) b.input_max=m_input_max_spin->value();
	if(m_output_min_spin) b.output_min=m_output_min_spin->value();
	if(m_output_max_spin) b.output_max=m_output_max_spin->value();
	if(m_threshold_spin) b.threshold=m_threshold_spin->value();
	if(m_toggle_mode_combo) b.toggle_mode=m_toggle_mode_combo->currentData().toInt();
	if(m_continuous_check) b.continuous_fire=m_continuous_check->isChecked();
	if(m_continuous_interval_spin) b.continuous_fire_interval_ms=m_continuous_interval_spin->value();
	b.invert=m_invert_check->isChecked();
	b.is_encoder=m_is_encoder; b.encoder_mode=m_encoder_mode; b.encoder_sensitivity=m_encoder_sensitivity;
	if(m_action_combo) b.action_mode=static_cast<ActionMode>(m_action_combo->currentData().toInt());
	if(m_action_p1) b.action_param1=m_action_p1->value();
	if(m_action_p2) b.action_param2=m_action_p2->value();
	for(auto *r : m_pre_filter_rows) b.pre_filters.append(static_cast<FilterStageRow*>(r)->build());
	for(auto *r : m_interp_rows) b.interp_stages.append(static_cast<InterpStageRow*>(r)->build());
	for(auto *r : m_post_filter_rows) b.post_filters.append(static_cast<FilterStageRow*>(r)->build());
	return b;
}

void BindingPanel::reset_to_defaults() {
	m_header_enabled->setChecked(true);
	if(m_input_min_spin) m_input_min_spin->setValue(0);
	if(m_input_max_spin) m_input_max_spin->setValue(127);
	if(m_output_min_spin) m_output_min_spin->setValue(m_default_out_min);
	if(m_output_max_spin) m_output_max_spin->setValue(m_default_out_max);
	m_invert_check->setChecked(false);
	if(m_action_combo) m_action_combo->setCurrentIndex(0);
	qDeleteAll(m_pre_filter_rows); m_pre_filter_rows.clear();
	qDeleteAll(m_interp_rows); m_interp_rows.clear();
	qDeleteAll(m_post_filter_rows); m_post_filter_rows.clear();
	// Default: add one Linear interp so mapping works out of the box
	if (m_map_mode == MidiPortBinding::Range) add_interp_stage({});
}

void BindingPanel::populate_devices(const QStringList &d) { m_device_combo->clear(); m_device_combo->addItems(d); }
void BindingPanel::set_learned_source(int dev, int ch, int cc, bool enc, EncoderMode em, double es) {
	// dev is raw device index; combo has "(Any)" at 0, so offset +1
	int combo_idx = (dev < 0) ? 0 : dev + 1;
	if(combo_idx < m_device_combo->count()) m_device_combo->setCurrentIndex(combo_idx);
	m_channel_spin->setValue(ch); m_cc_spin->setValue(cc);
	m_is_encoder=enc; m_encoder_mode=em; m_encoder_sensitivity=es;
	update_header(); emit changed();
}
void BindingPanel::set_expanded(bool e) { m_expanded=e; m_body->setVisible(e); update_header(); }
bool BindingPanel::is_expanded() const { return m_expanded; }
void BindingPanel::set_index(int i) { m_index=i; update_header(); }
int BindingPanel::index() const { return m_index; }
void BindingPanel::update_header() {
	m_header_btn->setText(QString("%1 Binding #%2  [Ch%3 CC%4]")
		.arg(m_expanded?"▼":"▶").arg(m_index+1).arg(m_channel_spin->value()).arg(m_cc_spin->value()));
}

// Sync UI parameters into m_preview_state, preserving runtime state for matching stages
void BindingPanel::sync_preview_params() {
	// Sync simple binding params from UI
	m_preview_state.map_mode = static_cast<MidiPortBinding::MapMode>(m_map_mode);
	m_preview_state.device_index = m_device_combo ? m_device_combo->currentIndex() - 1 : -1;
	m_preview_state.channel = m_channel_spin ? m_channel_spin->value() : 0;
	m_preview_state.data1 = m_cc_spin ? m_cc_spin->value() : 0;
	if (m_input_min_spin) m_preview_state.input_min = m_input_min_spin->value();
	if (m_input_max_spin) m_preview_state.input_max = m_input_max_spin->value();
	if (m_output_min_spin) m_preview_state.output_min = m_output_min_spin->value();
	if (m_output_max_spin) m_preview_state.output_max = m_output_max_spin->value();
	if (m_threshold_spin) m_preview_state.threshold = m_threshold_spin->value();
	m_preview_state.invert = m_invert_check ? m_invert_check->isChecked() : false;
	if (m_action_combo) m_preview_state.action_mode = static_cast<ActionMode>(m_action_combo->currentData().toInt());
	if (m_action_p1) m_preview_state.action_param1 = m_action_p1->value();
	if (m_action_p2) m_preview_state.action_param2 = m_action_p2->value();

	// Sync pre-filters: preserve runtime state when type matches
	auto sync_filters = [](const QVector<StageRow*> &rows, QVector<FilterStage> &stages) {
		int new_size = rows.size();
		// Grow/shrink
		while (stages.size() > new_size) stages.removeLast();
		while (stages.size() < new_size) stages.append(FilterStage{});
		for (int i = 0; i < new_size; i++) {
			auto built = static_cast<FilterStageRow*>(rows[i])->build();
			if (stages[i].type != built.type) {
				// Type changed — full reset
				stages[i] = built;
			} else {
				// Same type — update params only, keep runtime
				stages[i].enabled = built.enabled;
				stages[i].param1 = built.param1;
				stages[i].param2 = built.param2;
			}
		}
	};
	sync_filters(m_pre_filter_rows, m_preview_state.pre_filters);
	sync_filters(m_post_filter_rows, m_preview_state.post_filters);

	// Sync interp stages
	{
		int new_size = m_interp_rows.size();
		while (m_preview_state.interp_stages.size() > new_size)
			m_preview_state.interp_stages.removeLast();
		while (m_preview_state.interp_stages.size() < new_size)
			m_preview_state.interp_stages.append(InterpStage{});
		for (int i = 0; i < new_size; i++) {
			auto built = static_cast<InterpStageRow*>(m_interp_rows[i])->build();
			if (m_preview_state.interp_stages[i].type != built.type) {
				m_preview_state.interp_stages[i] = built;
			} else {
				m_preview_state.interp_stages[i].enabled = built.enabled;
				m_preview_state.interp_stages[i].param1 = built.param1;
				m_preview_state.interp_stages[i].param2 = built.param2;
			}
		}
	}
}

double BindingPanel::update_pipeline_preview(int raw) {
	sync_preview_params();
	auto p = m_preview_state.preview_pipeline(raw);
	m_last_preview = p;
	// Pulse + preview pre-filters
	for(int i=0;i<m_pre_filter_rows.size()&&i<p.after_pre_filter.size();i++) {
		double in = (i==0) ? double(raw) : p.after_pre_filter[i-1];
		m_pre_filter_rows[i]->set_preview(in, p.after_pre_filter[i]);
		m_pre_filter_rows[i]->pulse_activity();
	}
	// Pulse + preview interps
	for(int i=0;i<m_interp_rows.size()&&i<p.after_interp.size();i++) {
		double in = (i==0) ? p.normalized : p.after_interp[i-1];
		m_interp_rows[i]->set_preview(in, p.after_interp[i]);
		m_interp_rows[i]->pulse_activity();
	}
	// Pulse + preview post-filters
	for(int i=0;i<m_post_filter_rows.size()&&i<p.after_post_filter.size();i++) {
		double in = (i==0) ? p.mapped : p.after_post_filter[i-1];
		m_post_filter_rows[i]->set_preview(in, p.after_post_filter[i]);
		m_post_filter_rows[i]->pulse_activity();
	}
	return p.final_value;
}

bool BindingPanel::needs_preview_convergence() const {
	return m_preview_state.needs_convergence();
}

// ===== ControlAssignPopup =================================================
static const char *POPUP_STYLE =
	"QDialog{background:rgba(28,28,36,245);}"
	"QGroupBox{font-size:11px;font-weight:bold;color:#aab;border:1px solid rgba(255,255,255,0.08);border-radius:4px;margin-top:8px;padding-top:10px;}"
	"QGroupBox::title{subcontrol-origin:margin;left:8px;padding:0 4px;color:#8af;}"
	"QLabel{color:#ccc;font-size:11px;}"
	"QSpinBox,QDoubleSpinBox,QComboBox{background:rgba(40,40,55,200);color:#ddd;border:1px solid rgba(255,255,255,0.1);border-radius:3px;padding:2px 4px;font-size:11px;}"
	"QCheckBox{color:#bbb;font-size:11px;}"
	"QPushButton{background:rgba(50,60,80,200);color:#ccc;border:1px solid rgba(255,255,255,0.1);border-radius:4px;padding:4px 10px;font-size:11px;}"
	"QPushButton:hover{background:rgba(60,80,120,220);color:#fff;}"
	"QPushButton:disabled{color:#666;background:rgba(40,40,50,150);}"
	"QTabWidget::pane{border:1px solid rgba(255,255,255,0.08);border-radius:4px;}"
	"QTabBar::tab{background:rgba(40,40,55,200);color:#999;padding:4px 12px;border-top-left-radius:4px;border-top-right-radius:4px;}"
	"QTabBar::tab:selected{background:rgba(60,70,100,220);color:#fff;}";

ControlAssignPopup::ControlAssignPopup(const QString &port_id,
	const QString &display_name, int map_mode,
	double output_min, double output_max,
	const QStringList &combo_items, MidiAdapter *adapter, QWidget *parent)
	: QDialog(parent, Qt::Dialog | Qt::WindowCloseButtonHint)
	, m_port_id(port_id), m_display_name(display_name), m_map_mode(map_mode)
	, m_default_out_min(output_min), m_default_out_max(output_max)
	, m_combo_items(combo_items), m_adapter(adapter)
{
	setWindowTitle(QString("MIDI Assign — %1").arg(display_name));
	setAttribute(Qt::WA_DeleteOnClose);
	setMinimumSize(540, 400);
	resize(580, 550);
	setStyleSheet(POPUP_STYLE);
	setup_ui();
	populate_devices();
	sync_panels_from_adapter();
	sync_outputs_from_adapter();
	// Preview convergence timer — keeps graphs updating during time-based filters
	m_preview_timer = new QTimer(this);
	m_preview_timer->setInterval(16);
	connect(m_preview_timer, &QTimer::timeout, this, &ControlAssignPopup::on_preview_tick);
	m_preview_timer->start();
	mark_clean();
	if (m_adapter && m_adapter->backend())
		connect(m_adapter->backend(), &MidiBackend::midi_message, this, &ControlAssignPopup::on_raw_midi);
	// Initial preview so graphs show something
	refresh_preview(); // FIXME: this modifies the timedomain and posibly triggering output changes
}
ControlAssignPopup::~ControlAssignPopup() { emit closed(); }

void ControlAssignPopup::setup_ui() {
	auto *root = new QVBoxLayout(this); root->setContentsMargins(10,10,10,10); root->setSpacing(6);
	// Master Preview
	m_master_preview = new MasterPreview(m_display_name, m_default_out_min, m_default_out_max, this);
	root->addWidget(m_master_preview);
	// Pipeline visual button — placed in master preview header
	m_pipeline_btn = new QPushButton(QString::fromUtf8("\xF0\x9F\x93\x8A"), this);
	m_pipeline_btn->setFixedSize(26, 22);
	m_pipeline_btn->setToolTip("Pipeline View");
	m_pipeline_btn->setStyleSheet("QPushButton{font-size:12px;padding:0;border:1px solid rgba(100,180,255,60);border-radius:3px;background:rgba(40,40,60,180);}"
		"QPushButton:hover{background:rgba(60,60,90,220);}");
	connect(m_pipeline_btn, &QPushButton::clicked, this, [this]{
		if (m_pipeline_visual) { m_pipeline_visual->raise(); m_pipeline_visual->activateWindow(); return; }
		m_pipeline_visual = new PipelineVisualDialog(m_display_name, m_default_out_min, m_default_out_max, window());
		m_pipeline_visual->show();
		// Immediately feed with current data
		refresh_preview(); // FIXME: this modifies the timedomain and posibly triggering output changes
	});
	m_master_preview->add_pipeline_button(m_pipeline_btn);
	// Status
	m_status_label = new QLabel("Ready",this); m_status_label->setStyleSheet("color:#888;font-size:10px;font-style:italic;");
	root->addWidget(m_status_label);
	// Tabs
	m_tab_widget = new QTabWidget(this); root->addWidget(m_tab_widget,1);

	// === Input tab ===
	auto *in_tab = new QWidget(); auto *il = new QVBoxLayout(in_tab); il->setContentsMargins(4,4,4,4); il->setSpacing(4);
	m_scroll_area = new QScrollArea(in_tab); m_scroll_area->setWidgetResizable(true); m_scroll_area->setFrameShape(QFrame::NoFrame);
	m_panel_container = new QWidget(); m_panel_layout = new QVBoxLayout(m_panel_container);
	m_panel_layout->setContentsMargins(0,0,0,0); m_panel_layout->setSpacing(4); m_panel_layout->addStretch();
	m_scroll_area->setWidget(m_panel_container); il->addWidget(m_scroll_area,1);
	auto *ib = new QHBoxLayout();
	m_add_btn = new QPushButton("+ Add Binding",in_tab);
	m_learn_btn = new QPushButton("🎹 Learn",in_tab); m_learn_btn->setStyleSheet("QPushButton{background:rgba(46,204,113,180);color:#fff;}");
	ib->addWidget(m_add_btn); ib->addWidget(m_learn_btn); ib->addStretch();
	il->addLayout(ib);
	m_tab_widget->addTab(in_tab,"Input");

	// === Output tab ===
	auto *ot = new QWidget(); auto *ol = new QVBoxLayout(ot); ol->setContentsMargins(4,4,4,4); ol->setSpacing(4);
	m_output_scroll = new QScrollArea(ot); m_output_scroll->setWidgetResizable(true); m_output_scroll->setFrameShape(QFrame::NoFrame);
	m_output_container = new QWidget(); m_output_layout = new QVBoxLayout(m_output_container);
	m_output_layout->setContentsMargins(0,0,0,0); m_output_layout->setSpacing(4); m_output_layout->addStretch();
	m_output_scroll->setWidget(m_output_container); ol->addWidget(m_output_scroll,1);
	m_add_output_btn = new QPushButton("+ Add Output",ot); ol->addWidget(m_add_output_btn);
	m_tab_widget->addTab(ot,"Output");

	// Apply
	m_apply_btn = new QPushButton("Apply",this);
	m_apply_btn->setStyleSheet("QPushButton{background:rgba(52,152,219,200);color:#fff;font-weight:bold;padding:6px 16px;}QPushButton:disabled{background:rgba(40,40,50,150);color:#666;}");
	m_apply_btn->setEnabled(false);
	root->addWidget(m_apply_btn);

	// Monitor
	m_monitor_toggle = new QPushButton("MIDI Monitor ▶",this); m_monitor_toggle->setFlat(true); m_monitor_toggle->setStyleSheet("color:#888;font-size:10px;");
	root->addWidget(m_monitor_toggle);
	m_monitor_container = new QWidget(this); m_monitor_container->setVisible(false);
	auto *ml = new QVBoxLayout(m_monitor_container); ml->setContentsMargins(0,0,0,0);
	m_monitor_log = new QPlainTextEdit(m_monitor_container); m_monitor_log->setReadOnly(true); m_monitor_log->setMaximumHeight(80);
	m_monitor_log->setStyleSheet("background:rgba(20,20,28,200);color:#8f8;font-family:monospace;font-size:10px;border:1px solid rgba(255,255,255,0.05);border-radius:3px;");
	auto *clr = new QPushButton("Clear",m_monitor_container); clr->setFixedWidth(50);
	auto *mr = new QHBoxLayout(); mr->addWidget(m_monitor_log,1); mr->addWidget(clr,0,Qt::AlignTop);
	ml->addLayout(mr); root->addWidget(m_monitor_container);

	// Connect
	connect(m_add_btn,&QPushButton::clicked,this,&ControlAssignPopup::on_add_clicked);
	connect(m_learn_btn,&QPushButton::clicked,this,&ControlAssignPopup::on_learn_clicked);
	connect(m_apply_btn,&QPushButton::clicked,this,&ControlAssignPopup::on_apply_clicked);
	connect(m_add_output_btn,&QPushButton::clicked,this,&ControlAssignPopup::on_add_output_clicked);
	connect(m_monitor_toggle,&QPushButton::clicked,this,[this]{toggle_monitor(!m_monitor_container->isVisible());});
	connect(clr,&QPushButton::clicked,this,[this]{m_monitor_log->clear();m_monitor_msg_count=0;});
	if(m_adapter){
		connect(m_adapter,&MidiAdapter::binding_learned,this,&ControlAssignPopup::on_binding_learned);
		connect(m_adapter,&MidiAdapter::learn_cancelled,this,&ControlAssignPopup::on_learn_cancelled);
	}
}

void ControlAssignPopup::populate_devices() {
	m_cached_in_devices.clear(); m_cached_in_devices<<"(Any)";
	m_cached_out_devices.clear(); m_cached_out_devices<<"(Any)";
	if(m_adapter&&m_adapter->backend()){
		auto *be=m_adapter->backend();
		for(auto &d:be->available_devices()) m_cached_in_devices<<d;
		for(auto &d:be->available_output_devices()) m_cached_out_devices<<d;
	}
	for(auto *p:m_panels) p->populate_devices(m_cached_in_devices);
	for(auto *p:m_output_panels) p->populate_devices(m_cached_out_devices);
}

void ControlAssignPopup::sync_panels_from_adapter() {
	if(!m_adapter) return;
	for(const auto &b : m_adapter->bindings_for(m_port_id)) {
		int idx=m_panels.size();
		auto *p=new BindingPanel(idx,m_map_mode,m_default_out_min,m_default_out_max,m_combo_items,m_panel_container);
		p->populate_devices(m_cached_in_devices); p->load_from_binding(b);
		m_panel_layout->insertWidget(m_panel_layout->count()-1,p); m_panels.append(p);
		connect(p,&BindingPanel::expand_requested,this,&ControlAssignPopup::on_panel_expand);
		connect(p,&BindingPanel::remove_requested,this,&ControlAssignPopup::on_panel_remove);
		connect(p,&BindingPanel::changed,this,&ControlAssignPopup::mark_dirty);
		connect(p,&BindingPanel::changed,this,&ControlAssignPopup::refresh_preview);
	}
	if(!m_panels.isEmpty()){m_panels.first()->set_expanded(true);m_active_panel=0;}
}

void ControlAssignPopup::sync_outputs_from_adapter() {
	if(!m_adapter) return;
	for(const auto &o : m_adapter->outputs_for(m_port_id)) {
		int idx=m_output_panels.size();
		auto *p=new OutputBindingPanel(idx,m_output_container);
		p->populate_devices(m_cached_out_devices); p->load(o);
		m_output_layout->insertWidget(m_output_layout->count()-1,p); m_output_panels.append(p);
		connect(p,&OutputBindingPanel::expand_requested,this,&ControlAssignPopup::on_output_expand);
		connect(p,&OutputBindingPanel::remove_requested,this,&ControlAssignPopup::on_output_remove);
		connect(p,&OutputBindingPanel::changed,this,&ControlAssignPopup::mark_dirty);
	}
}

void ControlAssignPopup::mark_dirty(){m_dirty=true;m_apply_btn->setEnabled(true);}
void ControlAssignPopup::mark_clean(){m_dirty=false;m_apply_btn->setEnabled(false);}

void ControlAssignPopup::on_add_clicked() {
	int idx=m_panels.size();
	auto *p=new BindingPanel(idx,m_map_mode,m_default_out_min,m_default_out_max,m_combo_items,m_panel_container);
	p->populate_devices(m_cached_in_devices); p->reset_to_defaults();
	m_panel_layout->insertWidget(m_panel_layout->count()-1,p); m_panels.append(p);
	connect(p,&BindingPanel::expand_requested,this,&ControlAssignPopup::on_panel_expand);
	connect(p,&BindingPanel::remove_requested,this,&ControlAssignPopup::on_panel_remove);
	connect(p,&BindingPanel::changed,this,&ControlAssignPopup::mark_dirty);
	connect(p,&BindingPanel::changed,this,&ControlAssignPopup::refresh_preview);
	on_panel_expand(idx); mark_dirty();
}
void ControlAssignPopup::on_add_output_clicked() {
	int idx=m_output_panels.size();
	auto *p=new OutputBindingPanel(idx,m_output_container);
	p->populate_devices(m_cached_out_devices);
	m_output_layout->insertWidget(m_output_layout->count()-1,p); m_output_panels.append(p);
	connect(p,&OutputBindingPanel::expand_requested,this,&ControlAssignPopup::on_output_expand);
	connect(p,&OutputBindingPanel::remove_requested,this,&ControlAssignPopup::on_output_remove);
	connect(p,&OutputBindingPanel::changed,this,&ControlAssignPopup::mark_dirty);
	on_output_expand(idx); mark_dirty();
}

void ControlAssignPopup::on_learn_clicked() {
	if(!m_adapter)return;
	if(m_adapter->is_learning()){m_adapter->cancel_learn();return;}
	m_adapter->start_learn(m_port_id);
	m_learn_btn->setText("⏳ Listening..."); m_status_label->setText("Move a MIDI control...");
}
void ControlAssignPopup::on_binding_learned(const MidiPortBinding &b) {
	m_learn_btn->setText("🎹 Learn");
	if(m_panels.isEmpty()) on_add_clicked();
	int t=m_active_panel>=0?m_active_panel:0;
	if(t<m_panels.size()) m_panels[t]->set_learned_source(b.device_index,b.channel,b.data1,b.is_encoder,b.encoder_mode,b.encoder_sensitivity);
	// Find device name for status label
	QString dev_name = (b.device_index >= 0 && b.device_index + 1 < m_cached_in_devices.size())
		? m_cached_in_devices[b.device_index + 1] : "Any";
	m_status_label->setText(QString("Learned: %1 Ch%2 CC%3").arg(dev_name).arg(b.channel).arg(b.data1));
	mark_dirty();
}
void ControlAssignPopup::on_learn_cancelled() { m_learn_btn->setText("🎹 Learn"); m_status_label->setText("Learn cancelled"); }

void ControlAssignPopup::on_apply_clicked() {
	if(!m_adapter)return;
	m_adapter->remove_binding(m_port_id);
	for(auto *p:m_panels) m_adapter->add_binding(p->build_binding(m_port_id));
	m_adapter->remove_output(m_port_id);
	for(auto *p:m_output_panels) m_adapter->add_output(p->build(m_port_id));
	m_status_label->setText("Applied"); mark_clean();
}

void ControlAssignPopup::on_panel_expand(int i) {
	bool was_expanded = m_panels[i]->is_expanded();
	for(int j=0;j<m_panels.size();j++) m_panels[j]->set_expanded(false);
	if (!was_expanded) {
		m_panels[i]->set_expanded(true);
		m_active_panel=i;
	} else {
		m_active_panel=i; // keep tracking even when collapsed
	}
}
void ControlAssignPopup::on_panel_remove(int i) {
	if(i<0||i>=m_panels.size())return;
	auto *p=m_panels.takeAt(i); m_panel_layout->removeWidget(p); p->deleteLater();
	for(int j=0;j<m_panels.size();j++) m_panels[j]->set_index(j);
	if(m_active_panel>=m_panels.size()) m_active_panel=m_panels.size()-1;
	mark_dirty();
}
void ControlAssignPopup::on_output_expand(int i) {
	for(int j=0;j<m_output_panels.size();j++) m_output_panels[j]->set_expanded(j==i);
	m_active_output=i;
}
void ControlAssignPopup::on_output_remove(int i) {
	if(i<0||i>=m_output_panels.size())return;
	auto *p=m_output_panels.takeAt(i); m_output_layout->removeWidget(p); p->deleteLater();
	for(int j=0;j<m_output_panels.size();j++) m_output_panels[j]->set_index(j);
	mark_dirty();
}

void ControlAssignPopup::on_raw_midi(int device, int status, int data1, int data2) {
	// Monitor always shows all messages if visible
	if(m_monitor_container->isVisible()){
		if(m_monitor_msg_count>500) m_monitor_log->clear();
		m_monitor_log->appendPlainText(QString("[%1] d1=%2 d2=%3 dev=%4").arg(status,2,16).arg(data1).arg(data2).arg(device));
		m_monitor_msg_count++;
	}
	int mt = status & 0xF0;
	int channel = status & 0x0F;
	if(mt==0xB0 && m_active_panel>=0 && m_active_panel<m_panels.size()) {
		// Sync preview params for source matching (also syncs for pipeline run below)
		auto *panel = m_panels[m_active_panel];
		panel->sync_preview_params();
		auto &ps = panel->preview_state();
		bool device_match = (ps.device_index == -1) || (ps.device_index == device);
		bool source_match = device_match && (channel == ps.channel) && (data1 == ps.data1);
		if(!source_match) return;

		m_master_preview->pulse_input();
		m_master_preview->set_raw_midi(data2);
		m_last_raw = data2;
		panel->pulse_header_activity();
		double val = panel->update_pipeline_preview(data2);
		m_master_preview->set_value(val);
		if (m_pipeline_visual) m_pipeline_visual->feed(data2, panel->last_preview());
	}
}

void ControlAssignPopup::on_preview_tick() {
	if (m_active_panel < 0 || m_active_panel >= m_panels.size()) return;
	auto *panel = m_panels[m_active_panel];
	// Only re-evaluate when time-based stages need convergence (Smooth, AnimateTo, etc.)
	if (!panel->needs_preview_convergence()) return;
	double val = panel->update_pipeline_preview(m_last_raw);
	m_master_preview->set_value(val);
	if (m_pipeline_visual)
		m_pipeline_visual->feed(m_last_raw, panel->last_preview());
}

void ControlAssignPopup::refresh_preview() {
	if (m_active_panel < 0 || m_active_panel >= m_panels.size()) return;
	auto *panel = m_panels[m_active_panel];
	double val = panel->update_pipeline_preview(m_last_raw);
	m_master_preview->set_value(val);
	if (m_pipeline_visual)
		m_pipeline_visual->feed(m_last_raw, panel->last_preview());
}

void ControlAssignPopup::toggle_monitor(bool e) {
	m_monitor_container->setVisible(e);
	m_monitor_toggle->setText(e ? "MIDI Monitor ▼" : "MIDI Monitor ▶");
}

void ControlAssignPopup::show_near(QWidget *target) {
	if(!target){show();return;}
	auto tl=target->mapToGlobal(QPoint(target->width()+8,0));
	auto screen=QApplication::screenAt(tl);
	if(screen){
		auto sr=screen->availableGeometry();
		if(tl.x()+width()>sr.right()) tl.setX(target->mapToGlobal(QPoint(0,0)).x()-width()-8);
		if(tl.y()+height()>sr.bottom()) tl.setY(sr.bottom()-height());
		if(tl.y()<sr.top()) tl.setY(sr.top());
	}
	move(tl); show();
}

} // namespace super
