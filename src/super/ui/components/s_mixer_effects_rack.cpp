#include "s_mixer_effects_rack.hpp"
#include "s_mixer_filter_controls.hpp"
#include "s_mixer_sidebar_toggle.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QCheckBox>
#include <QPainter>
#include <QFontMetrics>
#include <QIcon>
#include <QPixmap>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QApplication>
#include <QLineEdit>

#include <QListWidgetItem>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <obs-frontend-api.h>
#include <cmath>

namespace super {

// Static clipboard
QList<SMixerEffectsRack::ClipboardFilter> SMixerEffectsRack::s_clipboard;

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
	QString text() const { return m_text; }

	QSize minimumSizeHint() const override {
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
		setStyleSheet("QPushButton { border: none; background: transparent; padding: 0px; margin: 0px; min-height: 0px; }");
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		p.setRenderHint(QPainter::SmoothPixmapTransform);

		auto color = QColor("#888");
		if (isDown()) color = QColor("#ffffff");
		else if (underMouse()) color = QColor("#00e5ff");

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
			p.setPen(color);
			QFont f = font();
			f.setBold(true);
			f.setPixelSize(14);
			p.setFont(f);
			p.drawText(rect(), Qt::AlignCenter, "+");
		}
	}
};

// Power icon button — paints a ⏻ power symbol with color tinting
class SMixerFilterPowerButton : public QPushButton {
public:
	explicit SMixerFilterPowerButton(bool initialEnabled, QWidget *parent = nullptr)
		: QPushButton(parent)
	{
		setObjectName("powerBtn");
		setFixedSize(14, 14);
		setCursor(Qt::PointingHandCursor);
		setToolTip("Toggle Enable");
		setProperty("filterEnabled", initialEnabled);
		setStyleSheet("QPushButton { border: none; background: transparent; padding: 0; margin: 0; min-height: 0; }");
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		p.setRenderHint(QPainter::SmoothPixmapTransform);

		bool enabled = property("filterEnabled").toBool();
		// Green (#00e676) for active, Gray (#555) for disabled
		auto color = enabled ? QColor("#00e676") : QColor("#555");
		
		if (isDown()) color = QColor("#ffffff");
		else if (underMouse()) color = enabled ? QColor("#66ffa6") : QColor("#888");

		static const QIcon icon(":/super/assets/icons/super/mixer/fx-power.svg");
		
		int dim = 12; 
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
			// Fallback if icon missing
			p.setPen(QPen(color, 1.5));
			QRectF r(3, 3, 8, 8);
			p.drawArc(r, 40 * 16, 280 * 16);
			p.drawLine(QPointF(7, 3), QPointF(7, 6));
		}
	}
};



// Plugin (wrench) icon button for VST filters
class SMixerFilterPluginButton : public QPushButton {
public:
	explicit SMixerFilterPluginButton(QWidget *parent = nullptr)
		: QPushButton(parent)
	{
		setObjectName("pluginBtn");
		setFixedSize(14, 14);
		setCursor(Qt::PointingHandCursor);
		setToolTip("Open Plugin Interface");
		setProperty("pluginOpen", false);
		setStyleSheet("QPushButton { border: none; background: transparent; padding: 0; margin: 0; min-height: 0; }");
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		p.setRenderHint(QPainter::SmoothPixmapTransform);

		bool isOpen = property("pluginOpen").toBool();
		auto color = isOpen ? QColor("#00e5ff") : QColor("#888");

		if (!isEnabled()) color = QColor("#444");
		else if (isDown()) color = QColor("#ffffff");
		else if (underMouse()) color = isOpen ? QColor("#66ffa6") : QColor("#aaa");

		static const QIcon icon(":/super/assets/icons/super/mixer/fx-wrench.svg");

		int dim = 12;
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
			p.setPen(QPen(color, 1.5));
			p.drawRect(3, 3, 8, 8);
		}
	}
};

// Settings (gear) icon button for accordion expand
class SMixerFilterSettingsButton : public QPushButton {
public:
	explicit SMixerFilterSettingsButton(QWidget *parent = nullptr)
		: QPushButton(parent)
	{
		setObjectName("settingsBtn");
		setFixedSize(14, 14);
		setCursor(Qt::PointingHandCursor);
		setToolTip("Toggle Controls");
		setProperty("expanded", false);
		setStyleSheet("QPushButton { border: none; background: transparent; padding: 0; margin: 0; min-height: 0; }");
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		p.setRenderHint(QPainter::SmoothPixmapTransform);

		bool expanded = property("expanded").toBool();
		auto color = expanded ? QColor("#00cccc") : QColor("#888");
		
		if (!isEnabled()) {
			color = QColor("#444");
		} else {
			if (isDown()) color = QColor("#ffffff");
			else if (underMouse()) color = QColor("#00e5ff");
		}

		static const QIcon icon(":/super/assets/icons/super/mixer/fx-controls.svg");

		int dim = 12;
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
			// Fallback
			p.setPen(QPen(color, 1.2));
			QPointF center(7, 7);
			double outerR = 5.5, innerR = 3.5;
			p.drawEllipse(center, innerR - 1.0, innerR - 1.0);
			for (int i = 0; i < 6; i++) {
				double angle = i * 60.0 * M_PI / 180.0;
				QPointF from(center.x() + innerR * cos(angle), center.y() + innerR * sin(angle));
				QPointF to(center.x() + outerR * cos(angle), center.y() + outerR * sin(angle));
				p.drawLine(from, to);
			}
		}
	}
};

// Callback for external filter enable/disable changes
// Context for filter callbacks
struct FilterCbCtx {
	QPushButton *powerBtn;
	SMixerElidedLabel *lbl;
	bool valid;
	SMixerFilterPluginButton *pluginBtn; // For VST filters
};

// Callback for external filter enable/disable changes
static void obs_filter_enable_change_cb(void *data, calldata_t *cd)
{
	auto *ctx = static_cast<FilterCbCtx *>(data);
	bool enabled = calldata_bool(cd, "enabled");
	QMetaObject::invokeMethod(ctx->powerBtn, [ctx, enabled]() {
		// Update Power Button visual
		ctx->powerBtn->setProperty("filterEnabled", enabled);
		ctx->powerBtn->setStyleSheet(ctx->powerBtn->styleSheet()); // force repaint
		ctx->powerBtn->update();
		// Update Label Color
		const char *c = "#ddd";
		if (!ctx->valid) c = "#ff4444";
		else if (!enabled) c = "#888";

		ctx->lbl->setStyleSheet(QString(
			"border: none; color: %1; font-size: 11px; font-family: 'Segoe UI', sans-serif;"
		).arg(c));
	});
}

// Callback for external filter property updates (specifically for VST UI state)
static void obs_filter_update_cb(void *data, calldata_t *cd)
{
	auto *ctx = static_cast<FilterCbCtx *>(data);
	if (!ctx->pluginBtn) return;

	obs_source_t *source = (obs_source_t*)calldata_ptr(cd, "source");
	if (!source) return;

	obs_properties_t *props = obs_source_properties(source);
	if (!props) return;

	obs_property_t *closeProp = obs_properties_get(props, "close_vst_settings");
	obs_property_t *openProp = obs_properties_get(props, "open_vst_settings");
	
	bool isOpen = closeProp && obs_property_visible(closeProp);
	bool hasUI = isOpen || (openProp && obs_property_visible(openProp));
	obs_properties_destroy(props);

	QMetaObject::invokeMethod(ctx->pluginBtn, [ctx, hasUI, isOpen]() {
		ctx->pluginBtn->setProperty("vstHasUI", hasUI);
		ctx->pluginBtn->setProperty("pluginOpen", isOpen);
		
		// If multiselected, checking selection logic is tricky here without rack context.
		// However, a simple update to setEnabled will be correct if it's single selected,
		// and the itemSelectionChanged handler will correct it next time selection shifts.
		// For safety, we just update the tooltip and redraw properties.
		if (!hasUI) {
			ctx->pluginBtn->setToolTip("Please select a VST plugin from the settings");
			ctx->pluginBtn->setEnabled(hasUI); // We force disable if no UI
		} else {
			ctx->pluginBtn->setToolTip("Open Plugin Interface");
			// We ideally shouldn't force-enable here in case of multi-selection, 
			// but we can let itemSelectionChanged manage the enabled state if needed later.
		}
		
		ctx->pluginBtn->setStyleSheet(ctx->pluginBtn->styleSheet());
		ctx->pluginBtn->update();
	});
}

// ============================================================================
// Menu Style (shared)
// ============================================================================

static const char *kMenuStyleSheet =
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
	"QMenu::item:disabled {"
	"  color: #666;"
	"}"
	"QMenu::separator {"
	"  height: 1px; background: #444; margin: 4px 8px;"
	"}";

// ============================================================================
// Filter Type Enumeration
// ============================================================================

struct FilterTypeInfo {
	QString id;
	QString displayName;
};

static QList<FilterTypeInfo> getAvailableFilterTypes()
{
	QList<FilterTypeInfo> result;
	
	size_t idx = 0;
	const char *typeId = nullptr;
	while (obs_enum_filter_types(idx++, &typeId)) {
		if (!typeId) continue;
		
		uint32_t flags = obs_get_source_output_flags(typeId);
		if (flags & OBS_SOURCE_CAP_DISABLED) continue;
		
		const char *displayName = obs_source_get_display_name(typeId);
		if (!displayName || !*displayName) continue;
		
		result.append({QString::fromUtf8(typeId), QString::fromUtf8(displayName)});
	}
	
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

// Helper: Get filter index in chain
static int getFilterIndex(obs_source_t *source, obs_source_t *filter)
{
	struct IndexData {
		obs_source_t *target;
		int index;
		int current;
	} data = {filter, -1, 0};

	obs_source_enum_filters(source, [](obs_source_t *, obs_source_t *f, void *param) {
		auto *d = static_cast<IndexData *>(param);
		if (f == d->target)
			d->index = d->current;
		d->current++;
	}, &data);

	return data.index;
}

// Helper: Get filter count
static int getFilterCount(obs_source_t *source)
{
	int count = 0;
	obs_source_enum_filters(source, [](obs_source_t *, obs_source_t *, void *param) {
		(*static_cast<int*>(param))++;
	}, &count);
	return count;
}

// Helper: Check if a filter has visible properties
static bool filterHasVisibleProperties(obs_source_t *filter)
{
	bool hasProperties = false;
	obs_properties_t *props = obs_source_properties(filter);
	if (props) {
		obs_property_t *prop = obs_properties_first(props);
		while (prop) {
			if (obs_property_visible(prop)) {
				hasProperties = true;
				break;
			}
			obs_property_next(&prop);
		}
		obs_properties_destroy(props);
	}
	return hasProperties;
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

	// Title
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
	m_list->setFocusPolicy(Qt::StrongFocus);
	m_list->setFrameShape(QFrame::NoFrame);
	m_list->setStyleSheet(
		"QListWidget { background: transparent; border: none; outline: none; }"
		"QListWidget::item { background: rgba(255, 255, 255, 4); border-radius: 4px; margin: 0px 2px; padding: 0px; border: none; }"
		"QListWidget::item:selected { background: rgba(255, 255, 255, 12); border: none; outline: none; }"
		"QListWidget::item:hover { background: rgba(255, 255, 255, 8); }"
		"QListWidget::item:focus { outline: none; border: none; }"
	);
	m_list->setSpacing(2);
	m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	
	// Drag & Drop Config
	m_list->setDragDropMode(QAbstractItemView::InternalMove);
	m_list->setDefaultDropAction(Qt::MoveAction);
	m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	
	// Allow keyboard focus
	m_list->installEventFilter(this);
	
	connect(m_list->model(), &QAbstractItemModel::rowsMoved, this, &SMixerEffectsRack::onReorder);
	
	// Ctrl+Click to open properties dialog (replaces double-click)
	// (Handled in eventFilter instead)

	// Single click emits filterClicked signal
	connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
		if (!m_source || !item) return;
		obs_source_t *filter = filterFromItem(item);
		if (filter) {
			emit filterClicked(filter);
		}
	});
	
	// Context menu on items
	m_list->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
		QListWidgetItem *item = m_list->itemAt(pos);
		if (item && (item->flags() & Qt::ItemIsSelectable)) {
			showItemContextMenu(item, m_list->viewport()->mapToGlobal(pos));
		} else {
			showRackContextMenu(m_list->viewport()->mapToGlobal(pos));
		}
	});

	// Multi-select updates
	connect(m_list, &QListWidget::itemSelectionChanged, this, [this]() {
		auto selected = m_list->selectedItems();
		bool multi = selected.size() > 1;
		
		for (int i = 0; i < m_list->count(); ++i) {
			auto *item = m_list->item(i);
			QWidget *container = m_list->itemWidget(item);
			if (container) {
				if (auto *p = container->findChild<QPushButton *>("pluginBtn"))
					p->setEnabled(!multi && p->property("vstHasUI").toBool());
				if (auto *s = container->findChild<QPushButton *>("settingsBtn")) {
					obs_source_t *f = filterFromItem(item);
					s->setEnabled(!multi && f && filterHasVisibleProperties(f));
				}
			}
		}
	});

	layout->addWidget(m_list);
}

// ============================================================================
// Filter Resolution
// ============================================================================

obs_source_t *SMixerEffectsRack::filterFromItem(QListWidgetItem *item)
{
	if (!m_source || !item) return nullptr;

	QString uuid = item->data(Qt::UserRole + 1).toString();
	if (!uuid.isEmpty()) {
		obs_source_t *f = findFilterByUuid(m_source, uuid.toUtf8().constData());
		if (f) return f;
	}
	
	// Fallback to name
	QString name = item->data(Qt::UserRole).toString();
	if (!name.isEmpty())
		return findFilterByName(m_source, name.toUtf8().constData());

	return nullptr;
}

// ============================================================================
// Add Filter Menu
// ============================================================================

void SMixerEffectsRack::showAddFilterMenu()
{
	if (!m_source) return;
	
	QMenu menu(this);
	menu.setStyleSheet(kMenuStyleSheet);
	
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
			for (const auto &ft : audioFilters) {
				auto *action = menu.addAction(ft.displayName);
				action->setData(ft.id);
			}
		}
		
		// Separator + open filters dialog
		menu.addSeparator();
		auto *openDialog = menu.addAction("Open Filters Dialog...");
		connect(openDialog, &QAction::triggered, this, [this]() {
			if (m_source) obs_frontend_open_source_filters(m_source);
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
	
	OBSSourceAutoRelease filter = obs_source_create(
		typeId.toUtf8().constData(),
		filterName.toUtf8().constData(),
		nullptr, nullptr
	);
	
	if (filter) {
		obs_source_filter_add(m_source, filter);
		refresh();

		auto flags = obs_source_get_output_flags(filter);
		if ((flags & OBS_SOURCE_CAP_DONT_SHOW_PROPERTIES) == 0 && filterHasVisibleProperties(filter)) {
			obs_frontend_open_source_properties(filter);
		}
	}
}

// ============================================================================
// Item Context Menu
// ============================================================================

void SMixerEffectsRack::showItemContextMenu(QListWidgetItem *item, const QPoint &globalPos)
{
	if (!m_source || !item) return;

	obs_source_t *filter = filterFromItem(item);
	if (!filter) return;

	bool enabled = obs_source_enabled(filter);
	int idx = getFilterIndex(m_source, filter);
	int count = getFilterCount(m_source);

	QMenu menu(this);
	menu.setStyleSheet(kMenuStyleSheet);

	// Enable/Disable
	auto *toggleAct = menu.addAction(enabled ? "Disable" : "Enable");
	toggleAct->setShortcut(QKeySequence("Alt+Click"));
	connect(toggleAct, &QAction::triggered, this, [this, item]() {
		toggleFilterEnabled(item);
	});

	menu.addSeparator();

	// Rename
	auto *renameAct = menu.addAction("Rename");
	renameAct->setShortcut(QKeySequence("F2"));
	connect(renameAct, &QAction::triggered, this, [this, item]() {
		renameFilter(item);
	});

	menu.addSeparator();

	// Move Up/Down
	auto *moveUpAct = menu.addAction("Move Up");
	moveUpAct->setEnabled(idx > 0);
	connect(moveUpAct, &QAction::triggered, this, [this, item]() {
		moveFilterUp(item);
	});

	auto *moveDownAct = menu.addAction("Move Down");
	moveDownAct->setEnabled(idx < count - 1);
	connect(moveDownAct, &QAction::triggered, this, [this, item]() {
		moveFilterDown(item);
	});

	menu.addSeparator();

	// Copy
	auto *copyAct = menu.addAction("Copy");
	connect(copyAct, &QAction::triggered, this, [this, filter]() {
		copyFilter(m_source, filter);
	});

	// Paste submenu
	auto *pasteMenu = menu.addMenu("Paste");
	pasteMenu->setStyleSheet(kMenuStyleSheet);
	pasteMenu->setEnabled(hasClipboardFilters());

	auto *pasteAbove = pasteMenu->addAction("Above");
	connect(pasteAbove, &QAction::triggered, this, [this, idx]() {
		pasteFilters(m_source, idx);
		refresh();
	});

	auto *pasteBelow = pasteMenu->addAction("Below");
	connect(pasteBelow, &QAction::triggered, this, [this, idx]() {
		pasteFilters(m_source, idx + 1);
		refresh();
	});

	menu.addSeparator();

	// Delete
	auto *deleteAct = menu.addAction("Delete");
	deleteAct->setShortcut(QKeySequence::Delete);
	connect(deleteAct, &QAction::triggered, this, [this]() {
		bool shiftHeld = QApplication::keyboardModifiers() & Qt::ShiftModifier;
		auto items = m_list->selectedItems();
		if (items.isEmpty()) return;

		if (!shiftHeld) {
			QString msg;
			if (items.size() == 1) {
				obs_source_t *f = filterFromItem(items.first());
				const char *n = f ? obs_source_get_name(f) : "(unnamed)";
				msg = QString("Delete filter \"%1\"?").arg(n ? n : "(unnamed)");
			} else {
				msg = QString("Delete %1 filters?").arg(items.size());
			}
			auto result = QMessageBox::question(this, "Delete Filter", msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
			if (result != QMessageBox::Yes) return;
		}

		for (auto *i : items) {
			deleteFilter(i, false);
		}
	});

	menu.addSeparator();

	// Properties
	auto *propsAct = menu.addAction("Properties");
	connect(propsAct, &QAction::triggered, this, [filter]() {
		obs_frontend_open_source_properties(filter);
	});

	menu.exec(globalPos);
}

// ============================================================================
// Rack Context Menu (background area)
// ============================================================================

void SMixerEffectsRack::showRackContextMenu(const QPoint &globalPos)
{
	if (!m_source) return;

	QMenu menu(this);
	menu.setStyleSheet(kMenuStyleSheet);

	// Add
	auto *addAct = menu.addAction("Add Filter...");
	connect(addAct, &QAction::triggered, this, &SMixerEffectsRack::showAddFilterMenu);

	menu.addSeparator();

	// Copy Filter(s)
	auto *copyAct = menu.addAction("Copy Filter(s)");
	copyAct->setEnabled(getFilterCount(m_source) > 0);
	connect(copyAct, &QAction::triggered, this, [this]() {
		copyAllFilters(m_source);
	});

	// Paste Filter(s)
	auto *pasteAct = menu.addAction("Paste Filter(s)");
	pasteAct->setEnabled(hasClipboardFilters());
	connect(pasteAct, &QAction::triggered, this, [this]() {
		pasteFilters(m_source);
		refresh();
	});

	menu.addSeparator();

	// Clear All
	auto *clearAct = menu.addAction("Clear All");
	clearAct->setEnabled(getFilterCount(m_source) > 0);
	connect(clearAct, &QAction::triggered, this, [this]() {
		clearAllFilters();
	});

	menu.exec(globalPos);
}

// ============================================================================
// Filter Operations
// ============================================================================

void SMixerEffectsRack::moveFilterUp(QListWidgetItem *item)
{
	obs_source_t *filter = filterFromItem(item);
	if (!filter || !m_source) return;
	obs_source_filter_set_order(m_source, filter, OBS_ORDER_MOVE_UP);
	refresh();
}

void SMixerEffectsRack::moveFilterDown(QListWidgetItem *item)
{
	obs_source_t *filter = filterFromItem(item);
	if (!filter || !m_source) return;
	obs_source_filter_set_order(m_source, filter, OBS_ORDER_MOVE_DOWN);
	refresh();
}

void SMixerEffectsRack::deleteFilter(QListWidgetItem *item, bool confirm)
{
	obs_source_t *filter = filterFromItem(item);
	if (!filter || !m_source) return;

	const char *name = obs_source_get_name(filter);

	if (confirm) {
		auto result = QMessageBox::question(
			this, "Delete Filter",
			QString("Delete filter \"%1\"?").arg(name ? name : "(unnamed)"),
			QMessageBox::Yes | QMessageBox::No,
			QMessageBox::No
		);
		if (result != QMessageBox::Yes) return;
	}

	obs_source_filter_remove(m_source, filter);
	refresh();
}

void SMixerEffectsRack::renameFilter(QListWidgetItem *item)
{
	obs_source_t *filter = filterFromItem(item);
	if (!filter || !m_source) return;

	QWidget *container = m_list->itemWidget(item);
	if (!container) return;

	QStackedWidget *stack = container->findChild<QStackedWidget*>();
	QLineEdit *edit = container->findChild<QLineEdit*>();
	if (!stack || !edit) return;

	const char *currentName = obs_source_get_name(filter);
	edit->setText(currentName ? currentName : "");
	stack->setCurrentIndex(1);
	edit->setFocus();
	edit->selectAll();
}

void SMixerEffectsRack::toggleFilterEnabled(QListWidgetItem *item)
{
	obs_source_t *filter = filterFromItem(item);
	if (!filter) return;
	obs_source_set_enabled(filter, !obs_source_enabled(filter));
	// The signal callback will update the switch UI
}

void SMixerEffectsRack::clearAllFilters()
{
	if (!m_source) return;

	auto result = QMessageBox::question(
		this, "Clear All Filters",
		"Remove all filters from this source?",
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No
	);
	if (result != QMessageBox::Yes) return;

	// Collect all filters first (can't modify while enumerating)
	QList<obs_source_t*> filters;
	obs_source_enum_filters(m_source, [](obs_source_t *, obs_source_t *filter, void *param) {
		auto *list = static_cast<QList<obs_source_t*>*>(param);
		list->append(obs_source_get_ref(filter));
	}, &filters);

	for (obs_source_t *f : filters) {
		obs_source_filter_remove(m_source, f);
		obs_source_release(f);
	}
	
	refresh();
}

// ============================================================================
// Clipboard Operations
// ============================================================================

void SMixerEffectsRack::copyFilter(obs_source_t *, obs_source_t *filter)
{
	s_clipboard.clear();
	if (!filter) return;

	ClipboardFilter cf;
	cf.typeId = QString::fromUtf8(obs_source_get_unversioned_id(filter));
	cf.name = QString::fromUtf8(obs_source_get_name(filter));
	cf.settings = OBSData(obs_source_get_settings(filter));
	s_clipboard.append(cf);
}

void SMixerEffectsRack::copyAllFilters(obs_source_t *source)
{
	s_clipboard.clear();
	if (!source) return;

	obs_source_enum_filters(source, [](obs_source_t *, obs_source_t *filter, void *param) {
		auto *clipboard = static_cast<QList<ClipboardFilter>*>(param);
		ClipboardFilter cf;
		cf.typeId = QString::fromUtf8(obs_source_get_unversioned_id(filter));
		cf.name = QString::fromUtf8(obs_source_get_name(filter));
		cf.settings = OBSData(obs_source_get_settings(filter));
		clipboard->append(cf);
	}, &s_clipboard);
}

bool SMixerEffectsRack::hasClipboardFilters()
{
	return !s_clipboard.isEmpty();
}

void SMixerEffectsRack::pasteFilters(obs_source_t *source, int insertIndex)
{
	if (!source || s_clipboard.isEmpty()) return;

	for (const auto &cf : s_clipboard) {
		QString uniqueName = generateUniqueFilterName(source, cf.name);
		
		OBSSourceAutoRelease newFilter = obs_source_create(
			cf.typeId.toUtf8().constData(),
			uniqueName.toUtf8().constData(),
			cf.settings, nullptr
		);
		
		if (newFilter) {
			obs_source_filter_add(source, newFilter);
			
			// Move to desired position if specified
			if (insertIndex >= 0) {
				int currentIdx = getFilterIndex(source, newFilter);
				if (currentIdx > insertIndex) {
					// Need to move up
					for (int i = currentIdx; i > insertIndex; --i) {
						obs_source_filter_set_order(source, newFilter, OBS_ORDER_MOVE_UP);
					}
				}
				insertIndex++; // Next filter goes after this one
			}
		}
	}
}

// ============================================================================
// Source Binding
// ============================================================================

// Callback for external filter enable/disable changes
static void obs_filter_enable_change_cb(void *data, calldata_t *cd);

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

	QSignalBlocker blocker(m_list->model());
	// Reset accordion state
	m_controls_items.clear();
	m_controls_map.clear();

	clearItems();

	if (!m_source) {
		auto *item = new QListWidgetItem(m_list);
		auto *lbl = new QLabel("No Source", m_list);
		lbl->setAlignment(Qt::AlignCenter);
		lbl->setStyleSheet("color: #555; font-style: italic; font-size: 10px; padding: 10px;");
		item->setSizeHint(QSize(0, 40));
		item->setFlags(Qt::NoItemFlags);
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
		
		// Capture Refs early
		obs_source_t *filter_ref = obs_source_get_ref(filter);

		// Create Container Widget
		auto *container = new QFrame();
		container->setObjectName("filterContainer");
		container->setStyleSheet("#filterContainer { border: 1px solid transparent; border-radius: 4px; }");
		auto *vbox = new QVBoxLayout(container);
		vbox->setContentsMargins(0, 0, 0, 0);
		vbox->setSpacing(0);

		// Create Row Widget
		auto *row = new QFrame(container);
		row->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
		row->setObjectName("filterRow");
		row->setStyleSheet("#filterRow { border-bottom: 1px solid transparent; border-radius: 4px; }");
		auto *h = new QHBoxLayout(row);
		h->setContentsMargins(8, 2, 8, 2); 
		h->setSpacing(8);

		// Determine Logic
		// Invalid/Missing Plugin (Red) vs Disabled (Gray) vs Enabled (Normal)
		const char *typeId = obs_source_get_id(filter);
		bool valid = obs_source_get_display_name(typeId) != nullptr;

		// Power Button (Left) — Enable/Disable toggle
		auto *powerBtn = new SMixerFilterPowerButton(enabled, row);

		// Name Stack (Label vs Edit)
		auto *nameStack = new QStackedWidget(row);
		nameStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		nameStack->setFixedHeight(18); // Prevent height jumping

		// Filter name - Using Elided Label
		auto *lbl = new SMixerElidedLabel(name ? name : "(Unnamed)", nameStack);
		const char *color = "#ddd";
		if (!valid) color = "#ff4444";
		else if (!enabled) color = "#888";

		lbl->setStyleSheet(QString(
			"border: none; color: %1; font-size: 11px; font-family: 'Segoe UI', sans-serif;"
		).arg(color));
		lbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

		// Edit box for rename
		auto *edit = new QLineEdit(nameStack);
		edit->setText(name ? name : "");
		edit->setStyleSheet(
			"background: #1a3a4a; color: #00e5ff;"
			"font-size: 11px; font-family: 'Segoe UI', sans-serif;"
			"border: 1px solid #00e5ff; border-radius: 3px;"
			"padding: 0 4px;"
			"selection-background-color: #00e5ff; selection-color: #1a1a1a;"
		);
		edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

		nameStack->addWidget(lbl);
		nameStack->addWidget(edit);
		nameStack->setCurrentIndex(0);

		auto finishRename = [rack, filter_ref, edit, nameStack, lbl]() {
			if (nameStack->currentIndex() != 1) return;
			nameStack->setCurrentIndex(0);

			QString newName = edit->text().trimmed();
			if (!newName.isEmpty() && filter_ref) {
				obs_source_set_name(filter_ref, newName.toUtf8().constData());
				lbl->setText(newName);
			}
			rack->m_list->setFocus();
		};
		QObject::connect(edit, &QLineEdit::editingFinished, rack, finishRename);

		// Settings Button (Right) — Accordion expand
		auto *settingsBtn = new SMixerFilterSettingsButton(row);
		settingsBtn->setObjectName("settingsBtn");
		
		// Check if filter has properties
		bool hasProperties = filterHasVisibleProperties(filter);
		
		settingsBtn->setEnabled(hasProperties);
		if (!hasProperties) {
			settingsBtn->setToolTip("No configurable properties");
		}
		
		h->addWidget(powerBtn);
		h->addWidget(nameStack);

		bool isVst = (strcmp(typeId, "vst_filter") == 0);
		SMixerFilterPluginButton *pluginBtn = nullptr;
		if (isVst) {
			pluginBtn = new SMixerFilterPluginButton(row);
			bool hasUI = false;
			obs_properties_t *props = obs_source_properties(filter);
			if (props) {
				obs_property_t *closeProp = obs_properties_get(props, "close_vst_settings");
				obs_property_t *openProp = obs_properties_get(props, "open_vst_settings");
				pluginBtn->setProperty("pluginOpen", closeProp && obs_property_visible(closeProp));
				hasUI = (closeProp && obs_property_visible(closeProp)) || (openProp && obs_property_visible(openProp));
				obs_properties_destroy(props);
			}
			pluginBtn->setProperty("vstHasUI", hasUI);
			pluginBtn->setEnabled(hasUI);
			if (!hasUI) {
				pluginBtn->setToolTip("Please select a VST plugin from the settings");
			}
			h->addWidget(pluginBtn);
		}

		h->addWidget(settingsBtn);

		vbox->addWidget(row);

		// Create Controls Body (Hidden initially)
		auto *body = new QFrame(container);
		body->setObjectName("filterBody");
		body->setVisible(false);
		auto *bodyLayout = new QVBoxLayout(body);
		bodyLayout->setContentsMargins(4, 4, 4, 4);
		bodyLayout->setSpacing(0);
		vbox->addWidget(body);

		// Context for callback
		auto *ctx = new FilterCbCtx{powerBtn, lbl, valid, pluginBtn};

		// Power button toggles filter enabled state
		QObject::connect(powerBtn, &QPushButton::clicked, rack, [rack, item, filter_ref, valid]() {
			if (!filter_ref) return;
			
			bool targetEnabled = !obs_source_enabled(filter_ref);
			bool isSelected = item->isSelected();
			
			if (isSelected) {
				auto items = rack->m_list->selectedItems();
				for (auto *i : items) {
					obs_source_t *f = rack->filterFromItem(i);
					if (f) obs_source_set_enabled(f, targetEnabled);
				}
			} else {
				obs_source_set_enabled(filter_ref, targetEnabled);
			}
		});

		// Settings button toggles accordion
		QObject::connect(settingsBtn, &QPushButton::clicked, rack, [rack, item, settingsBtn]() {
			rack->toggleFilterControls(item);
			bool expanded = rack->m_controls_items.contains(item);
			settingsBtn->setProperty("expanded", expanded);
			settingsBtn->update();
		});

		if (isVst && pluginBtn) {
			QObject::connect(pluginBtn, &QPushButton::clicked, rack, [filter_ref, pluginBtn]() {
				if (!filter_ref) return;
				obs_properties_t *props = obs_source_properties(filter_ref);
				if (props) {
					obs_property_t *closeProp = obs_properties_get(props, "close_vst_settings");
					bool isOpen = closeProp && obs_property_visible(closeProp);
					if (isOpen) {
						obs_property_button_clicked(closeProp, filter_ref);
					} else {
						obs_property_t *openProp = obs_properties_get(props, "open_vst_settings");
						if (openProp) {
							obs_property_button_clicked(openProp, filter_ref);
						}
					}
					// Update state
					obs_properties_t *newProps = obs_source_properties(filter_ref);
					if (newProps) {
						obs_property_t *newClose = obs_properties_get(newProps, "close_vst_settings");
						pluginBtn->setProperty("pluginOpen", newClose && obs_property_visible(newClose));
						obs_properties_destroy(newProps);
					}
					pluginBtn->update();
					obs_properties_destroy(props);
				}
			});
		}

		// Listen for external changes
		if (obs_source_t *f = obs_source_get_ref(filter)) {
			signal_handler_t *sh = obs_source_get_signal_handler(f);
			if (sh) {
				signal_handler_connect(sh, "enable", obs_filter_enable_change_cb, ctx);
				if (pluginBtn) {
					signal_handler_connect(sh, "update", obs_filter_update_cb, ctx);
				}
			}
			obs_source_release(f);
		}

		// Cleanup
		QObject::connect(row, &QObject::destroyed, [filter_ref, ctx, pluginBtn]() {
			if (filter_ref) {
				signal_handler_t *sh = obs_source_get_signal_handler(filter_ref);
				if (sh) {
					signal_handler_disconnect(sh, "enable", obs_filter_enable_change_cb, ctx);
					if (pluginBtn) {
						signal_handler_disconnect(sh, "update", obs_filter_update_cb, ctx);
					}
				}
				obs_source_release(filter_ref);
			}
			delete ctx;
		});

		QSize hint = container->sizeHint();
		hint.setWidth(0); 
		item->setSizeHint(hint);

		rack->m_list->addItem(item);
		rack->m_list->setItemWidget(item, container);

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

	int count = m_list->count();
	for (int i = count - 1; i >= 0; i--) {
		QListWidgetItem *item = m_list->item(i);
		obs_source_t *filter = filterFromItem(item);
		if (filter) {
			obs_source_filter_set_order(m_source, filter, OBS_ORDER_MOVE_TOP);
		}
	}

	m_updating_internal = false;
	refresh();
}

void SMixerEffectsRack::setExpanded(bool expanded)
{
	m_is_expanded = expanded;
	m_list->setVisible(expanded);
	if (m_collapse_btn) {
		m_collapse_btn->setExpanded(expanded);
	}
}

// ============================================================================
// Event Filter — Alt+Click, Context Menu on header
// ============================================================================

bool SMixerEffectsRack::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == m_list && event->type() == QEvent::MouseButtonPress) {
		auto *mouseEvent = static_cast<QMouseEvent*>(event);
		QListWidgetItem *item = m_list->itemAt(mouseEvent->pos());
		if (!item) {
			m_list->clearSelection();
		}

		// Alt+Click on list items to toggle enable/disable
		if (item && mouseEvent->modifiers() & Qt::AltModifier) {
			if (item->flags() & Qt::ItemIsSelectable) {
				toggleFilterEnabled(item);
				return true;
			}
		}

		// Ctrl+Click opens OBS properties dialog
		if (item && mouseEvent->modifiers() & Qt::ControlModifier) {
			if (item->flags() & Qt::ItemIsSelectable) {
				obs_source_t *filter = filterFromItem(item);
				if (filter)
					obs_frontend_open_source_properties(filter);
				return true;
			}
		}
	}

	// Item double-click to toggle accordion
	if (obj == m_list->viewport() && event->type() == QEvent::MouseButtonDblClick) {
		auto *mouseEvent = static_cast<QMouseEvent*>(event);
		QListWidgetItem *item = m_list->itemAt(mouseEvent->pos());
		if (item && (item->flags() & Qt::ItemIsSelectable)) {
			// Toggle accordion
			QWidget *container = m_list->itemWidget(item);
			if (container) {
				auto *settingsBtn = container->findChild<QPushButton*>("settingsBtn");
				if (settingsBtn) {
					settingsBtn->click();
					return true;
				}
			}
		}
	}

	// Header double-click to collapse
	if (event->type() == QEvent::MouseButtonDblClick) {
		setExpanded(!m_is_expanded);
		return true;
	}
	return QWidget::eventFilter(obj, event);
}

// ============================================================================
// Keyboard Shortcuts
// ============================================================================

void SMixerEffectsRack::keyPressEvent(QKeyEvent *event)
{
	QListWidgetItem *item = m_list->currentItem();

	switch (event->key()) {
	case Qt::Key_F2:
		// Rename selected filter
		if (item && (item->flags() & Qt::ItemIsSelectable)) {
			renameFilter(item);
			return;
		}
		break;

	case Qt::Key_Delete:
	{
		// Delete selected filter(s)
		auto items = m_list->selectedItems();
		if (!items.isEmpty()) {
			bool shiftHeld = event->modifiers() & Qt::ShiftModifier;
			
			if (!shiftHeld) {
				QString msg;
				if (items.size() == 1) {
					obs_source_t *f = filterFromItem(items.first());
					const char *n = f ? obs_source_get_name(f) : "(unnamed)";
					msg = QString("Delete filter \"%1\"?").arg(n ? n : "(unnamed)");
				} else {
					msg = QString("Delete %1 filters?").arg(items.size());
				}
				auto result = QMessageBox::question(this, "Delete Filter", msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
				if (result != QMessageBox::Yes) return;
			}
			
			for (auto *i : items) {
				deleteFilter(i, false);
			}
			return;
		}
		break;
	}

	default:
		break;
	}

	QWidget::keyPressEvent(event);
}

// ============================================================================
// Shift+Scroll — Move Filter Up/Down
// ============================================================================

void SMixerEffectsRack::wheelEvent(QWheelEvent *event)
{
	if (event->modifiers() & Qt::ShiftModifier) {
		QListWidgetItem *item = m_list->currentItem();
		if (item && (item->flags() & Qt::ItemIsSelectable)) {
			if (event->angleDelta().y() > 0)
				moveFilterUp(item);
			else if (event->angleDelta().y() < 0)
				moveFilterDown(item);
			return;
		}
	}

	QWidget::wheelEvent(event);
}

// ============================================================================
// Accordion Controls
// ============================================================================

void SMixerEffectsRack::toggleFilterControls(QListWidgetItem *item)
{
	if (!m_source || !item) return;

	bool isExpanded = m_controls_items.contains(item);

	collapseAllControls();

	if (isExpanded) return;

	obs_source_t *filter = filterFromItem(item);
	if (!filter) return;

	QWidget *container = m_list->itemWidget(item);
	if (!container) return;

	QWidget *row = container->findChild<QWidget*>("filterRow");
	QWidget *body = container->findChild<QWidget*>("filterBody");
	if (!row || !body) return;

	// Fill body with controls
	auto *controls = new SMixerFilterControls(filter, body);
	body->layout()->addWidget(controls);
	body->setVisible(true);

	// Dynamically resize when controls rebuild (e.g. property visibility changes)
	connect(controls, &SMixerFilterControls::heightChanged, this, [this, item, container]() {
		container->adjustSize();
		QSize hint = container->sizeHint();
		hint.setWidth(0);
		item->setSizeHint(hint);
	});

	// Style container and row
	container->setStyleSheet("#filterContainer { border: 1px solid #00cccc; border-radius: 4px; }");
	row->setStyleSheet("#filterRow { background: #252525; border-bottom: 1px solid #333; border-top-left-radius: 3px; border-top-right-radius: 3px; border-bottom-left-radius: 0px; border-bottom-right-radius: 0px; }");
	body->setStyleSheet("#filterBody { background: #202020; border-bottom-left-radius: 3px; border-bottom-right-radius: 3px; }");

	// Defer initial size calculation so layout settles first
	QTimer::singleShot(0, this, [this, item, container]() {
		container->adjustSize();
		QSize hint = container->sizeHint();
		hint.setWidth(0);
		item->setSizeHint(hint);
	});

	// Track state
	m_controls_items.insert(item, nullptr); 
}

void SMixerEffectsRack::collapseAllControls()
{
	for (auto it = m_controls_items.begin(); it != m_controls_items.end(); ++it) {
		QListWidgetItem *filterItem = it.key();

		QWidget *container = m_list->itemWidget(filterItem);
		if (container) {
			QWidget *row = container->findChild<QWidget*>("filterRow");
			QWidget *body = container->findChild<QWidget*>("filterBody");
			
			if (body) {
				body->setVisible(false);
				QLayoutItem *child;
				while ((child = body->layout()->takeAt(0)) != nullptr) {
					delete child->widget();
					delete child;
				}
			}

			if (row) {
				row->setStyleSheet("#filterRow { border-bottom: 1px solid transparent; border-radius: 4px; }");
				auto *settingsBtn = row->findChild<QPushButton*>("settingsBtn");
				if (settingsBtn) {
					settingsBtn->setProperty("expanded", false);
					settingsBtn->update();
				}
			}

			if (container) {
				container->setStyleSheet("#filterContainer { border: 1px solid transparent; border-radius: 4px; }");
				container->adjustSize();
			}

			QSize hint = container ? container->sizeHint() : QSize(0, 0);
			hint.setWidth(0);
			filterItem->setSizeHint(hint);
		}
	}

	m_controls_items.clear();
	m_controls_map.clear();
}

} // namespace super
