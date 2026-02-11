#include "sourcerer_item.hpp"
#include <QVBoxLayout>
#include <QPainter>
#include <QMouseEvent>
#include <obs-module.h>

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
}

SourcererItem::~SourcererItem()
{
	if (display && display->GetDisplay()) {
		obs_display_remove_draw_callback(display->GetDisplay(), SourcererItem::DrawPreview, this);
	}
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
	}

	if (isProgram) {
		borderColor = Qt::red;
		borderWidth = 4;
	} else if (isSelected) {
		borderColor = Qt::blue;
		borderWidth = 4;
	} else {
		// Default
		borderColor = QColor(60, 60, 60); // Dark Gray
		borderWidth = 1;
	}

	p.setPen(QPen(borderColor, borderWidth));
	p.drawRoundedRect(r, radius, radius);
}

void SourcererItem::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		emit Clicked(this);
	}
	QWidget::mousePressEvent(event);
}

void SourcererItem::mouseDoubleClickEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		emit DoubleClicked(this);
	}
	QWidget::mouseDoubleClickEvent(event);
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
