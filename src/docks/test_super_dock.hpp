#pragma once

// ============================================================================
// TestSuperDock — Comprehensive showcase dock for the SuperWidget system.
//
// Demonstrates EVERY feature:
//   • Inherits SuperWidget
//   • register_control() → ControlRegistry → ControlPorts
//   • Dual Toolbar (System + rhs User) with custom actions
//   • All control types: Slider, Dial, SpinBox, DoubleSpinBox, ComboBox,
//     CheckBox, Toggle Button, OneShot Button, Trigger Button
//   • Trigger group with sample trigger + toggle button + count
//   • Monitor Console integration (logs all control changes)
//   • ControlFilter pipeline demo (Smoothing, Deadzone, Quantize)
//   • Animation / TweenManager demo
//   • ControlRegistry Snapshots (capture / restore)
//   • Soft Takeover toggle
//   • Modifiers (Shift / Alt layers)
//   • ControlVariables (session + persistent counters)
//   • Activity indicator
//   • Full persistence (save/load state)
// ============================================================================

#include "../super/ui/super_widget.hpp"

#include <QSlider>
#include <QDial>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QToolBar>

#include <memory>

namespace super {
class SmoothingFilter;
class DeadzoneFilter;
class QuantizeFilter;
class ControlVariable;
}

class TestSuperDock : public super::SuperWidget {
	Q_OBJECT

public:
	explicit TestSuperDock(QWidget *parent = nullptr);
	~TestSuperDock() override = default;

	QJsonObject save_state() const override;
	void load_state(const QJsonObject &state) override;

private:
	void setup_ui();
	void setup_user_toolbar_actions();
	void setup_rhs_toolbar();
	void update_labels();
	void on_oneshot_fired();
	void on_sample_trigger_fired();
	void refresh_variable_display();
	void refresh_modifier_display();

	// -- Sliders --
	QSlider *m_volume_slider = nullptr;
	QSlider *m_pan_slider = nullptr;
	QLabel *m_volume_label = nullptr;
	QLabel *m_pan_label = nullptr;

	// -- Dial --
	QDial *m_send_dial = nullptr;
	QLabel *m_send_label = nullptr;

	// -- SpinBoxes --
	QSpinBox *m_delay_spin = nullptr;
	QDoubleSpinBox *m_gain_spin = nullptr;

	// -- Combo --
	QComboBox *m_mode_combo = nullptr;

	// -- Toggles --
	QCheckBox *m_solo_check = nullptr;
	QPushButton *m_mute_btn = nullptr;   // Toggle button
	QPushButton *m_rec_btn = nullptr;    // Toggle button

	// -- Triggers --
	QPushButton *m_oneshot_btn = nullptr;       // OneShot (non-checkable)
	QLabel *m_oneshot_counter = nullptr;
	int m_oneshot_count = 0;

	QPushButton *m_sample_trigger_btn = nullptr; // Trigger (non-checkable)
	QLabel *m_sample_trigger_counter = nullptr;
	int m_sample_trigger_count = 0;

	QPushButton *m_arm_btn = nullptr;            // Toggle button in triggers group

	// -- Activity --
	QProgressBar *m_activity_bar = nullptr;

	// -- Filter demos --
	QCheckBox *m_smooth_check = nullptr;
	QCheckBox *m_deadzone_check = nullptr;
	QCheckBox *m_quantize_check = nullptr;
	std::shared_ptr<super::SmoothingFilter> m_smooth_filter;
	std::shared_ptr<super::DeadzoneFilter> m_deadzone_filter;
	std::shared_ptr<super::QuantizeFilter> m_quantize_filter;

	// -- Animation --
	int m_current_tween_handle = -1;

	// -- Snapshot --
	QJsonObject m_saved_snapshot;

	// -- Modifiers --
	QPushButton *m_shift_btn = nullptr;
	QPushButton *m_alt_btn = nullptr;
	QLabel *m_modifier_status = nullptr;

	// -- Variables --
	QLabel *m_session_var_label = nullptr;
	QLabel *m_persist_var_label = nullptr;
	QPushButton *m_session_inc_btn = nullptr;
	QPushButton *m_persist_inc_btn = nullptr;
	QPushButton *m_vars_reset_btn = nullptr;

	// Variable pointers (owned by ControlRegistry)
	super::ControlVariable *m_session_counter = nullptr;
	super::ControlVariable *m_persist_counter = nullptr;

	// -- RHS Toolbar --
	QToolBar *m_rhs_toolbar = nullptr;
};
