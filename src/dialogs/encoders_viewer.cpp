#include "encoders_viewer.h"
#include <plugin-support.h>
#include <obs-frontend-api.h>

#include <QShowEvent>
#include <QHeaderView>

// Encoder enumeration callback
static bool encoder_enum_cb(void *param, obs_encoder_t *encoder)
{
	auto *list = static_cast<QList<obs_encoder_t *> *>(param);
	list->append(encoder);
	return true;
}

EncodersViewer::EncodersViewer(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(obs_module_text("EncodersViewer.Title"));
	resize(800, 500);
	setupUi();
}

EncodersViewer::~EncodersViewer()
{
}

void EncodersViewer::setupUi()
{
	auto *mainLayout = new QVBoxLayout(this);

	// Info label
	auto *infoLabel = new QLabel(obs_module_text("EncodersViewer.Info"), this);
	infoLabel->setWordWrap(true);
	mainLayout->addWidget(infoLabel);

	// Table
	m_table = new QTableWidget(this);
	m_table->setColumnCount(7); // Name, Codec, Type, Width, Height, FPS, Bitrate/Settings
	m_table->setHorizontalHeaderLabels({
		"Name",
		"Codec",
		"Type",
		"Width",
		"Height",
		"FPS",
		"Settings/Info"
	});

	m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
	m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
	m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
	
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
	connect(m_refreshBtn, &QPushButton::clicked, this, &EncodersViewer::refresh);
	connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void EncodersViewer::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	refresh();
}

void EncodersViewer::refresh()
{
	m_table->setRowCount(0);

	// Enumerate all encoders
	QList<obs_encoder_t *> encoders;
	obs_enum_encoders(encoder_enum_cb, &encoders);

	for (obs_encoder_t *encoder : encoders) {
		addEncoderRow(encoder);
	}
}

void EncodersViewer::addEncoderRow(obs_encoder_t *encoder)
{
	int row = m_table->rowCount();
	m_table->insertRow(row);

	// Name
	const char *name = obs_encoder_get_name(encoder);
	m_table->setItem(row, 0, new QTableWidgetItem(name ? QString::fromUtf8(name) : "(unnamed)"));

	// Codec
	const char *codec = obs_encoder_get_codec(encoder);
	m_table->setItem(row, 1, new QTableWidgetItem(codec ? QString::fromUtf8(codec) : ""));

	// Type
	obs_encoder_type type = obs_encoder_get_type(encoder);
	QString typeStr = (type == OBS_ENCODER_VIDEO) ? "Video" : 
	                  (type == OBS_ENCODER_AUDIO) ? "Audio" : "Unknown";
	m_table->setItem(row, 2, new QTableWidgetItem(typeStr));

	// Video properties
	if (type == OBS_ENCODER_VIDEO) {
		uint32_t width = obs_encoder_get_width(encoder);
		uint32_t height = obs_encoder_get_height(encoder);
		m_table->setItem(row, 3, new QTableWidgetItem(QString::number(width)));
		m_table->setItem(row, 4, new QTableWidgetItem(QString::number(height)));

		// FPS is tricky as it's often not directly exposed on encoder active/inactive
		// But we can try getting settings or video info if attached
		// For now, leave empty or put "-"
		
		obs_video_info ovi;
		if (obs_get_video_info(&ovi)) {
			// This is global video info, not necessarily this encoder's scaled output
			// But usually encoders encode at canvas or scaled res
		}
		
		m_table->setItem(row, 5, new QTableWidgetItem("-"));

		// Settings/Info
		obs_data_t *settings = obs_encoder_get_settings(encoder);
		if (settings) {
			const char *json = obs_data_get_json(settings);
			QString jsonStr = json ? QString::fromUtf8(json) : "";
			auto *item = new QTableWidgetItem(jsonStr);
			item->setToolTip(jsonStr);
			m_table->setItem(row, 6, item);
			obs_data_release(settings);
		}
	} else {
		// Audio properties
		uint32_t sampleRate = obs_encoder_get_sample_rate(encoder);
		m_table->setItem(row, 3, new QTableWidgetItem("-"));
		m_table->setItem(row, 4, new QTableWidgetItem("-"));
		m_table->setItem(row, 5, new QTableWidgetItem(QString::number(sampleRate) + " Hz"));
		
		// Settings/Info
		obs_data_t *settings = obs_encoder_get_settings(encoder);
		if (settings) {
			const char *json = obs_data_get_json(settings);
			QString jsonStr = json ? QString::fromUtf8(json) : "";
			auto *item = new QTableWidgetItem(jsonStr);
			item->setToolTip(jsonStr);
			m_table->setItem(row, 6, item);
			obs_data_release(settings);
		}
	}
}
