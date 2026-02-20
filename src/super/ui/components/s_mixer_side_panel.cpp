#include "s_mixer_side_panel.hpp"
#include "s_mixer_effects_rack.hpp"
#include "s_mixer_sends_panel.hpp"
#include "../widgets/s_mixer_channel.hpp"

#include <QVBoxLayout>

namespace super {

SMixerSidePanel::SMixerSidePanel(QWidget *parent) : QWidget(parent)
{
	setupUi();
}

void SMixerSidePanel::setupUi()
{
	setFixedWidth(MIXER_CHANNEL_SIDE_PANEL_WIDTH); // Matches the requested side panel width
	setObjectName("sidePanel");

	// FIXME: the left border steals 1px from the content area
	setStyleSheet(
		"#sidePanel { background: #1e1e1e; border-left: 1px solid #333; }"
	);

	// Direct Vertical Layout allows Effects Rack to expand and fill space
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	// Effects Rack (Expands deeply)
	m_effects_rack = new SMixerEffectsRack(this);
	m_effects_rack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	layout->addWidget(m_effects_rack);

	// Thin Separator
	auto *sep = new QWidget(this);
	sep->setFixedHeight(1);
	sep->setStyleSheet("background: #333;");
	layout->addWidget(sep);

	// Sends Panel (Compact, bottom aligned if rack push it, but rack expands so they stack nicely)
	m_sends_panel = new SMixerSendsPanel(this);
	m_sends_panel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum); 
	layout->addWidget(m_sends_panel);
}

void SMixerSidePanel::setSource(obs_source_t *source)
{
	if (m_effects_rack)
		m_effects_rack->setSource(source);
	if (m_sends_panel)
		m_sends_panel->setSource(source);
}

void SMixerSidePanel::refresh()
{
	if (m_effects_rack)
		m_effects_rack->refresh();
	if (m_sends_panel)
		m_sends_panel->refresh();
}

} // namespace super
