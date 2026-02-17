#include "s_mixer_props_selector.hpp"

#include <QVBoxLayout>
#include <obs-frontend-api.h>

namespace super {

SMixerPropsSelector::SMixerPropsSelector(QWidget *parent) : QWidget(parent)
{
	setupUi();
}

void SMixerPropsSelector::setupUi()
{
	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);

	m_btn_filters = new QPushButton("F", this);
	m_btn_filters->setFixedHeight(24);
	m_btn_filters->setStyleSheet(
		"QPushButton {"
		"  background: #2b2b2b; color: #aaa;"
		"  border: 1px solid #333; border-radius: 3px;"
		"  font-size: 12px; font-family: 'Segoe UI', sans-serif;"
		"  padding: 0 6px;"
		"}"
		"QPushButton:hover {"
		"  color: #fff; border: 1px solid #555;"
		"  background: #333;"
		"}"
	);

	connect(m_btn_filters, &QPushButton::clicked, this, [this]() {
		if (m_source)
			obs_frontend_open_source_filters(m_source);
		emit filtersClicked();
	});

	layout->addWidget(m_btn_filters);

	m_btn_properties = new QPushButton("P", this);
	m_btn_properties->setFixedHeight(24);
	m_btn_properties->setStyleSheet(
		"QPushButton {"
		"  background: #2b2b2b; color: #aaa;"
		"  border: 1px solid #333; border-radius: 3px;"
		"  font-size: 12px; font-family: 'Segoe UI', sans-serif;"
		"  padding: 0 6px;"
		"}"
		"QPushButton:hover {"
		"  color: #fff; border: 1px solid #555;"
		"  background: #333;"
		"}"
	);

	connect(m_btn_properties, &QPushButton::clicked, this, [this]() {
		if (m_source)
			obs_frontend_open_source_properties(m_source);
		emit propertiedClicked();
	});

	layout->addWidget(m_btn_properties);
}

void SMixerPropsSelector::setSource(obs_source_t *source)
{
	m_source = source;
}

} // namespace super
