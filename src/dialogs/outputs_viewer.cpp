#include "outputs_viewer.h"
#include <plugin-support.h>
#include <obs-frontend-api.h>

#include <QShowEvent>
#include <QHeaderView>

// Output enumeration callback
static bool output_enum_cb(void *param, obs_output_t *output)
{
	auto *list = static_cast<QList<obs_output_t *> *>(param);
	list->append(output);
	return true;
}

OutputsViewer::OutputsViewer(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(obs_module_text("OutputsViewer.Title"));
	resize(900, 500);
	setupUi();
}

OutputsViewer::~OutputsViewer()
{
}

void OutputsViewer::setupUi()
{
	auto *mainLayout = new QVBoxLayout(this);

	// Info label
	auto *infoLabel = new QLabel(obs_module_text("OutputsViewer.Info"), this);
	infoLabel->setWordWrap(true);
	mainLayout->addWidget(infoLabel);

	// Table
	m_table = new QTableWidget(this);
	m_table->setColumnCount(9);
	m_table->setHorizontalHeaderLabels({
		"Name",
		"ID",
		"Active",
		"Rec", // Reconnecting
		"Video",
		"Audio",
		"Svc", // Service
		"Multi", // Multitrack
		"Enc" // Encoded
	});

	m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	
	// Set fixed width for status/flag columns
	for (int i = 2; i < 9; i++) {
		m_table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed);
		m_table->setColumnWidth(i, 45);
	}

	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->verticalHeader()->setVisible(false);
	m_table->setAlternatingRowColors(true);
	mainLayout->addWidget(m_table);

	// Buttons row
	auto *btnLayout = new QHBoxLayout();
	m_refreshBtn = new QPushButton(obs_module_text("Refresh"), this);
	m_closeBtn = new QPushButton(obs_module_text("Close"), this);

	btnLayout->addWidget(m_refreshBtn);
	btnLayout->addStretch();
	btnLayout->addWidget(m_closeBtn);

	mainLayout->addLayout(btnLayout);

	// Connections
	connect(m_refreshBtn, &QPushButton::clicked, this, &OutputsViewer::refresh);
	connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void OutputsViewer::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	refresh();
}

void OutputsViewer::refresh()
{
	m_table->setRowCount(0);

	// Enumerate all outputs
	QList<obs_output_t *> outputs;
	obs_enum_outputs(output_enum_cb, &outputs);

	for (obs_output_t *output : outputs) {
		addOutputRow(output);
	}
}

void OutputsViewer::addOutputRow(obs_output_t *output)
{
	int row = m_table->rowCount();
	m_table->insertRow(row);

	// Name
	const char *name = obs_output_get_name(output);
	auto *nameItem = new QTableWidgetItem(name ? QString::fromUtf8(name) : "(unnamed)");
	m_table->setItem(row, 0, nameItem);

	// ID
	const char *id = obs_output_get_id(output);
	auto *idItem = new QTableWidgetItem(id ? QString::fromUtf8(id) : "");
	m_table->setItem(row, 1, idItem);

	// Helper for checkmarks
	auto setCheckItem = [&](int col, bool checked) {
		auto *item = new QTableWidgetItem(checked ? "✓" : "");
		item->setTextAlignment(Qt::AlignCenter);
		m_table->setItem(row, col, item);
	};

	// Status
	setCheckItem(2, obs_output_active(output));
	setCheckItem(3, obs_output_reconnecting(output));

	// Flags
	uint32_t flags = obs_output_get_flags(output);
	setCheckItem(4, flags & OBS_OUTPUT_VIDEO);
	setCheckItem(5, flags & OBS_OUTPUT_AUDIO);
	setCheckItem(6, flags & OBS_OUTPUT_SERVICE);
	setCheckItem(7, flags & OBS_OUTPUT_MULTI_TRACK);
	setCheckItem(8, flags & OBS_OUTPUT_ENCODED);
}
