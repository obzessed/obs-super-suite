#pragma once

#include <QWidget>
#include <QGroupBox>
#include <QLineEdit>
#include <QFormLayout>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QSlider>
#include <QComboBox>
#include <obs.hpp>
#include <string>

namespace super {

class SMixerFilterPropertyWidget : public QWidget {
	Q_OBJECT

public:
	SMixerFilterPropertyWidget(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QWidget *parent = nullptr);
	virtual ~SMixerFilterPropertyWidget() = default;

	// Called internally by the derived class when the UI element value changes
	void notifyChanged();

	// Called externally (e.g., from obs_source "update" signal) to update the UI
	virtual void updateFromSettings() = 0;

signals:
	void changed();
	void needsRebuild();

protected:
	obs_source_t *m_filter;
	obs_property_t *m_prop;
	OBSData &m_settings;
	std::string m_name;
	bool m_updatingFromSettings = false; // Flag to prevent cyclical updates
};

class SMixerFilterPropertyBool : public SMixerFilterPropertyWidget {
	Q_OBJECT
public:
	SMixerFilterPropertyBool(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent = nullptr);
	void updateFromSettings() override;
private:
	QCheckBox *m_checkBox;
};

class SMixerFilterPropertyInt : public SMixerFilterPropertyWidget {
	Q_OBJECT
public:
	SMixerFilterPropertyInt(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent = nullptr);
	void updateFromSettings() override;
private:
	QSpinBox *m_spinBox;
	QSlider *m_slider = nullptr;
};

class SMixerFilterPropertyFloat : public SMixerFilterPropertyWidget {
	Q_OBJECT
public:
	SMixerFilterPropertyFloat(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent = nullptr);
	void updateFromSettings() override;
private:
	QDoubleSpinBox *m_spinBox;
	QSlider *m_slider = nullptr;
	double m_step = 1.0;
};

class SMixerFilterPropertyText : public SMixerFilterPropertyWidget {
	Q_OBJECT
public:
	SMixerFilterPropertyText(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent = nullptr);
	void updateFromSettings() override;
private:
	QLineEdit *m_lineEdit;
};

class SMixerFilterPropertyList : public SMixerFilterPropertyWidget {
	Q_OBJECT
public:
	SMixerFilterPropertyList(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent = nullptr);
	void updateFromSettings() override;
private:
	QComboBox *m_comboBox;
	obs_combo_format m_format;
};

class SMixerFilterPropertyColor : public SMixerFilterPropertyWidget {
	Q_OBJECT
public:
	SMixerFilterPropertyColor(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, bool alpha, QWidget *parent = nullptr);
	void updateFromSettings() override;
private:
	QPushButton *m_swatch;
	bool m_alpha;
};

class SMixerFilterPropertyButton : public SMixerFilterPropertyWidget {
	Q_OBJECT
public:
	SMixerFilterPropertyButton(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent = nullptr);
	void updateFromSettings() override {}
private:
	QPushButton *m_button;
};

class SMixerFilterPropertyGroup : public SMixerFilterPropertyWidget {
	Q_OBJECT
public:
	SMixerFilterPropertyGroup(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent = nullptr);
	void updateFromSettings() override;
private:
	QGroupBox *m_group;
	QList<SMixerFilterPropertyWidget*> m_children;
};

class SMixerFilterPropertyFallback : public SMixerFilterPropertyWidget {
	Q_OBJECT
public:
	SMixerFilterPropertyFallback(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent = nullptr);
	void updateFromSettings() override {}
private:
	QPushButton *m_button;
};

SMixerFilterPropertyWidget* createPropertyWidget(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent);

} // namespace super
