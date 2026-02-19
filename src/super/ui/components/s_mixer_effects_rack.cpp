#include "s_mixer_effects_rack.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QEvent>
#include <QMouseEvent>
#include <QCheckBox>
#include <QPainter>
#include <QFontMetrics>
#include <QIcon>
#include <QPixmap>
#include <QMenu>
#include <QInputDialog>

#include <QListWidgetItem>
#include <QSignalBlocker>
#include <obs-frontend-api.h>
#include "s_mixer_switch.hpp"

namespace super {

// ============================================================================
// Helpers
// ============================================================================

// Elided Label for truncating long text with "..."
class SMixerElidedLabel : public QWidget {
	QString m_text;
public:
	SMixerElidedLabel(const QString &text, QWidget *parent = nullptr) : QWidget(parent), m_text(text) {
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	}
	void setText(const QString &text) { m_text = text; update(); }

	QSize minimumSizeHint() const override {
		// Critical: Allow shrinking to 0 width so layout isn't forced to expand
		return QSize(0, fontMetrics().height());
	}

	QSize sizeHint() const override {
		return QSize(fontMetrics().horizontalAdvance(m_text), fontMetrics().height());
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter painter(this);
		painter.setPen(palette().color(QPalette::WindowText));
		QFontMetrics metrics(font());
		QString elided = metrics.elidedText(m_text, Qt::ElideRight, width());
		painter.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, elided);
	}
};

// Callback for external filter enable/disable changes
static void obs_filter_enabled_cb(void *data, calldata_t *cd)
{
	auto *sw = static_cast<SMixerSwitch *>(data);
	bool enabled = calldata_bool(cd, "enabled");
	QMetaObject::invokeMethod(sw, [sw, enabled]() {
		// Only update if state differs to avoid feedback loops
		if (sw->isChecked() != enabled) {
			sw->setChecked(enabled, true); // Animate external change
		}
	});
}

// Helper to find filter by name (since obs_source_get_filter_by_name might not exist or be exposed)
static obs_source_t *findFilterByName(obs_source_t *source, const char *name)
{
	struct FindData {
		const char *targetName;
		obs_source_t *foundFilter;
	} data = {name, nullptr};

	obs_source_enum_filters(source, [](obs_source_t *, obs_source_t *filter, void *param) {
		auto *d = static_cast<FindData *>(param);
		if (d->foundFilter) return;

		const char *n = obs_source_get_name(filter);
		if (n && strcmp(n, d->targetName) == 0) {
			d->foundFilter = filter;
		}
	}, &data);

	return data.foundFilter;
}

// Helper to find filter by UUID
static obs_source_t *findFilterByUuid(obs_source_t *source, const char *uuid)
{
	struct FindData {
		const char *targetUuid;
		obs_source_t *foundFilter;
	} data = {uuid, nullptr};

	obs_source_enum_filters(source, [](obs_source_t *, obs_source_t *filter, void *param) {
		auto *d = static_cast<FindData *>(param);
		if (d->foundFilter) return;

		const char *u = obs_source_get_uuid(filter);
		if (u && strcmp(u, d->targetUuid) == 0) {
			d->foundFilter = filter;
		}
	}, &data);

	return data.foundFilter;
}

// Simple button that paints an SVG icon with color tinting on hover/press
class SMixerFilterAddButton : public QPushButton {
public:
	explicit SMixerFilterAddButton(QWidget *parent = nullptr) : QPushButton(parent) {
		setFixedSize(22, 14);
		setCursor(Qt::PointingHandCursor);
		setToolTip("Add Filter");
		// Ensure background doesn't interfere, reset native styling
		setStyleSheet("QPushButton { border: none; background: transparent; padding: 0px; margin: 0px; min-height: 0px; }");
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		p.setRenderHint(QPainter::SmoothPixmapTransform);

		// Determine tint color based on state
		auto color = QColor("#888"); // Default gray
		if (isDown()) color = QColor("#ffffff"); // White on press
		else if (underMouse()) color = QColor("#00e5ff"); // Cyan on hover

		// Load icon (static to avoid reload)
		static const QIcon icon(":/super/assets/icons/super/mixer/fx-add.svg");
		
		int dim = 14; 
		int x = (width() - dim) / 2;
		int y = (height() - dim) / 2;

		if (!icon.isNull()) {
			QPixmap pix(dim, dim);
			pix.fill(Qt::transparent);
			QPainter ip(&pix);
			ip.setRenderHint(QPainter::Antialiasing);
			icon.paint(&ip, pix.rect(), Qt::AlignCenter);
			ip.setCompositionMode(QPainter::CompositionMode_SourceIn);
			ip.fillRect(pix.rect(), color);
			ip.end();
			p.drawPixmap(x, y, pix);
		} else {
			// Fallback text "+"
			p.setPen(color);
			QFont f = font();
			f.setBold(true);
			f.setPixelSize(14);
			p.setFont(f);
			p.drawText(rect(), Qt::AlignCenter, "+");
		}
	}
};

// ============================================================================
// Filter Type Enumeration
// ============================================================================

struct FilterTypeInfo {
	QString id;           // e.g. "noise_gate_filter"
	QString displayName;  // e.g. "Noise Gate"
};

static QList<FilterTypeInfo> getAvailableFilterTypes()
{
	QList<FilterTypeInfo> result;
	
	size_t idx = 0;
	const char *typeId = nullptr;
	while (obs_enum_filter_types(idx++, &typeId)) {
		if (!typeId) continue;
		
		uint32_t flags = obs_get_source_output_flags(typeId);
		
		// Skip disabled/internal
		if (flags & OBS_SOURCE_CAP_DISABLED) continue;
		
		const char *displayName = obs_source_get_display_name(typeId);
		if (!displayName || !*displayName) continue;
		
		result.append({QString::fromUtf8(typeId), QString::fromUtf8(displayName)});
	}
	
	// Sort alphabetically by display name
	std::sort(result.begin(), result.end(), [](const FilterTypeInfo &a, const FilterTypeInfo &b) {
		return a.displayName.compare(b.displayName, Qt::CaseInsensitive) < 0;
	});
	
	return result;
}

static QString generateUniqueFilterName(obs_source_t *source, const QString &baseName)
{
	QString name = baseName;
	int counter = 1;
	
	while (findFilterByName(source, name.toUtf8().constData())) {
		counter++;
		name = QString("%1 %2").arg(baseName).arg(counter);
	}
	
	return name;
}

// ============================================================================
// SMixerEffectsRack Implementation
// ============================================================================

SMixerEffectsRack::SMixerEffectsRack(QWidget *parent) : QWidget(parent)
{
	setupUi();
}

void SMixerEffectsRack::setupUi()
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0); 

	// Header Container
	auto *headerWidget = new QWidget(this);
	headerWidget->setObjectName("fxHeaderRow");
	headerWidget->setStyleSheet(
		"#fxHeaderRow { border-bottom: 1px solid #333; }"
	);
	
	auto *header = new QHBoxLayout(headerWidget);
	header->setContentsMargins(8, 6, 8, 6);
	header->setSpacing(4);

	// Title (Left-aligned for cleaner look)
	m_header_label = new QLabel("EFFECTS", headerWidget);
	m_header_label->setStyleSheet(
		"color: #888; font-weight: bold; font-size: 10px;"
		"font-family: 'Segoe UI', sans-serif;"
		"letter-spacing: 1px;"
		"border: none;"
	);
	header->addWidget(m_header_label);

	header->addStretch();

	// Add Filter Button (Right)
	m_add_btn = new SMixerFilterAddButton(headerWidget);
	connect(m_add_btn, &QPushButton::clicked, this, &SMixerEffectsRack::showAddFilterMenu);
	header->addWidget(m_add_btn);

	layout->addWidget(headerWidget);

	// Items List (Drag & Drop enabled)
	m_list = new QListWidget(this);
	m_list->setFocusPolicy(Qt::NoFocus);
	m_list->setFrameShape(QFrame::NoFrame);
	m_list->setStyleSheet(
		"QListWidget { background: transparent; border: none; outline: none; }"
		"QListWidget::item { border: none; padding: 1px 0px; }"
		"QListWidget::item:selected { background: transparent; }"
		"QListWidget::item:hover { background: rgba(255,255,255,8); }"
	);
	m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	
	// Drag & Drop Config
	m_list->setDragDropMode(QAbstractItemView::InternalMove);
	m_list->setDefaultDropAction(Qt::MoveAction);
	m_list->setSelectionMode(QAbstractItemView::SingleSelection);
	
	connect(m_list->model(), &QAbstractItemModel::rowsMoved, this, &SMixerEffectsRack::onReorder);
	
	// Double-click to open properties
	connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
		if (!m_source || !item) return;

		obs_source_t *filter = nullptr;
		QString uuid = item->data(Qt::UserRole + 1).toString();
		if (!uuid.isEmpty()) {
			filter = findFilterByUuid(m_source, uuid.toUtf8().constData());
		}
		
		if (!filter) {
			QString name = item->data(Qt::UserRole).toString();
			if (!name.isEmpty())
				filter = findFilterByName(m_source, name.toUtf8().constData());
		}

		if (filter) {
			obs_frontend_open_source_properties(filter);
		}
	});

	layout->addWidget(m_list);
}

void SMixerEffectsRack::showAddFilterMenu()
{
	if (!m_source) return;
	
	QMenu menu(this);
	menu.setStyleSheet(
		"QMenu {"
		"  background: #2a2a2a; border: 1px solid #444;"
		"  color: #ddd; font-size: 11px;"
		"  font-family: 'Segoe UI', sans-serif;"
		"  padding: 4px 0px;"
		"  border-radius: 4px;"
		"}"
		"QMenu::item {"
		"  padding: 5px 20px 5px 12px;"
		"}"
		"QMenu::item:selected {"
		"  background: #00e5ff; color: #111;"
		"}"
		"QMenu::separator {"
		"  height: 1px; background: #444; margin: 4px 8px;"
		"}"
	);
	
	auto filterTypes = getAvailableFilterTypes();
	
	if (filterTypes.isEmpty()) {
		auto *noFilters = menu.addAction("No filters available");
		noFilters->setEnabled(false);
	} else {
		// Separate audio and video filters
		QList<FilterTypeInfo> audioFilters;
		QList<FilterTypeInfo> videoFilters;
		QList<FilterTypeInfo> otherFilters;
		
		for (const auto &ft : filterTypes) {
			uint32_t flags = obs_get_source_output_flags(ft.id.toUtf8().constData());
			if (flags & OBS_SOURCE_AUDIO) {
				audioFilters.append(ft);
			} else if (flags & OBS_SOURCE_VIDEO) {
				videoFilters.append(ft);
			} else {
				otherFilters.append(ft);
			}
		}
		
		// Audio filters section
		if (!audioFilters.isEmpty()) {
			// auto *sectionLabel = menu.addAction("— Audio —");
			// sectionLabel->setEnabled(false);
			for (const auto &ft : audioFilters) {
				auto *action = menu.addAction(ft.displayName);
				action->setData(ft.id);
			}
		}
		
		// Video filters section
		// if (!videoFilters.isEmpty()) {
		// 	if (!audioFilters.isEmpty())
		// 		menu.addSeparator();
		// 	auto *sectionLabel = menu.addAction("— Video —");
		// 	sectionLabel->setEnabled(false);
		// 	for (const auto &ft : videoFilters) {
		// 		auto *action = menu.addAction(ft.displayName);
		// 		action->setData(ft.id);
		// 	}
		// }
		
		// Other filters
		// if (!otherFilters.isEmpty()) {
		// 	if (!audioFilters.isEmpty() || !videoFilters.isEmpty())
		// 		menu.addSeparator();
		// 	for (const auto &ft : otherFilters) {
		// 		auto *action = menu.addAction(ft.displayName);
		// 		action->setData(ft.id);
		// 	}
		// }
		
		// Separator + open filters dialog
		menu.addSeparator();
		auto *openDialog = menu.addAction("Open Filters Dialog...");
		connect(openDialog, &QAction::triggered, this, [this]() {
			if (m_source)
				obs_frontend_open_source_filters(m_source);
		});
	}
	
	QAction *selected = menu.exec(m_add_btn->mapToGlobal(QPoint(0, m_add_btn->height())));
	if (selected && selected->data().isValid()) {
		QString typeId = selected->data().toString();
		if (!typeId.isEmpty()) {
			addFilter(typeId);
		}
	}
}

void SMixerEffectsRack::addFilter(const QString &typeId)
{
	if (!m_source) return;
	
	const char *displayName = obs_source_get_display_name(typeId.toUtf8().constData());
	QString baseName = displayName ? QString::fromUtf8(displayName) : typeId;
	QString filterName = generateUniqueFilterName(m_source, baseName);
	
	// Create the filter source
	OBSSourceAutoRelease filter = obs_source_create(
		typeId.toUtf8().constData(),
		filterName.toUtf8().constData(),
		nullptr, nullptr
	);
	
	if (filter) {
		obs_source_filter_add(m_source, filter);
		refresh();

		// Open properties immediately after adding to encourage configuration
		obs_frontend_open_source_properties(filter);
	}
}

void SMixerEffectsRack::setSource(obs_source_t *source)
{
	m_source = source;
	refresh();
}

void SMixerEffectsRack::clearItems()
{
	m_list->clear();
}

void SMixerEffectsRack::refresh()
{
	if (m_updating_internal)
		return;

	QSignalBlocker blocker(m_list->model()); // Prevent rowsMoved during population
	clearItems();

	if (!m_source) {
		auto *item = new QListWidgetItem(m_list);
		auto *lbl = new QLabel("No Source", m_list);
		lbl->setAlignment(Qt::AlignCenter);
		lbl->setStyleSheet("color: #555; font-style: italic; font-size: 10px; padding: 10px;");
		item->setSizeHint(QSize(0, 40));
		item->setFlags(Qt::NoItemFlags); // Not selectable/draggable
		m_list->addItem(item);
		m_list->setItemWidget(item, lbl);
		return;
	}

	// OBS filters enum
	struct EnumData {
		SMixerEffectsRack *rack;
		bool empty;
	} data = {this, true};

	obs_source_enum_filters(m_source, [](obs_source_t *, obs_source_t *filter, void *param) {
		auto *d = static_cast<EnumData *>(param);
		d->empty = false;
		auto *rack = d->rack;

		const char *name = obs_source_get_name(filter);
		const char *uuid = obs_source_get_uuid(filter);
		bool enabled = obs_source_enabled(filter);

		auto *item = new QListWidgetItem(rack->m_list);
		item->setData(Qt::UserRole, QString::fromUtf8(name));
		if (uuid)
			item->setData(Qt::UserRole + 1, QString::fromUtf8(uuid));
		
		// Create Row Widget
		auto *row = new QWidget();
		auto *h = new QHBoxLayout(row);
		h->setContentsMargins(8, 3, 8, 3); 
		h->setSpacing(6);

		// Filter name (Left) - Using Elided Label
		auto *lbl = new SMixerElidedLabel(name ? name : "(Unnamed)", row);
		lbl->setStyleSheet(QString(
			"border: none; color: %1; font-size: 11px; font-family: 'Segoe UI', sans-serif;"
		).arg(enabled ? "#ddd" : "#666"));
		lbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		h->addWidget(lbl);

		// Enable Switch (Right)
		auto *sw = new SMixerSwitch(row);
		sw->setFixedSize(26, 14); // Explicit size to ensure fit
		sw->setChecked(enabled, false);

		// Capture filter ref for toggle
		obs_source_t *filter_ref = obs_source_get_ref(filter);
		signal_handler_t *sh = obs_source_get_signal_handler(filter);
		
		QObject::connect(sw, &SMixerSwitch::toggled, rack, [filter_ref](bool checked) {
			if (filter_ref)
				obs_source_set_enabled(filter_ref, checked);
		});
		
		// Update label color on toggle
		QObject::connect(sw, &SMixerSwitch::toggled, lbl, [lbl](bool checked) {
			lbl->setStyleSheet(QString(
				"border: none; color: %1; font-size: 11px; font-family: 'Segoe UI', sans-serif;"
			).arg(checked ? "#ddd" : "#666"));
		});

		// Listen for external changes
		signal_handler_connect(sh, "enable", obs_filter_enabled_cb, sw);

		// Cleanup
		QObject::connect(row, &QObject::destroyed, [filter_ref, sh, sw]() {
			signal_handler_disconnect(sh, "enable", obs_filter_enabled_cb, sw);
			if (filter_ref)
				obs_source_release(filter_ref);
		});

		h->addWidget(sw);

		// Important: Set width to something small to prevent QListWidget from expanding 
		// the item beyond viewport width (which causes scrolling/overflow).
		// Height must be sufficient for content.
		QSize hint = row->sizeHint();
		hint.setWidth(0); 
		item->setSizeHint(hint);
		
		rack->m_list->addItem(item);
		rack->m_list->setItemWidget(item, row);
		
	}, &data);

	if (data.empty) {
		auto *item = new QListWidgetItem(m_list);
		auto *lbl = new QLabel("No Filters", m_list);
		lbl->setAlignment(Qt::AlignCenter);
		lbl->setStyleSheet("color: #555; font-style: italic; font-size: 10px; padding: 10px;");
		item->setSizeHint(QSize(0, 40));
		item->setFlags(Qt::NoItemFlags);
		m_list->addItem(item);
		m_list->setItemWidget(item, lbl);
	}
}

void SMixerEffectsRack::onReorder()
{
	if (!m_source || m_updating_internal)
		return;

	m_updating_internal = true;

	// Algorithm: Iterate items from BOTTOM to TOP.
	// For each item, move its corresponding OBS filter to TOP (OBS_ORDER_MOVE_TOP).
	// This reconstructs the UI order in OBS (as a stack).
	
	int count = m_list->count();
	for (int i = count - 1; i >= 0; i--) {
		QListWidgetItem *item = m_list->item(i);
		
		obs_source_t *filter = nullptr;
		QString uuid = item->data(Qt::UserRole + 1).toString();
		if (!uuid.isEmpty()) {
			filter = findFilterByUuid(m_source, uuid.toUtf8().constData());
		}
		
		// Fallback to name
		if (!filter) {
			QString name = item->data(Qt::UserRole).toString();
			if (!name.isEmpty())
				filter = findFilterByName(m_source, name.toUtf8().constData());
		}

		if (filter) {
			obs_source_filter_set_order(m_source, filter, OBS_ORDER_MOVE_TOP);
			// Filter pointer from enum is borrowed/internal, so we don't release.
		}
	}

	m_updating_internal = false;
	
	// Rebuild immediately to restore widgets (since drag-drop destroyed them)
	refresh();
}

void SMixerEffectsRack::setExpanded(bool expanded)
{
	m_is_expanded = expanded;
	m_list->setVisible(expanded);
	if (m_collapse_btn) {
		m_collapse_btn->setText(expanded ? "v" : ">");
		m_collapse_btn->setToolTip(expanded ? "Collapse" : "Expand");
	}
}

bool SMixerEffectsRack::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::MouseButtonDblClick) {
		setExpanded(!m_is_expanded);
		return true;
	}
	return QWidget::eventFilter(obj, event);
}

} // namespace super
