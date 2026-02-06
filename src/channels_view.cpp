#include "channels_view.h"
#include "asio_config.h" 
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QIcon>
#include <QTreeWidget>

static bool canvas_enum_cb(void *param, obs_canvas_t *canvas) {
	auto *view = static_cast<ChannelsView*>(param);
	view->addCanvasGroup(canvas);
	return true;
}

ChannelsView::ChannelsView(QWidget *parent) : QDialog(parent)
{
	setWindowTitle("Output Channels");
	resize(700, 600);
	setupUi();
}

ChannelsView::~ChannelsView()
{
}

void ChannelsView::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	refresh();
}

void ChannelsView::setupUi()
{
	auto *layout = new QVBoxLayout(this);
	
	// Description
	// auto *infoLabel = new QLabel("Browsing visual channels (canvases) and their output channels.\n"
	// 	"Grouped by Canvas.", this);
	// infoLabel->setStyleSheet("color: #888;");
	// layout->addWidget(infoLabel);
	
	// Tree
	m_tree = new QTreeWidget(this);
	m_tree->setColumnCount(6);
	m_tree->setHeaderLabels({"Channel", "Source", "Audio", "Video", "Properties", "Filters"});
	m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_tree->header()->setSectionResizeMode(2, QHeaderView::Fixed);
	m_tree->header()->setSectionResizeMode(3, QHeaderView::Fixed);
	m_tree->header()->setSectionResizeMode(4, QHeaderView::Fixed);
	m_tree->header()->setSectionResizeMode(5, QHeaderView::Fixed);
	m_tree->header()->setStretchLastSection(false);
	m_tree->setColumnWidth(2, 50);
	m_tree->setColumnWidth(3, 50);
	m_tree->setColumnWidth(4, 40);
	m_tree->setColumnWidth(5, 40);
	m_tree->setSelectionMode(QAbstractItemView::NoSelection);
	// m_tree->setAlternatingRowColors(true); // Maybe?
	
	layout->addWidget(m_tree);
	
	// Buttons
	auto *btnLayout = new QHBoxLayout();
	m_refreshBtn = new QPushButton("Refresh", this);
	m_closeBtn = new QPushButton("Close", this);
	
	btnLayout->addWidget(m_refreshBtn);
	btnLayout->addStretch();
	btnLayout->addWidget(m_closeBtn);
	layout->addLayout(btnLayout);
	
	connect(m_refreshBtn, &QPushButton::clicked, this, &ChannelsView::refresh);
	connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void ChannelsView::refresh()
{
	m_tree->clear();
	
	// Enumerate canvases
	obs_enum_canvases(canvas_enum_cb, this);
	
	m_tree->expandAll();
}

void ChannelsView::addCanvasGroup(obs_canvas_t *canvas)
{
	// Determine canvas name or index
	int index = m_tree->topLevelItemCount();
	const char *cname = obs_canvas_get_name(canvas);
	QString title = cname ? QString("Canvas: %1").arg(cname) : QString("Canvas %1").arg(index + 1);
	
	auto *item = new QTreeWidgetItem(m_tree);
	item->setText(0, title);
	// Make it span all columns or just bold?
	QFont font = item->font(0);
	font.setBold(true);
	item->setFont(0, font);
	item->setFirstColumnSpanned(true); // Span across all columns for clarity
	
	// Iterate output channels for this canvas
	for (int i = 0; i < MAX_CHANNELS; i++) {
		obs_source_t *source = obs_canvas_get_channel(canvas, i);
		addChannelItem(item, i, source);
		if (source) obs_source_release(source);
	}
}

void ChannelsView::addChannelItem(QTreeWidgetItem *parent, int channel, obs_source_t *source)
{
	auto *item = new QTreeWidgetItem(parent);
	
	// Channel (0-indexed internally, show 0-63 internally but display 1-64 per user pref)
	item->setText(0, QString::number(channel + 1));
	item->setTextAlignment(0, Qt::AlignCenter);
	
	if (!source) {
		item->setText(1, "- Empty -");
		item->setForeground(1, QBrush(QColor(100, 100, 100)));
		return;
	}
	
	QString name = obs_source_get_name(source);
	item->setText(1, name);
	
	// Check output flags for audio/video capability
	uint32_t outputFlags = obs_source_get_output_flags(source);
	bool hasAudio = (outputFlags & OBS_SOURCE_AUDIO) != 0;
	bool hasVideo = (outputFlags & OBS_SOURCE_VIDEO) != 0;
	
	// Audio column
	item->setText(2, hasAudio ? "✓" : "-");
	item->setTextAlignment(2, Qt::AlignCenter);
	if (!hasAudio) item->setForeground(2, QBrush(QColor(100, 100, 100)));
	
	// Video column
	item->setText(3, hasVideo ? "✓" : "-");
	item->setTextAlignment(3, Qt::AlignCenter);
	if (!hasVideo) item->setForeground(3, QBrush(QColor(100, 100, 100)));
	
	// Add buttons using QTreeWidget::setItemWidget
	// Properties Button
	auto *propWidget = new QWidget();
	auto *propLayout = new QHBoxLayout(propWidget);
	propLayout->setContentsMargins(0, 0, 0, 0);
	propLayout->setAlignment(Qt::AlignCenter);
	auto *propBtn = new QPushButton();
	propBtn->setProperty("toolButton", true);
	propBtn->setIcon(QIcon(":/super/assets/icons/settings.svg"));
	propBtn->setStyleSheet("QPushButton { background: transparent; border: none; }");
	propBtn->setToolTip("Properties");
	propLayout->addWidget(propBtn);
	
	m_tree->setItemWidget(item, 4, propWidget);
	
	connect(propBtn, &QPushButton::clicked, this, [this, name]() {
		obs_source_t *s = obs_get_source_by_name(name.toUtf8().constData());
		if (s) {
			obs_frontend_open_source_properties(s);
			obs_source_release(s);
		} else {
			refresh(); 
		}
	});

	// Filters Button
	auto *filterWidget = new QWidget();
	auto *filterLayout = new QHBoxLayout(filterWidget);
	filterLayout->setContentsMargins(0, 0, 0, 0);
	filterLayout->setAlignment(Qt::AlignCenter);
	auto *filterBtn = new QPushButton();
	filterBtn->setProperty("toolButton", true);
	filterBtn->setIcon(QIcon(":/super/assets/icons/sliders.svg"));
	filterBtn->setStyleSheet("QPushButton { background: transparent; border: none; }");
	filterBtn->setToolTip("Filters");
	filterLayout->addWidget(filterBtn);
	
	m_tree->setItemWidget(item, 5, filterWidget);
	
	connect(filterBtn, &QPushButton::clicked, this, [this, name]() {
		obs_source_t *s = obs_get_source_by_name(name.toUtf8().constData());
		if (s) {
			obs_frontend_open_source_filters(s);
			obs_source_release(s);
		} else {
			refresh();
		}
	});
}
