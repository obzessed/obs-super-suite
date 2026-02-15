#pragma once
#include "../io/midi_adapter.hpp"
#include <QDialog>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QStringList>
#include <QVector>
#include <QPointer>
#include <QTimer>
#include <QProgressBar>

namespace super {

class MidiAdapter;

// ---------------------------------------------------------------------------
// ActivityDot — A 6×6 colored circle that blinks on signal activity.
// ---------------------------------------------------------------------------
class ActivityDot : public QWidget {
	Q_OBJECT
public:
	ActivityDot(const QColor &color, QWidget *parent = nullptr);
	void pulse();
	QSize sizeHint() const override { return {8, 8}; }
protected:
	void paintEvent(QPaintEvent *) override;
private:
	QColor m_color;
	double m_opacity = 0.15;
	QTimer m_fade_timer;
};

// ---------------------------------------------------------------------------
// PipelineVisualDialog — Full pipeline view with faders and stage graphs.
// ---------------------------------------------------------------------------
class PipelineVisualDialog : public QDialog {
	Q_OBJECT
public:
	PipelineVisualDialog(const QString &name, double out_min, double out_max,
		QWidget *parent = nullptr);
	void feed(int raw, const PipelinePreview &p);
protected:
	void paintEvent(QPaintEvent *) override;
private:
	struct Column {
		QString label;
		QColor color;
		double val_min, val_max;
		QVector<double> buf_in, buf_out;
		int head = 0;
		bool full = false;
		double last_in = 0.0, last_out = 0.0;
		bool dimmed = false;
	};
	void draw_col_graph(QPainter &p, const QRect &area, const Column &c);
	void draw_fader(QPainter &p, const QRect &area, double val,
		double vmin, double vmax, const QColor &col, const QString &lbl);
	QString m_name;
	double m_out_min, m_out_max;
	double m_raw = 0.0, m_final = 0.0;
	QVector<Column> m_columns;
	int m_prev_col_count = 0;
	QString m_prev_name_key;
	static constexpr int COL_BUF = 120;
};

// ---------------------------------------------------------------------------
// GraphDetailDialog — Large resizable graph with time markers and fader bars.
// ---------------------------------------------------------------------------
class GraphDetailDialog : public QDialog {
	Q_OBJECT
public:
	enum FaderPos { FP_Hidden = 0, FP_Left, FP_Right, FP_Top, FP_Bottom };
	GraphDetailDialog(const QString &title, const QColor &primary_color,
		const QColor &secondary_color, double val_min, double val_max,
		QWidget *parent = nullptr);
	void push(double primary, double secondary);
protected:
	void paintEvent(QPaintEvent *) override;
	void contextMenuEvent(QContextMenuEvent *) override;
private:
	void draw_series(QPainter &p, const QVector<double> &buf, int head, bool full,
		const QColor &col, const QRect &area);
	void draw_fader_v(QPainter &p, const QRect &area, double val,
		const QColor &col, const QString &label);
	void draw_fader_h(QPainter &p, const QRect &area, double val,
		const QColor &col, const QString &label);
	QString m_title;
	QColor m_primary_color, m_secondary_color;
	double m_min, m_max;
	static constexpr int BUF_SIZE = 300;
	QVector<double> m_primary, m_secondary;
	int m_head = 0;
	bool m_full = false;
	double m_last_primary = 0.0, m_last_secondary = 0.0;
	FaderPos m_in_pos = FP_Left;   // IN fader position
	FaderPos m_out_pos = FP_Right; // OUT fader position
};

// ---------------------------------------------------------------------------
// MiniGraph — A compact sparkline/oscilloscope for real-time value display.
// ---------------------------------------------------------------------------
class MiniGraph : public QWidget {
	Q_OBJECT
public:
	MiniGraph(const QColor &line_color, int sample_count = 80,
		double val_min = 0.0, double val_max = 1.0,
		QWidget *parent = nullptr);
	void push(double val);
	void push_dual(double val_a, double val_b);
	void set_secondary_color(const QColor &c) { m_line_color_b = c; m_dual = true; }
	void set_range(double mn, double mx) { m_min = mn; m_max = mx; update(); }
	void set_title(const QString &t) { m_title = t; }
	void set_dimmed(bool d) { m_dimmed = d; update(); }
	void close_detail();
	void force_update() { update(); }
	QSize sizeHint() const override { return {m_sample_count, 36}; }
protected:
	void paintEvent(QPaintEvent *) override;
	void mouseDoubleClickEvent(QMouseEvent *) override;
private:
	void draw_series(QPainter &p, const QVector<double> &buf, int head, bool full, const QColor &col);
	void forward_to_detail(double a, double b);
	QColor m_line_color, m_line_color_b;
	int m_sample_count;
	double m_min, m_max;
	QVector<double> m_samples, m_samples_b;
	int m_head = 0;
	bool m_full = false;
	bool m_dual = false;
	bool m_dimmed = false;
	QString m_title;
	QPointer<GraphDetailDialog> m_detail;
};

// ---------------------------------------------------------------------------
// StageRow — Base row for a pipeline stage (filter or interp).
// Shows: [dot] [enable] [type combo] [params] [in→out preview] [↑↓✕]
// ---------------------------------------------------------------------------
class StageRow : public QWidget {
	Q_OBJECT
public:
	StageRow(int index, const QColor &dot_color, QWidget *parent = nullptr);
	virtual ~StageRow();
	void set_preview(double in, double out);
	void set_index(int idx);
	int index() const { return m_index; }
	void pulse_activity();
	bool is_stage_enabled() const;
	virtual void update_title(const QString &prefix, int num);
	void set_title_prefix(const QString &p) { m_title_prefix = p; }
signals:
	void move_up(int index);
	void move_down(int index);
	void remove(int index);
	void changed();
protected:
	void setup_base_row(QHBoxLayout *row);
	int m_index;
	QCheckBox *m_enabled;
	QComboBox *m_type;
	QDoubleSpinBox *m_p1;
	QDoubleSpinBox *m_p2;
	QLabel *m_p1_label;
	QLabel *m_p2_label;
	QLabel *m_preview;
	ActivityDot *m_dot;
	QString m_title_prefix;
public:
	MiniGraph *m_graph;
};

// ---------------------------------------------------------------------------
// InterpStageRow
// ---------------------------------------------------------------------------
class InterpStageRow : public StageRow {
	Q_OBJECT
public:
	InterpStageRow(int index, QWidget *parent = nullptr);
	void load(const InterpStage &s);
	InterpStage build() const;
private:
	void on_type_changed(int combo_idx);
};

// ---------------------------------------------------------------------------
// FilterStageRow
// ---------------------------------------------------------------------------
class FilterStageRow : public StageRow {
	Q_OBJECT
public:
	FilterStageRow(int index, const QColor &color, QWidget *parent = nullptr);
	void load(const FilterStage &s);
	FilterStage build() const;
private:
	void on_type_changed(int combo_idx);
};

// ---------------------------------------------------------------------------
// MasterPreview — Large value display + meter bar at top of dialog.
// ---------------------------------------------------------------------------
class MasterPreview : public QWidget {
	Q_OBJECT
public:
	MasterPreview(const QString &name, double min, double max, QWidget *parent = nullptr);
	void set_value(double val);
	void pulse_input();
	void set_raw_midi(int raw);
	void add_pipeline_button(QPushButton *btn) { m_pipeline_btn_slot->addWidget(btn); }
private:
	QLabel *m_name_label;
	QLabel *m_value_label;
	QLabel *m_raw_label;
	QProgressBar *m_meter;
	ActivityDot *m_input_dot;
	ActivityDot *m_output_dot;
	MiniGraph *m_graph;
	QHBoxLayout *m_pipeline_btn_slot;
	double m_min, m_max;
	double m_last_raw_norm = 0.0;
};

// ---------------------------------------------------------------------------
// OutputBindingPanel
// ---------------------------------------------------------------------------
class OutputBindingPanel : public QFrame {
	Q_OBJECT
public:
	OutputBindingPanel(int index, QWidget *parent = nullptr);
	void load(const MidiOutputBinding &o);
	MidiOutputBinding build(const QString &port_id) const;
	void populate_devices(const QStringList &devices);
	void set_index(int idx);
	void set_expanded(bool expanded);
	bool is_expanded() const;
signals:
	void expand_requested(int index);
	void remove_requested(int index);
	void changed();
private:
	int m_index;
	bool m_expanded = false;
	QPushButton *m_header_btn = nullptr;
	QWidget *m_body = nullptr;
	QCheckBox *m_enabled = nullptr;
	QComboBox *m_device_combo = nullptr;
	QSpinBox *m_channel_spin = nullptr;
	QSpinBox *m_cc_spin = nullptr;
	QDoubleSpinBox *m_in_min_spin = nullptr;
	QDoubleSpinBox *m_in_max_spin = nullptr;
	QSpinBox *m_out_min_spin = nullptr;
	QSpinBox *m_out_max_spin = nullptr;
	QCheckBox *m_on_change_check = nullptr;
};

// ---------------------------------------------------------------------------
// BindingPanel — One accordion for an input binding (full pipeline view).
// ---------------------------------------------------------------------------
class BindingPanel : public QFrame {
	Q_OBJECT
public:
	explicit BindingPanel(int index, int map_mode,
		double default_out_min, double default_out_max,
		const QStringList &combo_items,
		QWidget *parent = nullptr);

	void load_from_binding(const MidiPortBinding &b);
	MidiPortBinding build_binding(const QString &port_id) const;
	void reset_to_defaults();
	void populate_devices(const QStringList &devices);
	void set_learned_source(int device, int channel, int cc,
		bool is_encoder, EncoderMode enc_mode, double enc_sens);
	void set_expanded(bool expanded);
	bool is_expanded() const;
	void set_index(int idx);
	int index() const;
	void update_header();
	double update_pipeline_preview(int raw_midi);
	PipelinePreview last_preview() const { return m_last_preview; }
	bool needs_preview_convergence() const;
	void sync_preview_params();
	const MidiPortBinding &preview_state() const { return m_preview_state; }
	void pulse_header_activity();
signals:
	void expand_requested(int index);
	void remove_requested(int index);
	void changed();
private:
	void setup_ui();
	void add_pre_filter(const FilterStage &s = {});
	void add_interp_stage(const InterpStage &s = {});
	void add_post_filter(const FilterStage &s = {});
	void rebuild_indices(QVector<StageRow*> &rows, QVBoxLayout *layout);

	int m_index;
	int m_map_mode;
	double m_default_out_min, m_default_out_max;
	QStringList m_combo_items;
	bool m_expanded = false;
	bool m_is_encoder = false;
	EncoderMode m_encoder_mode = EncoderMode::Absolute;
	double m_encoder_sensitivity = 1.0;

	// Header
	QPushButton *m_header_btn = nullptr;
	QCheckBox *m_header_enabled = nullptr;
	QPushButton *m_header_remove = nullptr;
	ActivityDot *m_header_dot = nullptr;

	// Body
	QWidget *m_body = nullptr;

	// MIDI Source
	QComboBox *m_device_combo = nullptr;
	QSpinBox *m_channel_spin = nullptr;
	QSpinBox *m_cc_spin = nullptr;

	// Pre-Filters
	QGroupBox *m_pre_filter_group = nullptr;
	QVBoxLayout *m_pre_filter_layout = nullptr;
	QVector<StageRow*> m_pre_filter_rows;

	// Value Mapping
	QGroupBox *m_range_group = nullptr;
	QSpinBox *m_input_min_spin = nullptr;
	QSpinBox *m_input_max_spin = nullptr;
	QDoubleSpinBox *m_output_min_spin = nullptr;
	QDoubleSpinBox *m_output_max_spin = nullptr;

	// Interp chain
	QGroupBox *m_interp_group = nullptr;
	QVBoxLayout *m_interp_layout = nullptr;
	QVector<StageRow*> m_interp_rows;

	// Post-Filters
	QGroupBox *m_post_filter_group = nullptr;
	QVBoxLayout *m_post_filter_layout = nullptr;
	QVector<StageRow*> m_post_filter_rows;

	// Action
	QGroupBox *m_action_group = nullptr;
	QComboBox *m_action_combo = nullptr;
	QDoubleSpinBox *m_action_p1 = nullptr;
	QDoubleSpinBox *m_action_p2 = nullptr;
	QLabel *m_action_p1_label = nullptr;
	QLabel *m_action_p2_label = nullptr;

	// Toggle/Trigger extras
	QGroupBox *m_threshold_group = nullptr;
	QSpinBox *m_threshold_spin = nullptr;
	QComboBox *m_toggle_mode_combo = nullptr;
	QCheckBox *m_continuous_check = nullptr;
	QSpinBox *m_continuous_interval_spin = nullptr;

	// Options
	QCheckBox *m_invert_check = nullptr;

	// Persistent preview state (maintains filter runtime across ticks)
	MidiPortBinding m_preview_state;
	PipelinePreview m_last_preview;
};

// ---------------------------------------------------------------------------
// ControlAssignPopup — Main dialog with Input/Output tabs.
// movable, resizable, with master preview and activity indicators.
// ---------------------------------------------------------------------------
class ControlAssignPopup : public QDialog {
	Q_OBJECT
public:
	explicit ControlAssignPopup(const QString &port_id,
		const QString &display_name,
		int map_mode,
		double output_min, double output_max,
		const QStringList &combo_items,
		MidiAdapter *adapter,
		QWidget *parent = nullptr);
	~ControlAssignPopup() override;

	void show_near(QWidget *target);

signals:
	void closed();

private:
	void setup_ui();
	void populate_devices();
	void sync_panels_from_adapter();
	void sync_outputs_from_adapter();
	void mark_dirty();
	void mark_clean();

	// Input tab actions
	void on_add_clicked();
	void on_learn_clicked();
	void on_apply_clicked();
	void on_binding_learned(const MidiPortBinding &binding);
	void on_learn_cancelled();
	void on_panel_expand(int index);
	void on_panel_remove(int index);

	// Output tab actions
	void on_add_output_clicked();
	void on_output_expand(int index);
	void on_output_remove(int index);

	// Preview & monitor
	void on_raw_midi(int device, int status, int data1, int data2);
	void toggle_monitor(bool expanded);

	// Data
	QString m_port_id;
	QString m_display_name;
	int m_map_mode;
	double m_default_out_min, m_default_out_max;
	QStringList m_combo_items;
	MidiAdapter *m_adapter;
	QStringList m_cached_in_devices;
	QStringList m_cached_out_devices;
	bool m_dirty = false;

	// UI
	MasterPreview *m_master_preview = nullptr;
	QLabel *m_status_label = nullptr;
	QTabWidget *m_tab_widget = nullptr;

	// Input tab
	QScrollArea *m_scroll_area = nullptr;
	QWidget *m_panel_container = nullptr;
	QVBoxLayout *m_panel_layout = nullptr;
	QVector<BindingPanel *> m_panels;
	int m_active_panel = -1;

	// Output tab
	QScrollArea *m_output_scroll = nullptr;
	QWidget *m_output_container = nullptr;
	QVBoxLayout *m_output_layout = nullptr;
	QVector<OutputBindingPanel *> m_output_panels;
	int m_active_output = -1;

	// Buttons
	QPushButton *m_add_btn = nullptr;
	QPushButton *m_learn_btn = nullptr;
	QPushButton *m_apply_btn = nullptr;
	QPushButton *m_add_output_btn = nullptr;

	// MIDI Monitor
	QPushButton *m_monitor_toggle = nullptr;
	QWidget *m_monitor_container = nullptr;
	QPlainTextEdit *m_monitor_log = nullptr;
	int m_monitor_msg_count = 0;

	// Preview convergence
	QTimer *m_preview_timer = nullptr;
	int m_last_raw = 0;
	void on_preview_tick();
	void refresh_preview();

	// Pipeline Visualizer
	QPushButton *m_pipeline_btn = nullptr;
	QPointer<PipelineVisualDialog> m_pipeline_visual;
};

} // namespace super
