#include "s_mixer_sends_panel.hpp"
#include "s_mixer_switch.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QEvent>
#include <QMouseEvent>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <algorithm>

namespace super {

// ============================================================================
// SMixerChevron Implementation
// ============================================================================

SMixerChevron::SMixerChevron(QWidget *parent) : QPushButton(parent) {
	setFixedSize(22, 14);
	setCursor(Qt::PointingHandCursor);
	setToolTip("Collapse");
	// Reset native styling
	setStyleSheet("border: none; background: transparent; padding: 0px; margin: 0px; min-height: 0px;");
}

void SMixerChevron::setExpanded(bool expanded) {
	// Expanded: Points Up (0 deg) - "Click to collapse"
	// Collapsed: Points Down (180 deg) - "Click to expand" (Upside-down relative to Up)
	// Or if "upside-down" means 180 is the "Effect" state?
	// Let's assume Expanded = 0 (Normal Up), Collapsed = 180 (Inverted).
	// Wait, Standard chevron-up points UP.
	// If Expanded, we see content. Button usually collapses. Icon UP is fine.
	// If Collapsed, we don't. Button expands. Icon DOWN (180).

	qreal target = expanded ? 0.0 : 180.0;
	animateTo(target);
}

void SMixerChevron::paintEvent(QPaintEvent *) {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	p.setRenderHint(QPainter::SmoothPixmapTransform);

	// Determine color
	QColor color = QColor("#666");
	if (underMouse()) color = QColor("#fff");
	if (isDown()) color = QColor("#ccc");

	int dim = 12; // Small icon

	p.translate(width() / 2, height() / 2);
	p.rotate(m_angle);
	p.translate(-dim / 2, -dim / 2);

	static const QIcon icon(":/super/assets/icons/super/mixer/chevron-down.svg");

	if (!icon.isNull()) {
		QPixmap pix(dim, dim);
		pix.fill(Qt::transparent);
		QPainter ip(&pix);
		ip.setRenderHint(QPainter::Antialiasing);
		icon.paint(&ip, pix.rect(), Qt::AlignCenter);
		ip.setCompositionMode(QPainter::CompositionMode_SourceIn);
		ip.fillRect(pix.rect(), color);
		ip.end();
		p.drawPixmap(0, 0, pix);
	} else {
		// Fallback text
		p.setPen(color);
		p.drawText(QRect(0, 0, dim, dim), Qt::AlignCenter, "^");
	}
}

void SMixerChevron::animateTo(qreal endAngle) {
	if (qAbs(m_angle - endAngle) < 0.1) return;

	if (!m_anim) {
		m_anim = new QVariantAnimation(this);
		m_anim->setDuration(150);
		m_anim->setEasingCurve(QEasingCurve::OutCubic);
		connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v){
			m_angle = v.toReal();
			update();
		});
	}

	m_anim->stop();
	m_anim->setStartValue(m_angle);
	m_anim->setEndValue(endAngle);
	m_anim->start();
}

// ============================================================================
// SMixerSendsPanel Implementation
// ============================================================================

SMixerSendsPanel::SMixerSendsPanel(QWidget *parent) : QWidget(parent)
{
	setupUi();
}

SMixerSendsPanel::~SMixerSendsPanel()
{
	disconnectSource();
}

void SMixerSendsPanel::setupUi()
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	// Header Container
	auto *headerWidget = new QWidget(this);
	headerWidget->setObjectName("sendsHeaderRow");
	headerWidget->setStyleSheet(
		"#sendsHeaderRow { border-bottom: 1px solid #333; }"
	);
	
	auto *header = new QHBoxLayout(headerWidget);
	header->setContentsMargins(8, 6, 8, 6);
	header->setSpacing(4);

	// Collapse Button (Right)
	m_collapse_btn = new SMixerChevron(headerWidget);
	
	connect(m_collapse_btn, &QPushButton::clicked, this, [this]() {
		setExpanded(!m_is_expanded);
	});

	// Title
	m_header_label = new QLabel("SENDS", headerWidget);
	m_header_label->setStyleSheet(
		"color: #888; font-weight: bold; font-size: 10px;"
		"font-family: 'Segoe UI', sans-serif;"
		"letter-spacing: 1px;"
		"border: none;"
	);
	header->addWidget(m_header_label);

	header->addStretch();

	// Note: disabling it for now, this makes the header look chunky
	header->addWidget(m_collapse_btn);

	headerWidget->installEventFilter(this);

	layout->addWidget(headerWidget);

	// Content Container
	m_content_container = new QWidget(this);
	m_items_layout = new QVBoxLayout(m_content_container);
	m_items_layout->setContentsMargins(0, 2, 0, 2);
	m_items_layout->setSpacing(0);
	
	layout->addWidget(m_content_container);
}

void SMixerSendsPanel::setSource(obs_source_t *source)
{
	if (m_source == source)
		return;

	disconnectSource();
	m_source = source;
	connectSource();
	refresh();
}

void SMixerSendsPanel::connectSource()
{
	if (!m_source) return;

	m_signal_handler = obs_source_get_signal_handler(m_source);
	if (m_signal_handler) {
		signal_handler_connect(m_signal_handler, "audio_mixers", audioMixersChangedCb, this);
	}
}

void SMixerSendsPanel::disconnectSource()
{
	if (m_signal_handler) {
		signal_handler_disconnect(m_signal_handler, "audio_mixers", audioMixersChangedCb, this);
		m_signal_handler = nullptr;
	}
	m_source = nullptr;
}

void SMixerSendsPanel::audioMixersChangedCb(void *data, calldata_t *cd)
{
	auto *self = static_cast<SMixerSendsPanel *>(data);
	int mixers = static_cast<int>(calldata_int(cd, "mixers"));
	QMetaObject::invokeMethod(self, [self, mixers]() {
		self->updateSwitches(static_cast<uint32_t>(mixers));
	});
}

void SMixerSendsPanel::updateSwitches(uint32_t mixers)
{
	for (int i = 0; i < static_cast<int>(m_switches.size()); i++) {
		if (i >= m_track_count) break;
		
		bool active = (mixers & (1 << i)) != 0;
		auto *sw = m_switches[i];
		if (sw && sw->isChecked() != active) {
			sw->setChecked(active, true); // Animate external change
		}
	}
}

void SMixerSendsPanel::clearItems()
{
	m_switches.clear();
	QLayoutItem *item;
	while ((item = m_items_layout->takeAt(0)) != nullptr) {
		if (item->widget())
			delete item->widget();
		delete item;
	}
}

void SMixerSendsPanel::setTrackCount(int count)
{
	m_track_count = std::clamp(count, 1, 64);
	refresh();
}

void SMixerSendsPanel::refresh()
{
	clearItems();

	if (!m_source) {
		auto *lbl = new QLabel("No Source", this);
		lbl->setAlignment(Qt::AlignCenter);
		lbl->setStyleSheet("color: #555; font-style: italic; font-size: 10px; padding: 10px;");
		m_items_layout->addWidget(lbl);
		return;
	}

	uint32_t mixers = obs_source_get_audio_mixers(m_source);

	for (int i = 0; i < m_track_count; i++) {
		int track_num = i + 1;
		
		auto *row = new QWidget(this);
		auto *rowLayout = new QHBoxLayout(row);
		rowLayout->setContentsMargins(8, 3, 8, 3);
		rowLayout->setSpacing(6);

		// Label (Left)
		QLabel *lbl = new QLabel(QString("Track %1").arg(track_num), row);
		lbl->setStyleSheet("border: none; color: #aaa; font-size: 11px; font-family: 'Segoe UI', sans-serif;");
		rowLayout->addWidget(lbl);

		rowLayout->addStretch();

		// Switch (Right)
		auto *sw = new SMixerSwitch(row);
		m_switches.push_back(sw);
		
		// Set initial state (no animation since not visible yet)
		bool state = (mixers & (1 << i)) != 0;
		sw->setChecked(state, false); 
		
		// Connect toggle -> OBS
		connect(sw, &SMixerSwitch::toggled, this, [this, i](bool checked) {
			if (!m_source)
				return;
			uint32_t current = obs_source_get_audio_mixers(m_source);
			uint32_t mask = (1 << i);
			
			// Only update if changed (to double-check)
			bool currentState = (current & mask) != 0;
			if (currentState != checked) {
				if (checked) current |= mask;
				else current &= ~mask;
				obs_source_set_audio_mixers(m_source, current);
				emit trackChanged(i, checked);
			}
		});

		rowLayout->addWidget(sw);

		m_items_layout->addWidget(row);
	}
}

void SMixerSendsPanel::setExpanded(bool expanded)
{
	m_is_expanded = expanded;
	if (m_content_container)
		m_content_container->setVisible(expanded);
		
	if (m_collapse_btn) {
		m_collapse_btn->setToolTip(expanded ? "Collapse" : "Expand");
		m_collapse_btn->setExpanded(expanded);
	}
}

bool SMixerSendsPanel::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::MouseButtonDblClick) {
		setExpanded(!m_is_expanded);
		return true;
	}
	return QWidget::eventFilter(obj, event);
}

} // namespace super
