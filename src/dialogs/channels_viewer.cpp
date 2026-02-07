#include "./channels_viewer.h"
#include "../models/audio_channel_source_config.h"
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QIcon>
#include <QTreeWidget>

static bool canvas_enum_cb(void *param, obs_canvas_t *canvas) {
	auto *view = static_cast<ChannelsDialog*>(param);
	view->addCanvasGroup(canvas);
	return true;
}

ChannelsDialog::ChannelsDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle("Output Channels");
	resize(700, 600);
	setupUi();
}

ChannelsDialog::~ChannelsDialog()
{
}

void ChannelsDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	refresh();
}

void ChannelsDialog::setupUi()
{
	auto *layout = new QVBoxLayout(this);
	
	// Tree
	m_tree = new QTreeWidget(this);
	m_tree->setColumnCount(8);
	m_tree->setHeaderLabels({"Channel", "Source", "Source ID", "Source Type", "Audio", "Video", "Properties", "Filters"});
	m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	m_tree->header()->setSectionResizeMode(4, QHeaderView::Fixed);
	m_tree->header()->setSectionResizeMode(5, QHeaderView::Fixed);
	m_tree->header()->setSectionResizeMode(6, QHeaderView::Fixed);
	m_tree->header()->setSectionResizeMode(7, QHeaderView::Fixed);
	m_tree->header()->setStretchLastSection(false);
	m_tree->setColumnWidth(4, 50);
	m_tree->setColumnWidth(5, 50);
	m_tree->setColumnWidth(6, 40);
	m_tree->setColumnWidth(7, 40);
	m_tree->setSelectionMode(QAbstractItemView::NoSelection);
	m_tree->setAlternatingRowColors(true);
	
	layout->addWidget(m_tree);
	
	// Buttons
	auto *btnLayout = new QHBoxLayout();
	m_refreshBtn = new QPushButton("Refresh", this);
	m_closeBtn = new QPushButton("Close", this);
	
	btnLayout->addWidget(m_refreshBtn);
	btnLayout->addStretch();
	btnLayout->addWidget(m_closeBtn);
	layout->addLayout(btnLayout);
	
	connect(m_refreshBtn, &QPushButton::clicked, this, &ChannelsDialog::refresh);
	connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void ChannelsDialog::refresh()
{
	m_tree->clear();
	
	// Enumerate canvases
	obs_enum_canvases(canvas_enum_cb, this);
	
	m_tree->expandAll();
}

void ChannelsDialog::addCanvasGroup(obs_canvas_t *canvas)
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

void ChannelsDialog::addChannelItem(QTreeWidgetItem *parent, int channel, obs_source_t *source)
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
	
	// Source ID column (obs_source_get_id)
	const char *sourceId = obs_source_get_id(source);
	item->setText(2, sourceId ? sourceId : "");
	item->setForeground(2, QBrush(QColor(130, 130, 130)));
	
	// Source Type column (obs_source_get_type)
	obs_source_type type = obs_source_get_type(source);
	QString typeStr;
	switch (type) {
		case OBS_SOURCE_TYPE_INPUT: typeStr = "Input"; break;
		case OBS_SOURCE_TYPE_FILTER: typeStr = "Filter"; break;
		case OBS_SOURCE_TYPE_TRANSITION: typeStr = "Transition"; break;
		case OBS_SOURCE_TYPE_SCENE: typeStr = "Scene"; break;
		default: typeStr = "Unknown"; break;
	}
	item->setText(3, typeStr);
	item->setForeground(3, QBrush(QColor(130, 130, 130)));
	
	// Check output flags for audio/video capability
	uint32_t outputFlags = obs_source_get_output_flags(source);
	bool hasAudio = (outputFlags & OBS_SOURCE_AUDIO) != 0;
	bool hasVideo = (outputFlags & OBS_SOURCE_VIDEO) != 0;
	
	// Audio column (now column 4)
	item->setText(4, hasAudio ? "✓" : "-");
	item->setTextAlignment(4, Qt::AlignCenter);
	if (!hasAudio) item->setForeground(4, QBrush(QColor(100, 100, 100)));
	
	// Video column (now column 5)
	item->setText(5, hasVideo ? "✓" : "-");
	item->setTextAlignment(5, Qt::AlignCenter);
	if (!hasVideo) item->setForeground(5, QBrush(QColor(100, 100, 100)));
	
	// Add buttons using QTreeWidget::setItemWidget
	// Properties Button (now column 6)
	auto *propWidget = new QWidget();
	auto *propLayout = new QHBoxLayout(propWidget);
	propLayout->setContentsMargins(0, 0, 0, 0);
	propLayout->setAlignment(Qt::AlignCenter);
	auto *propBtn = new QPushButton();
	propBtn->setProperty("toolButton", true);
	propBtn->setIcon(QIcon(":/super/assets/icons/settings.svg"));
	propBtn->setToolTip("Properties");
	
	// Disable if source has no configurable properties
	if (!obs_source_configurable(source)) {
		propBtn->setEnabled(false);
		propBtn->setToolTip("No configurable properties");
	}
	
	propLayout->addWidget(propBtn);
	
	m_tree->setItemWidget(item, 6, propWidget);
	
	connect(propBtn, &QPushButton::clicked, this, [this, name]() {
		obs_source_t *s = obs_get_source_by_name(name.toUtf8().constData());
		if (s) {
			obs_frontend_open_source_properties(s);
			obs_source_release(s);
		} else {
			refresh(); 
		}
	});

	// Filters Button (now column 7)
	auto *filterWidget = new QWidget();
	auto *filterLayout = new QHBoxLayout(filterWidget);
	filterLayout->setContentsMargins(0, 0, 0, 0);
	filterLayout->setAlignment(Qt::AlignCenter);
	auto *filterBtn = new QPushButton();
	filterBtn->setProperty("toolButton", true);
	filterBtn->setIcon(QIcon(":/super/assets/icons/sliders.svg"));
	filterBtn->setToolTip("Filters");
	
	// Disable filter button for non-input sources
	if (type != OBS_SOURCE_TYPE_INPUT && type != OBS_SOURCE_TYPE_SCENE) {
		filterBtn->setEnabled(false);
		filterBtn->setToolTip("Filters not available for this source type");
	}
	
	filterLayout->addWidget(filterBtn);
	
	m_tree->setItemWidget(item, 7, filterWidget);
	
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
