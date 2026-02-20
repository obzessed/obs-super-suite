#pragma once

// ============================================================================
// SMixerFilterControls — Inline property editor for an OBS filter
//
// Reads the filter's obs_properties_t and dynamically builds compact
// controls (sliders, spinboxes, checkboxes, combos, color swatches, etc.)
// inside a dark-themed panel. Designed to sit inline in the effects rack
// as an accordion body beneath the filter row header.
// ============================================================================

#include <QWidget>
#include <QTimer>
#include <QFormLayout>
#include <QFormLayout>
#include <QList>
#include <QMouseEvent>
#include <obs.hpp>
#include "s_mixer_filter_property_widget.hpp"

namespace super {

class SMixerFilterControls : public QWidget {
	Q_OBJECT

public:
	explicit SMixerFilterControls(obs_source_t *filter, QWidget *parent = nullptr);
	~SMixerFilterControls();

	void rebuild();

signals:
	void heightChanged();

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	void applySettings();

	static void onFilterUpdated(void *data, calldata_t *cd);
	static void onFilterDestroyed(void *data, calldata_t *cd);

	obs_weak_source_t *m_weak_filter = nullptr;
	OBSData m_settings;
	QWidget *m_content = nullptr;
	QList<SMixerFilterPropertyWidget*> m_widgets;
	obs_properties_t *m_props = nullptr;
};

} // namespace super
