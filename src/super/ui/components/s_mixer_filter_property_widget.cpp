#include "s_mixer_filter_property_widget.hpp"
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QColorDialog>
#include <QLabel>
#include <QKeyEvent>
#include <QDesktopServices>
#include <QUrl>
#include <obs-frontend-api.h>

namespace super {

// ============================================================================
// Custom Number Drag Filter
// ============================================================================

namespace {
class SMixerNumberDragFilter : public QObject {
public:
	static SMixerNumberDragFilter* instance() {
		static SMixerNumberDragFilter filter;
		return &filter;
	}

	bool eventFilter(QObject *obj, QEvent *event) override {
		QWidget *widget = qobject_cast<QWidget*>(obj);
		if (!widget) return false;

		QAbstractSpinBox *spin = qobject_cast<QAbstractSpinBox*>(widget);
		if (!spin) {
			if (auto *le = qobject_cast<QLineEdit*>(widget)) {
				spin = qobject_cast<QAbstractSpinBox*>(le->parentWidget());
			}
		}
		if (!spin) return false;

		auto *iSpin = qobject_cast<QSpinBox*>(spin);
		auto *dSpin = qobject_cast<QDoubleSpinBox*>(spin);
		if (!iSpin && !dSpin) return false;

		if (event->type() == QEvent::MouseButtonPress) {
			auto *me = static_cast<QMouseEvent*>(event);
			if (me->button() == Qt::LeftButton) {
				if (!spin->hasFocus()) {
					return true; // Consume to avoid focus on single click (relying on double-click instead)
				}
			}
		} else if (event->type() == QEvent::MouseButtonDblClick) {
			auto *me = static_cast<QMouseEvent*>(event);
			if (me->button() == Qt::LeftButton && !spin->hasFocus()) {
				spin->setFocus(Qt::MouseFocusReason);
				if (auto *le = spin->findChild<QLineEdit*>()) le->selectAll();
				return true;
			}
		} else if (event->type() == QEvent::KeyPress) {
			auto *ke = static_cast<QKeyEvent*>(event);
			if (spin->hasFocus() && (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Escape)) {
				spin->clearFocus();
				return true;
			}
		}
		
		// Cursor handling
		if (event->type() == QEvent::Enter && !spin->hasFocus()) {
			// No vertical size cursor since dragging is disabled
		} else if (event->type() == QEvent::Leave) {
			widget->unsetCursor();
		} else if (event->type() == QEvent::FocusIn) {
			widget->setCursor(Qt::IBeamCursor);
		} else if (event->type() == QEvent::FocusOut) {
			widget->unsetCursor();
		}

		return QObject::eventFilter(obj, event);
	}
};

void ApplyCompactSpinBoxStyle(QAbstractSpinBox *spin) {
	spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
	spin->setAlignment(Qt::AlignCenter);
	spin->setKeyboardTracking(false);
	spin->setStyleSheet(
		"QAbstractSpinBox {"
		"  background: rgba(255, 255, 255, 10);"
		"  border: 1px solid transparent;"
		"  border-radius: 4px;"
		"  padding: 0px 4px;"
		"  margin: 0;"
		"  color: #ddd;"
		"  font-size: 10px;"
		"  font-family: 'Segoe UI', sans-serif;"
		"  min-height: 18px;"
		"  max-height: 18px;"
		"}"
		"QAbstractSpinBox:hover {"
		"  background: rgba(255, 255, 255, 20);"
		"  border: 1px solid #555;"
		"}"
		"QAbstractSpinBox:focus {"
		"  background: #111;"
		"  border: 1px solid #00cccc;"
		"  color: #fff;"
		"}"
		"QAbstractSpinBox QLineEdit {"
		"  background: transparent;"
		"  border: none;"
		"  padding: 0;"
		"  margin: 0;"
		"  min-height: 0;"
		"}"
	);
	spin->installEventFilter(SMixerNumberDragFilter::instance());
	if (auto *le = spin->findChild<QLineEdit*>()) {
		le->installEventFilter(SMixerNumberDragFilter::instance());
	}
}

QColor colorFromInt(long long val) {
	return QColor(val & 0xFF, (val >> 8) & 0xFF, (val >> 16) & 0xFF, (val >> 24) & 0xFF);
}

long long colorToInt(QColor color) {
	auto shift = [](unsigned val, int shift) { return ((long long)(unsigned)val << shift); };
	return shift(color.red(), 0) | shift(color.green(), 8) | shift(color.blue(), 16) | shift(color.alpha(), 24);
}
} // namespace


// ============================================================================
// Base Class: SMixerFilterPropertyWidget
// ============================================================================

SMixerFilterPropertyWidget::SMixerFilterPropertyWidget(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QWidget *parent)
	: QWidget(parent)
	, m_filter(filter)
	, m_prop(prop)
	, m_settings(settings)
{
	m_name = obs_property_name(prop);
}

void SMixerFilterPropertyWidget::notifyChanged()
{
	if (m_updatingFromSettings) return;
	emit changed();

	if (obs_property_modified(m_prop, m_settings)) {
		emit needsRebuild();
	}
}

// ============================================================================
// Bool Property
// ============================================================================

SMixerFilterPropertyBool::SMixerFilterPropertyBool(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent)
	: SMixerFilterPropertyWidget(filter, prop, settings, parent)
{
	const char *desc = obs_property_description(prop);
	bool val = obs_data_get_bool(m_settings, m_name.c_str());

	m_checkBox = new QCheckBox(QString::fromUtf8(desc), this);
	m_checkBox->setChecked(val);
	m_checkBox->setEnabled(obs_property_enabled(prop));

	connect(m_checkBox, &QCheckBox::toggled, this, [this](bool checked) {
		if (m_updatingFromSettings) return;
		obs_data_set_bool(m_settings, m_name.c_str(), checked);
		notifyChanged();
	});

	layout->addRow(m_checkBox);
}

void SMixerFilterPropertyBool::updateFromSettings()
{
	bool val = obs_data_get_bool(m_settings, m_name.c_str());
	if (m_checkBox->isChecked() != val) {
		m_updatingFromSettings = true;
		m_checkBox->setChecked(val);
		m_updatingFromSettings = false;
	}
	m_checkBox->setEnabled(obs_property_enabled(m_prop));
}

// ============================================================================
// Int Property
// ============================================================================

SMixerFilterPropertyInt::SMixerFilterPropertyInt(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent)
	: SMixerFilterPropertyWidget(filter, prop, settings, parent)
{
	const char *desc = obs_property_description(prop);
	obs_number_type numType = obs_property_int_type(prop);

	int val = (int)obs_data_get_int(m_settings, m_name.c_str());
	int minVal = obs_property_int_min(prop);
	int maxVal = obs_property_int_max(prop);
	int step = obs_property_int_step(prop);
	const char *suffix = obs_property_int_suffix(prop);

	m_spinBox = new QSpinBox(this);
	ApplyCompactSpinBoxStyle(m_spinBox);
	m_spinBox->setMinimum(minVal);
	m_spinBox->setMaximum(maxVal);
	m_spinBox->setSingleStep(step);
	m_spinBox->setValue(val);
	m_spinBox->setEnabled(obs_property_enabled(prop));
	if (suffix && *suffix)
		m_spinBox->setSuffix(QString::fromUtf8(suffix));

	if (numType == OBS_NUMBER_SLIDER) {
		auto *row = new QWidget(this);
		auto *h = new QHBoxLayout(row);
		h->setContentsMargins(0, 0, 0, 0);
		h->setSpacing(4);

		m_slider = new QSlider(Qt::Horizontal, row);
		m_slider->setMinimum(minVal);
		m_slider->setMaximum(maxVal);
		m_slider->setPageStep(step);
		m_slider->setValue(val);
		m_slider->setEnabled(obs_property_enabled(prop));

		connect(m_slider, &QSlider::valueChanged, m_spinBox, &QSpinBox::setValue);
		connect(m_spinBox, &QSpinBox::valueChanged, m_slider, &QSlider::setValue);

		h->addWidget(m_slider, 1);
		h->addWidget(m_spinBox);

		auto *label = new QLabel(QString::fromUtf8(desc), this);
		layout->addRow(label, row);
	} else {
		auto *label = new QLabel(QString::fromUtf8(desc), this);
		layout->addRow(label, m_spinBox);
	}

	connect(m_spinBox, &QSpinBox::valueChanged, this, [this](int v) {
		if (m_updatingFromSettings) return;
		obs_data_set_int(m_settings, m_name.c_str(), v);
		notifyChanged();
	});
}

void SMixerFilterPropertyInt::updateFromSettings()
{
	int val = (int)obs_data_get_int(m_settings, m_name.c_str());
	if (m_spinBox->value() != val) {
		m_updatingFromSettings = true;
		m_spinBox->setValue(val);
		m_updatingFromSettings = false;
	}
	bool enabled = obs_property_enabled(m_prop);
	m_spinBox->setEnabled(enabled);
	if (m_slider) m_slider->setEnabled(enabled);
}

// ============================================================================
// Float Property
// ============================================================================

SMixerFilterPropertyFloat::SMixerFilterPropertyFloat(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent)
	: SMixerFilterPropertyWidget(filter, prop, settings, parent)
{
	const char *desc = obs_property_description(prop);
	obs_number_type numType = obs_property_float_type(prop);

	double val = obs_data_get_double(m_settings, m_name.c_str());
	double minVal = obs_property_float_min(prop);
	double maxVal = obs_property_float_max(prop);
	m_step = obs_property_float_step(prop);
	const char *suffix = obs_property_float_suffix(prop);

	m_spinBox = new QDoubleSpinBox(this);
	ApplyCompactSpinBoxStyle(m_spinBox);
	m_spinBox->setMinimum(minVal);
	m_spinBox->setMaximum(maxVal);
	m_spinBox->setSingleStep(m_step);
	m_spinBox->setValue(val);
	m_spinBox->setEnabled(obs_property_enabled(prop));
	if (suffix && *suffix)
		m_spinBox->setSuffix(QString::fromUtf8(suffix));

	if (m_step < 1.0) {
		int decimals = std::min<int>((int)(log10(1.0 / m_step) + 0.99), 8);
		if (decimals > m_spinBox->decimals())
			m_spinBox->setDecimals(decimals);
	}

	if (numType == OBS_NUMBER_SLIDER) {
		auto *row = new QWidget(this);
		auto *h = new QHBoxLayout(row);
		h->setContentsMargins(0, 0, 0, 0);
		h->setSpacing(4);

		int sliderMin = (int)(minVal / m_step);
		int sliderMax = (int)(maxVal / m_step);
		int sliderVal = (int)(val / m_step);

		m_slider = new QSlider(Qt::Horizontal, row);
		m_slider->setMinimum(sliderMin);
		m_slider->setMaximum(sliderMax);
		m_slider->setValue(sliderVal);
		m_slider->setEnabled(obs_property_enabled(prop));

		connect(m_slider, &QSlider::valueChanged, m_spinBox, [this](int v) {
			if (!m_updatingFromSettings) m_spinBox->setValue(v * m_step);
		});
		connect(m_spinBox, &QDoubleSpinBox::valueChanged, m_slider, [this](double v) {
			if (!m_updatingFromSettings) m_slider->setValue((int)(v / m_step));
		});

		h->addWidget(m_slider, 1);
		h->addWidget(m_spinBox);

		auto *label = new QLabel(QString::fromUtf8(desc), this);
		layout->addRow(label, row);
	} else {
		auto *label = new QLabel(QString::fromUtf8(desc), this);
		layout->addRow(label, m_spinBox);
	}

	connect(m_spinBox, &QDoubleSpinBox::valueChanged, this, [this](double v) {
		if (m_updatingFromSettings) return;
		obs_data_set_double(m_settings, m_name.c_str(), v);
		notifyChanged();
	});
}

void SMixerFilterPropertyFloat::updateFromSettings()
{
	double val = obs_data_get_double(m_settings, m_name.c_str());
	if (!qFuzzyCompare(m_spinBox->value(), val)) {
		m_updatingFromSettings = true;
		m_spinBox->setValue(val);
		if (m_slider) m_slider->setValue((int)(val / m_step));
		m_updatingFromSettings = false;
	}
	bool enabled = obs_property_enabled(m_prop);
	m_spinBox->setEnabled(enabled);
	if (m_slider) m_slider->setEnabled(enabled);
}

// ============================================================================
// Text Property
// ============================================================================

SMixerFilterPropertyText::SMixerFilterPropertyText(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent)
	: SMixerFilterPropertyWidget(filter, prop, settings, parent)
{
	const char *desc = obs_property_description(prop);
	const char *val = obs_data_get_string(m_settings, m_name.c_str());
	obs_text_type textType = obs_property_text_type(prop);

	m_lineEdit = new QLineEdit(this);
	m_lineEdit->setText(QString::fromUtf8(val));
	m_lineEdit->setEnabled(obs_property_enabled(prop));

	if (textType == OBS_TEXT_PASSWORD)
		m_lineEdit->setEchoMode(QLineEdit::Password);

	auto *label = new QLabel(QString::fromUtf8(desc), this);
	layout->addRow(label, m_lineEdit);

	connect(m_lineEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
		if (m_updatingFromSettings) return;
		obs_data_set_string(m_settings, m_name.c_str(), text.toUtf8().constData());
		notifyChanged();
	});
}

void SMixerFilterPropertyText::updateFromSettings()
{
	QString val = QString::fromUtf8(obs_data_get_string(m_settings, m_name.c_str()));
	if (m_lineEdit->text() != val) {
		m_updatingFromSettings = true;
		m_lineEdit->setText(val);
		m_updatingFromSettings = false;
	}
	m_lineEdit->setEnabled(obs_property_enabled(m_prop));
}

// ============================================================================
// List Property
// ============================================================================

SMixerFilterPropertyList::SMixerFilterPropertyList(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent)
	: SMixerFilterPropertyWidget(filter, prop, settings, parent)
{
	const char *desc = obs_property_description(prop);
	m_format = obs_property_list_format(prop);
	size_t count = obs_property_list_item_count(prop);

	m_comboBox = new QComboBox(this);
	m_comboBox->setEnabled(obs_property_enabled(prop));
	m_comboBox->setMaxVisibleItems(20);
	m_comboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	m_comboBox->setMinimumContentsLength(0);
	m_comboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	for (size_t i = 0; i < count; i++) {
		const char *itemName = obs_property_list_item_name(prop, i);
		QVariant itemData;

		switch (m_format) {
		case OBS_COMBO_FORMAT_INT:
			itemData = QVariant::fromValue<long long>(obs_property_list_item_int(prop, i));
			break;
		case OBS_COMBO_FORMAT_FLOAT:
			itemData = QVariant::fromValue<double>(obs_property_list_item_float(prop, i));
			break;
		case OBS_COMBO_FORMAT_STRING:
			itemData = QByteArray(obs_property_list_item_string(prop, i));
			break;
		case OBS_COMBO_FORMAT_BOOL:
			itemData = QVariant::fromValue<bool>(obs_property_list_item_bool(prop, i));
			break;
		default:
			break;
		}
		m_comboBox->addItem(QString::fromUtf8(itemName), itemData);
	}

	auto *label = new QLabel(QString::fromUtf8(desc), this);
	layout->addRow(label, m_comboBox);

	updateFromSettings(); // Init current value

	connect(m_comboBox, &QComboBox::currentIndexChanged, this, [this](int idx) {
		if (m_updatingFromSettings || idx < 0) return;
		QVariant data = m_comboBox->itemData(idx);
		switch (m_format) {
		case OBS_COMBO_FORMAT_INT:
			obs_data_set_int(m_settings, m_name.c_str(), data.toLongLong());
			break;
		case OBS_COMBO_FORMAT_FLOAT:
			obs_data_set_double(m_settings, m_name.c_str(), data.toDouble());
			break;
		case OBS_COMBO_FORMAT_STRING:
			obs_data_set_string(m_settings, m_name.c_str(), data.toByteArray().constData());
			break;
		case OBS_COMBO_FORMAT_BOOL:
			obs_data_set_bool(m_settings, m_name.c_str(), data.toBool());
			break;
		default:
			break;
		}
		notifyChanged();
	});
}

void SMixerFilterPropertyList::updateFromSettings()
{
	QVariant currentVal;
	switch (m_format) {
	case OBS_COMBO_FORMAT_INT:
		currentVal = QVariant::fromValue<long long>(obs_data_get_int(m_settings, m_name.c_str()));
		break;
	case OBS_COMBO_FORMAT_FLOAT:
		currentVal = QVariant::fromValue<double>(obs_data_get_double(m_settings, m_name.c_str()));
		break;
	case OBS_COMBO_FORMAT_STRING:
		currentVal = QByteArray(obs_data_get_string(m_settings, m_name.c_str()));
		break;
	case OBS_COMBO_FORMAT_BOOL:
		currentVal = QVariant::fromValue<bool>(obs_data_get_bool(m_settings, m_name.c_str()));
		break;
	default:
		break;
	}

	int selectedIdx = m_comboBox->findData(currentVal);
	if (selectedIdx >= 0 && m_comboBox->currentIndex() != selectedIdx) {
		m_updatingFromSettings = true;
		m_comboBox->setCurrentIndex(selectedIdx);
		m_updatingFromSettings = false;
	}
	m_comboBox->setEnabled(obs_property_enabled(m_prop));
}

// ============================================================================
// Color Property
// ============================================================================

SMixerFilterPropertyColor::SMixerFilterPropertyColor(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, bool alpha, QWidget *parent)
	: SMixerFilterPropertyWidget(filter, prop, settings, parent)
	, m_alpha(alpha)
{
	const char *desc = obs_property_description(prop);

	m_swatch = new QPushButton(this);
	m_swatch->setFixedSize(40, 18);
	m_swatch->setCursor(Qt::PointingHandCursor);

	auto *label = new QLabel(QString::fromUtf8(desc), this);
	layout->addRow(label, m_swatch);

	updateFromSettings(); // Sets the initial color styling

	connect(m_swatch, &QPushButton::clicked, this, [this]() {
		if (m_updatingFromSettings) return;

		long long cur = obs_data_get_int(m_settings, m_name.c_str());
		QColor c = colorFromInt(cur);

		QColorDialog::ColorDialogOptions opts;
		if (m_alpha) opts |= QColorDialog::ShowAlphaChannel;

		c = QColorDialog::getColor(c, this, "Select Color", opts);
		if (!c.isValid()) return;

		if (!m_alpha) c.setAlpha(255);
		obs_data_set_int(m_settings, m_name.c_str(), colorToInt(c));
		
		updateFromSettings(); // Re-apply style
		notifyChanged();
	});
}

void SMixerFilterPropertyColor::updateFromSettings()
{
	long long val = obs_data_get_int(m_settings, m_name.c_str());
	QColor color = colorFromInt(val);
	if (!m_alpha) color.setAlpha(255);

	m_swatch->setStyleSheet(QString(
		"QPushButton { background: %1; border: 1px solid #555; border-radius: 3px; }"
		"QPushButton:hover { border-color: #00cccc; }"
	).arg(color.name(m_alpha ? QColor::HexArgb : QColor::HexRgb)));
	m_swatch->setEnabled(obs_property_enabled(m_prop));
}

// ============================================================================
// Button Property
// ============================================================================

SMixerFilterPropertyButton::SMixerFilterPropertyButton(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent)
	: SMixerFilterPropertyWidget(filter, prop, settings, parent)
{
	const char *desc = obs_property_description(prop);

	m_button = new QPushButton(QString::fromUtf8(desc), this);
	m_button->setEnabled(obs_property_enabled(prop));
	m_button->setStyleSheet(
		"QPushButton {"
		"  background: #2a2a2a; color: #ccc; border: 1px solid #444;"
		"  border-radius: 3px; padding: 2px 8px; font-size: 10px;"
		"  min-height: 20px; margin: 0px;"
		"}"
		"QPushButton:hover { background: #333; border-color: #555; }"
		"QPushButton:pressed { background: #222; }"
		"QPushButton:disabled { color: #555; border-color: #333; }"
	);

	connect(m_button, &QPushButton::clicked, this, [this]() {
		obs_button_type type = obs_property_button_type(m_prop);
		const char *savedUrl = obs_property_button_url(m_prop);

		if (type == OBS_BUTTON_URL && savedUrl && savedUrl[0] != '\0') {
			QUrl url(QString::fromUtf8(savedUrl), QUrl::StrictMode);
			if (url.isValid() && (url.scheme() == "http" || url.scheme() == "https")) {
				QDesktopServices::openUrl(url);
			}
			return;
		}

		if (obs_property_button_clicked(m_prop, m_filter)) {
			emit needsRebuild();
		}
	});

	layout->addRow(m_button);
}

// ============================================================================
// Group Property
// ============================================================================

SMixerFilterPropertyGroup::SMixerFilterPropertyGroup(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent)
	: SMixerFilterPropertyWidget(filter, prop, settings, parent)
{
	const char *desc = obs_property_description(prop);
	obs_group_type type = obs_property_group_type(prop);

	m_group = new QGroupBox(QString::fromUtf8(desc), this);
	m_group->setCheckable(type == OBS_GROUP_CHECKABLE);

	auto *subLayout = new QFormLayout(m_group);
	subLayout->setContentsMargins(8, 4, 4, 4);
	subLayout->setSpacing(4);
	subLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

	layout->setWidget(layout->rowCount(), QFormLayout::SpanningRole, m_group);

	updateFromSettings(); // Set group check initially

	if (m_group->isCheckable()) {
		connect(m_group, &QGroupBox::toggled, this, [this](bool checked) {
			if (m_updatingFromSettings) return;
			obs_data_set_bool(m_settings, m_name.c_str(), checked);
			notifyChanged();
		});
	}

	obs_properties_t *content = obs_property_group_content(prop);
	obs_property_t *el = obs_properties_first(content);
	while (el) {
		if (obs_property_visible(el)) {
			auto *w = createPropertyWidget(m_filter, el, m_settings, subLayout, this);
			if (w) {
				m_children.append(w);
				connect(w, &SMixerFilterPropertyWidget::changed, this, &SMixerFilterPropertyWidget::changed);
				connect(w, &SMixerFilterPropertyWidget::needsRebuild, this, &SMixerFilterPropertyWidget::needsRebuild);
			}
		}
		obs_property_next(&el);
	}
}

void SMixerFilterPropertyGroup::updateFromSettings()
{
	if (m_group->isCheckable()) {
		bool val = obs_data_get_bool(m_settings, m_name.c_str());
		if (m_group->isChecked() != val) {
			m_updatingFromSettings = true;
			m_group->setChecked(val);
			m_updatingFromSettings = false;
		}
	} else {
		m_group->setChecked(true);
	}
	m_group->setEnabled(obs_property_enabled(m_prop));

	for (auto *w : m_children) {
		w->updateFromSettings();
	}
}

// ============================================================================
// Fallback Property (Complex Types)
// ============================================================================

SMixerFilterPropertyFallback::SMixerFilterPropertyFallback(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent)
	: SMixerFilterPropertyWidget(filter, prop, settings, parent)
{
	m_button = new QPushButton("Open Properties...", this);
	m_button->setStyleSheet(
		"QPushButton {"
		"  background: #2a2a2a; color: #ccc; border: 1px solid #444;"
		"  border-radius: 3px; padding: 2px 8px; font-size: 10px;"
		"  min-height: 20px; margin: 0px;"
		"  font-family: 'Segoe UI', sans-serif;"
		"}"
		"QPushButton:hover { background: #333; border-color: #555; }"
		"QPushButton:pressed { background: #222; }"
		"QPushButton:disabled { color: #555; border-color: #333; }"
	);

	connect(m_button, &QPushButton::clicked, this, [this]() {
		if (m_filter)
			obs_frontend_open_source_properties(m_filter);
	});

	layout->addRow(m_button);
}

// ============================================================================
// Factory
// ============================================================================

SMixerFilterPropertyWidget* createPropertyWidget(obs_source_t *filter, obs_property_t *prop, OBSData &settings, QFormLayout *layout, QWidget *parent)
{
	obs_property_type type = obs_property_get_type(prop);

	switch (type) {
	case OBS_PROPERTY_BOOL:
		return new SMixerFilterPropertyBool(filter, prop, settings, layout, parent);
	case OBS_PROPERTY_INT:
		return new SMixerFilterPropertyInt(filter, prop, settings, layout, parent);
	case OBS_PROPERTY_FLOAT:
		return new SMixerFilterPropertyFloat(filter, prop, settings, layout, parent);
	case OBS_PROPERTY_TEXT:
	{
		obs_text_type textType = obs_property_text_type(prop);
		if (textType == OBS_TEXT_MULTILINE || textType == OBS_TEXT_INFO)
			return new SMixerFilterPropertyFallback(filter, prop, settings, layout, parent);
		return new SMixerFilterPropertyText(filter, prop, settings, layout, parent);
	}
	case OBS_PROPERTY_LIST:
		return new SMixerFilterPropertyList(filter, prop, settings, layout, parent);
	case OBS_PROPERTY_COLOR:
		return new SMixerFilterPropertyColor(filter, prop, settings, layout, false, parent);
	case OBS_PROPERTY_COLOR_ALPHA:
		return new SMixerFilterPropertyColor(filter, prop, settings, layout, true, parent);
	case OBS_PROPERTY_BUTTON:
		return new SMixerFilterPropertyButton(filter, prop, settings, layout, parent);
	case OBS_PROPERTY_GROUP:
		return new SMixerFilterPropertyGroup(filter, prop, settings, layout, parent);
	case OBS_PROPERTY_INVALID:
		return nullptr;
	default:
		return new SMixerFilterPropertyFallback(filter, prop, settings, layout, parent);
	}
}

} // namespace super
