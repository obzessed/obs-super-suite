#pragma once

// ============================================================================
// SMixerSidePanel — Expandable side panel container
//
// The expandable right-side panel on a mixer channel that houses:
//   - Effects/Filters rack
//   - Sends/Track routing panel
// Features:
//   - Animated show/hide (or instant toggle)
//   - Clean border separator from main channel strip
//   - Scrollable content if it overflows
// ============================================================================

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <obs.hpp>

namespace super {

class SMixerEffectsRack;
class SMixerSendsPanel;

class SMixerSidePanel : public QWidget {
	Q_OBJECT

public:
	explicit SMixerSidePanel(QWidget *parent = nullptr);

	// --- Source Binding ---
	void setSource(obs_source_t *source);
	void refresh();

	// --- Access ---
	SMixerEffectsRack *effectsRack() const { return m_effects_rack; }
	SMixerSendsPanel *sendsPanel() const { return m_sends_panel; }

private:
	void setupUi();

	SMixerEffectsRack *m_effects_rack = nullptr;
	SMixerSendsPanel *m_sends_panel = nullptr;
};

} // namespace super
