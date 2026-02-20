#include "s_mixer_filter_controls.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QColorDialog>
#include <QGroupBox>
#include <QFrame>
#include <QWidget>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <obs-frontend-api.h>

namespace super {

// ============================================================================
// Styling Constants
// ============================================================================

static const char *kControlsStyleSheet =
	"QLabel { color: #999; font-size: 10px; font-family: 'Segoe UI', sans-serif; border: none; }"
	"QLineEdit {"
	"  background: #1a1a1a; color: #ddd; border: 1px solid #333;"
	"  border-radius: 3px; padding: 2px 4px; font-size: 10px;"
	"  font-family: 'Segoe UI', sans-serif; min-height: 18px;"
	"}"
	"QComboBox {"
	"  background: #1a1a1a; color: #ddd; border: 1px solid #333;"
	"  border-radius: 3px; padding: 2px 4px; font-size: 10px;"
	"  font-family: 'Segoe UI', sans-serif; min-height: 18px;"
	"}"
	"QComboBox::drop-down { border: none; width: 16px; }"
	"QComboBox QAbstractItemView {"
	"  background: #2a2a2a; color: #ddd; border: 1px solid #444;"
	"  selection-background-color: #00e5ff; selection-color: #111;"
	"}"
	"QSlider::groove:horizontal {"
	"  height: 4px; background: #333; border-radius: 2px;"
	"}"
	"QSlider::handle:horizontal {"
	"  width: 10px; height: 10px; margin: -3px 0;"
	"  background: #00cccc; border-radius: 5px;"
	"}"
	"QSlider::sub-page:horizontal {"
	"  background: #00cccc; border-radius: 2px;"
	"}"
	"QCheckBox { color: #ddd; font-size: 10px; font-family: 'Segoe UI', sans-serif; }"
	"QCheckBox::indicator { width: 12px; height: 12px; }"
	"QPushButton {"
	"  background: #333; color: #ddd; border: 1px solid #444;"
	"  border-radius: 3px; padding: 3px 8px; font-size: 10px;"
	"  font-family: 'Segoe UI', sans-serif; min-height: 18px;"
	"}"
	"QPushButton:hover { background: #444; }"
	"QPushButton:pressed { background: #00cccc; color: #111; }"
	"QGroupBox {"
	"  color: #aaa; font-size: 10px; font-weight: bold;"
	"  border: 1px solid #333; border-radius: 3px;"
	"  margin-top: 6px; padding-top: 10px;"
	"}"
	"QGroupBox::title {"
	"  subcontrol-origin: margin; left: 8px; padding: 0 3px;"
	"}";

// ============================================================================
// Signal Handlers
// ============================================================================

void SMixerFilterControls::onFilterUpdated(void *data, calldata_t *)
{
	auto *controls = static_cast<SMixerFilterControls*>(data);
	QMetaObject::invokeMethod(controls, [controls]() {
		for (auto *w : controls->m_widgets) {
			w->updateFromSettings();
		}
	}, Qt::QueuedConnection);
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

SMixerFilterControls::SMixerFilterControls(obs_source_t *filter, QWidget *parent)
	: QWidget(parent)
{
	setStyleSheet(kControlsStyleSheet);

	auto *outerLayout = new QVBoxLayout(this);
	outerLayout->setContentsMargins(0, 0, 0, 0);
	outerLayout->setSpacing(0);

	if (filter) {
		m_weak_filter = obs_source_get_weak_source(filter);

		// Get current settings
		m_settings = OBSData(obs_source_get_settings(filter));

		signal_handler_t *sh = obs_source_get_signal_handler(filter);
		signal_handler_connect(sh, "update", onFilterUpdated, this);
		signal_handler_connect(sh, "destroy", onFilterDestroyed, this);
	}

	rebuild();
}

SMixerFilterControls::~SMixerFilterControls()
{
	if (m_weak_filter) {
		obs_source_t *filter = obs_weak_source_get_source(m_weak_filter);
		if (filter) {
			signal_handler_t *sh = obs_source_get_signal_handler(filter);
			signal_handler_disconnect(sh, "update", onFilterUpdated, this);
			signal_handler_disconnect(sh, "destroy", onFilterDestroyed, this);
			obs_source_release(filter);
		}
		obs_weak_source_release(m_weak_filter);
	}

	if (m_props) {
		obs_properties_destroy(m_props);
	}
}

void SMixerFilterControls::onFilterDestroyed(void *data, calldata_t *)
{
	auto *controls = static_cast<SMixerFilterControls*>(data);
	QMetaObject::invokeMethod(controls, [controls]() {
		if (controls->m_weak_filter) {
			obs_weak_source_release(controls->m_weak_filter);
			controls->m_weak_filter = nullptr;
		}
		// Optionally trigger rebuild or hide
		controls->rebuild();
	}, Qt::QueuedConnection);
}

// ============================================================================
// Build UI from Properties
// ============================================================================

void SMixerFilterControls::rebuild()
{
	if (m_content) {
		m_content->deleteLater();
		m_content = nullptr;
	}

	for (auto *w : m_widgets) {
		w->deleteLater();
	}
	m_widgets.clear();

	if (m_props) {
		obs_properties_destroy(m_props);
		m_props = nullptr;
	}

	m_content = new QWidget(this);
	m_content->setObjectName("filterControlsBody");
	m_content->setFixedWidth(width() > 0 ? width() : parentWidget() ? parentWidget()->width() : 200);

	auto *formLayout = new QFormLayout(m_content);
	formLayout->setContentsMargins(2, 2, 2, 2); // we have 4px outside
	formLayout->setSpacing(4);
	formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

	obs_source_t *filter = nullptr;
	if (m_weak_filter) {
		filter = obs_weak_source_get_source(m_weak_filter);
	}

	if (!filter) {
		auto *lbl = new QLabel("Filter destroyed", m_content);
		lbl->setAlignment(Qt::AlignCenter);
		lbl->setStyleSheet("color: #555; font-style: italic; font-size: 10px; padding: 8px;");
		formLayout->addRow(lbl);
	} else {
		m_props = obs_source_properties(filter);
		if (!m_props) {
			auto *lbl = new QLabel("No properties", m_content);
			lbl->setAlignment(Qt::AlignCenter);
			lbl->setStyleSheet("color: #555; font-style: italic; font-size: 10px; padding: 8px;");
			formLayout->addRow(lbl);
		} else {
			obs_property_t *prop = obs_properties_first(m_props);
			bool hasAny = false;
			while (prop) {
				if (obs_property_visible(prop)) {
					if (auto *w = createPropertyWidget(filter, prop, m_settings, formLayout, m_content)) {
						m_widgets.append(w);
						connect(w, &SMixerFilterPropertyWidget::changed, this, &SMixerFilterControls::applySettings);
						connect(w, &SMixerFilterPropertyWidget::needsRebuild, this, &SMixerFilterControls::rebuild);
					}
					hasAny = true;
				}
				obs_property_next(&prop);
			}

			if (!hasAny) {
				auto *lbl = new QLabel("No visible properties", m_content);
				lbl->setAlignment(Qt::AlignCenter);
				lbl->setStyleSheet("color: #555; font-style: italic; font-size: 10px; padding: 8px;");
				formLayout->addRow(lbl);
			}
		}
		obs_source_release(filter);
	}

	layout()->addWidget(m_content);

	// Notify parent that our height may have changed
	QMetaObject::invokeMethod(this, [this]() {
		emit heightChanged();
	}, Qt::QueuedConnection);
}

// ============================================================================
// Settings Application
// ============================================================================

void SMixerFilterControls::applySettings()
{
	if (!m_weak_filter) return;
	obs_source_t *filter = obs_weak_source_get_source(m_weak_filter);
	if (!filter) return;
	
	obs_source_update(filter, m_settings);
	obs_source_release(filter);
}

// ============================================================================
// Event Blocking to Prevent Dragging List Item from empty space
// ============================================================================

void SMixerFilterControls::mousePressEvent(QMouseEvent *event)
{
	event->accept();
}

void SMixerFilterControls::mouseMoveEvent(QMouseEvent *event)
{
	event->accept();
}

void SMixerFilterControls::mouseReleaseEvent(QMouseEvent *event)
{
	event->accept();
}

void SMixerFilterControls::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	if (m_content && event->size().width() > 0) {
		m_content->setFixedWidth(event->size().width());
	}
}

} // namespace super
