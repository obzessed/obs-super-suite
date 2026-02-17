#pragma once

// ============================================================================
// SMixerBusSelector — Output bus / routing destination selector
//
// A button that shows the current output bus assignment (e.g. "Master",
// "Piano Bus", etc.). Click opens source properties or a routing menu.
// ============================================================================

#include <QWidget>
#include <QPushButton>
#include <obs.hpp>

namespace super {

class SMixerPropsSelector : public QWidget {
	Q_OBJECT

public:
	explicit SMixerPropsSelector(QWidget *parent = nullptr);

	// --- Source Binding ---
	void setSource(obs_source_t *source);
signals:
	void filtersClicked();
	void propertiedClicked();

private:
	void setupUi();

	QPushButton *m_btn_filters = nullptr;
	QPushButton *m_btn_properties = nullptr;
	obs_source_t *m_source = nullptr;
};

} // namespace super
