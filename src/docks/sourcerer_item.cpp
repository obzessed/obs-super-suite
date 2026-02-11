#include "sourcerer_item.hpp"
#include <QVBoxLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <obs-module.h>
#include <obs-frontend-api.h>

class SourcererDisplay : public OBSQTDisplay {
public:
	double aspectRatio = 16.0 / 9.0;

	SourcererDisplay(QWidget *parent) : OBSQTDisplay(parent)
	{
		struct obs_video_info ovi;
		if (obs_get_video_info(&ovi)) {
			aspectRatio = (double)ovi.base_width / (double)ovi.base_height;
		}

		QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
		policy.setHeightForWidth(true);
		setSizePolicy(policy);
	}

	virtual bool hasHeightForWidth() const override { return true; }
	virtual int heightForWidth(int width) const override { return (int)(width / aspectRatio); }

	// Paint background/border via stylesheet or paintEvent?
	// OBSQTDisplay is a native window usually, so stylesheets might be tricky for rounded corners
	// unless we mask it or use a container.
	// But let's try to paint a border on the PARENT (SourcererItem).
};

SourcererItem::SourcererItem(obs_source_t *source, QWidget *parent) : QWidget(parent), source(source)
{
	// Increment reference to ensure source stays alive while this widget exists
	obs_source_get_ref(source);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->setSpacing(2);

	display = new SourcererDisplay(this);
	display->setMinimumSize(120, 60);

	// Transparent for mouse events so parent handles clicks
	display->setAttribute(Qt::WA_TransparentForMouseEvents);

	label = new QLabel(this);

	label->setAlignment(Qt::AlignCenter);
	label->setWordWrap(true);

	layout->addWidget(display);
	layout->addWidget(label);

	UpdateName();

	// Initialize display
	auto OnDisplayCreated = [this](OBSQTDisplay *w) {
		if (w != display)
			return;
		obs_display_add_draw_callback(display->GetDisplay(), SourcererItem::DrawPreview, this);
	};

	connect(display, &OBSQTDisplay::DisplayCreated, this, OnDisplayCreated);

	// Create the display now (or it happens on show)
	display->CreateDisplay();

	signal_handler_t *sh = obs_source_get_signal_handler(source);
	signal_handler_connect(sh, "rename", SourceRenamed, this);
	signal_handler_connect(sh, "enable", SourceEnabled, this);
	signal_handler_connect(sh, "disable", SourceDisabled, this);
}

SourcererItem::~SourcererItem()
{
	if (display && display->GetDisplay()) {
		obs_display_remove_draw_callback(display->GetDisplay(), SourcererItem::DrawPreview, this);
	}

	signal_handler_t *sh = obs_source_get_signal_handler(source);
	signal_handler_disconnect(sh, "rename", SourceRenamed, this);
	signal_handler_disconnect(sh, "enable", SourceEnabled, this);
	signal_handler_disconnect(sh, "disable", SourceDisabled, this);

	obs_source_release(source);
}

void SourcererItem::SetItemWidth(int width)
{
	if (display) {
		int height = display->heightForWidth(width);
		display->setFixedSize(width, height);
	}
}

void SourcererItem::UpdateName()
{
	if (source) {
		const char *name = obs_source_get_name(source);
		label->setText(QString::fromUtf8(name));
	}
}

void SourcererItem::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
}

void SourcererItem::SetSelected(bool selected)
{
	if (isSelected == selected)
		return;
	isSelected = selected;
	update(); // Trigger repaint
}

void SourcererItem::SetProgram(bool program)
{
	if (isProgram == program)
		return;
	isProgram = program;
	update();
}

void SourcererItem::SetSceneItemVisible(bool visible)
{
	if (isSceneItemVisible == visible)
		return;
	isSceneItemVisible = visible;
	UpdateStatus();
}

void SourcererItem::UpdateStatus()
{
	bool active = isSourceEnabled && isSceneItemVisible;
	if (label) {
		label->setEnabled(active);
	}

	update();
}

void SourcererItem::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);

	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	// Default border rect (inset slightly to avoid clipping)
	QRect r = rect().adjusted(1, 1, -1, -1);
	int radius = 4;
	int borderWidth = 1;
	QColor borderColor = palette().color(QPalette::Mid);

	if (isProgram && isSelected) {
		// Combined: Blue outside, Red inside
		p.setPen(QPen(Qt::blue, 2));
		p.drawRoundedRect(r, radius, radius);

		p.setPen(QPen(Qt::red, 2));
		p.drawRoundedRect(r.adjusted(2, 2, -2, -2), radius - 1, radius - 1);
		return;
	} else if (isProgram) {
		borderColor = Qt::red;
		borderWidth = 4;
		p.setPen(QPen(borderColor, borderWidth));
		p.drawRoundedRect(r, radius, radius);
	} else if (isSelected) {
		borderColor = Qt::blue;
		borderWidth = 4;
		p.setPen(QPen(borderColor, borderWidth));
		p.drawRoundedRect(r, radius, radius);
	} else {
		// Default
		borderColor = QColor(60, 60, 60); // Dark Gray
		borderWidth = 1;
		p.setPen(QPen(borderColor, borderWidth));
		p.drawRoundedRect(r, radius, radius);
	}
}

void SourcererItem::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		emit Clicked(this);
		event->accept();
		return;
	}
	QWidget::mousePressEvent(event);
}

void SourcererItem::mouseDoubleClickEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		emit DoubleClicked(this);
		event->accept();
		return;
	}
	QWidget::mouseDoubleClickEvent(event);
}

void SourcererItem::contextMenuEvent(QContextMenuEvent *event)
{
	QMenu menu(this);

	// Standard Source Actions
	QAction *renameAction = menu.addAction(tr("Rename"));
	connect(renameAction, &QAction::triggered, [this]() {
		if (!source)
			return;
		const char *oldName = obs_source_get_name(source);
		bool ok;
		QString newName = QInputDialog::getText(this, tr("Rename Source"), tr("Name:"), QLineEdit::Normal,
							QString::fromUtf8(oldName), &ok);
		if (ok && !newName.isEmpty()) {
			obs_source_set_name(source, newName.toUtf8().constData());
		}
	});

	menu.addSeparator();

	QAction *filtersAction = menu.addAction(tr("Filters"));
	connect(filtersAction, &QAction::triggered, [this]() {
		if (source)
			obs_frontend_open_source_filters(source);
	});

	QAction *propsAction = menu.addAction(tr("Properties"));
	connect(propsAction, &QAction::triggered, [this]() {
		if (source)
			obs_frontend_open_source_properties(source);
	});

	// Allow parent to add items (e.g., "Hide from Scene")
	emit MenuRequested(this, &menu);

	menu.exec(event->globalPos());
}

void SourcererItem::DrawPreview(void *data, uint32_t cx, uint32_t cy)
{
	SourcererItem *item = static_cast<SourcererItem *>(data);
	if (!item || !item->source)
		return;

	obs_source_t *source = item->source;

	uint32_t sourceCX = obs_source_get_width(source);
	uint32_t sourceCY = obs_source_get_height(source);

	if (!sourceCX || !sourceCY)
		return;

	float scaleX = (float)cx / (float)sourceCX;
	float scaleY = (float)cy / (float)sourceCY;
	float scale = (scaleX < scaleY) ? scaleX : scaleY;

	// Center the source
	float newWidth = (float)sourceCX * scale;
	float newHeight = (float)sourceCY * scale;
	float x = ((float)cx - newWidth) * 0.5f;
	float y = ((float)cy - newHeight) * 0.5f;

	gs_matrix_push();
	gs_matrix_translate3f(x, y, 0.0f);
	gs_matrix_scale3f(scale, scale, 1.0f);

	obs_source_video_render(source);

	gs_matrix_pop();
}

void SourcererItem::SourceRenamed(void *data, calldata_t *cd)
{
	Q_UNUSED(cd);
	SourcererItem *item = static_cast<SourcererItem *>(data);
	QMetaObject::invokeMethod(item, "UpdateName", Qt::QueuedConnection);
}

void SourcererItem::SourceEnabled(void *data, calldata_t *cd)
{
	Q_UNUSED(cd);
	SourcererItem *item = static_cast<SourcererItem *>(data);
	QMetaObject::invokeMethod(
		item,
		[item]() {
			item->isSourceEnabled = true;
			item->UpdateStatus();
		},
		Qt::QueuedConnection);
}

void SourcererItem::SourceDisabled(void *data, calldata_t *cd)
{
	Q_UNUSED(cd);
	SourcererItem *item = static_cast<SourcererItem *>(data);
	QMetaObject::invokeMethod(
		item,
		[item]() {
			item->isSourceEnabled = false;
			item->UpdateStatus();
		},
		Qt::QueuedConnection);
}