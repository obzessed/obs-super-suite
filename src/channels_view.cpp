#include "channels_view.h"
#include "asio_config.h" 
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QIcon>

ChannelsView::ChannelsView(QWidget *parent) : QDialog(parent)
{
	setWindowTitle("Output Channels");
	resize(600, 500);
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
	auto *infoLabel = new QLabel("This view shows all OBS Output Channels and their assigned sources.\n"
		"Channels are 1-indexed for display, but 0-indexed internally.", this);
	infoLabel->setStyleSheet("color: #888;");
	layout->addWidget(infoLabel);
	
	// Table
	m_table = new QTableWidget(this);
	m_table->setColumnCount(4);
	m_table->setHorizontalHeaderLabels({"Channel", "Source", "Properties", "Filters"});
	m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
	m_table->setColumnWidth(2, 30);
	m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
	m_table->setColumnWidth(3, 30);
	m_table->verticalHeader()->setVisible(false);
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	layout->addWidget(m_table);
	
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
	m_table->setRowCount(0);
	
	for (int i = 0; i < MAX_CHANNELS; i++) {
		obs_source_t *source = obs_get_output_source(i);
		addRow(i, source);
		if (source) obs_source_release(source);
	}
}

void ChannelsView::addRow(int channel, obs_source_t *source)
{
	int row = m_table->rowCount();
	m_table->insertRow(row);
	
	// Channel (1-indexed for display)
	auto *chanItem = new QTableWidgetItem(QString::number(channel + 1));
	chanItem->setTextAlignment(Qt::AlignCenter);
	m_table->setItem(row, 0, chanItem);
	
	if (!source) {
		auto *emptyItem = new QTableWidgetItem("- Empty -");
		emptyItem->setForeground(QBrush(QColor(100, 100, 100)));
		m_table->setItem(row, 1, emptyItem);
		return;
	}
	
	// Source Name
	QString name = obs_source_get_name(source);
	auto *nameItem = new QTableWidgetItem(name);
	m_table->setItem(row, 1, nameItem);
	
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
	m_table->setCellWidget(row, 2, propWidget);
	
	connect(propBtn, &QPushButton::clicked, this, [this, name]() {
		obs_source_t *s = obs_get_source_by_name(name.toUtf8().constData());
		if (s) {
			obs_frontend_open_source_properties(s);
			obs_source_release(s);
		} else {
			refresh(); // Source might be gone
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
	m_table->setCellWidget(row, 3, filterWidget);
	
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
