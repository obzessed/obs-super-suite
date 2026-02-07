#include "./canvas_manager.h"
#include <plugin-support.h>
#include <obs-frontend-api.h>

#include <QShowEvent>
#include <QHeaderView>
#include <QMessageBox>

#define OBS_CANVAS_PROGRAM (obs_canvas_flags::PROGRAM)
#define OBS_CANVAS_PREVIEW (obs_canvas_flags::PREVIEW)
#define OBS_CANVAS_DEVICE (obs_canvas_flags::DEVICE)

#define OBS_CANVAS_MAIN (obs_canvas_flags::MAIN)
#define OBS_CANVAS_MIX_AUDIO (obs_canvas_flags::MIX_AUDIO)
#define OBS_CANVAS_EPHEMERAL (obs_canvas_flags::EPHEMERAL)

// Canvas enumeration callback
static bool canvas_enum_cb(void *param, obs_canvas_t *canvas)
{
	auto *list = static_cast<QList<obs_canvas_t *> *>(param);
	list->append(canvas);
	return true;
}

CanvasManager::CanvasManager(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(obs_module_text("CanvasManager.Title"));
	resize(800, 400);
	setupUi();
}

CanvasManager::~CanvasManager()
{
}

void CanvasManager::setupUi()
{
	auto *mainLayout = new QVBoxLayout(this);

	// Info label
	auto *infoLabel = new QLabel(obs_module_text("CanvasManager.Info"), this);
	infoLabel->setWordWrap(true);
	mainLayout->addWidget(infoLabel);

	// Table
	m_table = new QTableWidget(this);
	m_table->setColumnCount(11);
	m_table->setHorizontalHeaderLabels({
		"Name",
		"UUID",
		"Base",
		"Output",
		"FPS",
		"Type",
		"Prog",    // OBS_CANVAS_PROGRAM
		"Prev",    // OBS_CANVAS_PREVIEW
		"Dev",     // OBS_CANVAS_DEVICE
		"Mix",     // OBS_CANVAS_MIX_AUDIO
		"Eph"      // OBS_CANVAS_EPHEMERAL
	});
	m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
	m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
	for (int i = 6; i < 11; i++) {
		m_table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed);
		m_table->setColumnWidth(i, 35);
	}
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->verticalHeader()->setVisible(false);
	m_table->setAlternatingRowColors(true);
	mainLayout->addWidget(m_table);

	connect(m_table, &QTableWidget::itemSelectionChanged, this, &CanvasManager::updateButtonStates);

	// Buttons row
	auto *btnLayout = new QHBoxLayout();

	m_addBtn = new QPushButton(obs_module_text("CanvasManager.Add"), this);
	m_editBtn = new QPushButton(obs_module_text("CanvasManager.Edit"), this);
	m_removeBtn = new QPushButton(obs_module_text("CanvasManager.Remove"), this);
	m_refreshBtn = new QPushButton(obs_module_text("CanvasManager.Refresh"), this);
	m_closeBtn = new QPushButton(obs_module_text("Cancel"), this);

	btnLayout->addWidget(m_addBtn);
	btnLayout->addWidget(m_editBtn);
	btnLayout->addWidget(m_removeBtn);
	btnLayout->addStretch();
	btnLayout->addWidget(m_refreshBtn);
	btnLayout->addWidget(m_closeBtn);

	mainLayout->addLayout(btnLayout);

	// Connections
	connect(m_addBtn, &QPushButton::clicked, this, &CanvasManager::addCanvas);
	connect(m_editBtn, &QPushButton::clicked, this, &CanvasManager::editCanvas);
	connect(m_removeBtn, &QPushButton::clicked, this, &CanvasManager::removeCanvas);
	connect(m_refreshBtn, &QPushButton::clicked, this, &CanvasManager::refresh);
	connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);

	updateButtonStates();
}

void CanvasManager::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	refresh();
}

void CanvasManager::refresh()
{
	m_table->setRowCount(0);

	// Enumerate all canvases
	QList<obs_canvas_t *> canvases;
	obs_enum_canvases(canvas_enum_cb, &canvases);

	// Get main canvas for comparison
	obs_canvas_t *mainCanvas = obs_get_main_canvas();

	for (obs_canvas_t *canvas : canvases) {
		int row = m_table->rowCount();
		m_table->insertRow(row);

		// Name
		const char *name = obs_canvas_get_name(canvas);
		auto *nameItem = new QTableWidgetItem(name ? QString::fromUtf8(name) : "(unnamed)");
		m_table->setItem(row, 0, nameItem);

		// UUID
		const char *uuid = obs_canvas_get_uuid(canvas);
		auto *uuidItem = new QTableWidgetItem(uuid ? QString::fromUtf8(uuid).left(8) + "..." : "");
		uuidItem->setToolTip(uuid ? QString::fromUtf8(uuid) : "");
		m_table->setItem(row, 1, uuidItem);

		// Resolution & FPS
		obs_video_info ovi{};
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t outWidth = 0;
		uint32_t outHeight = 0;
		if (obs_canvas_get_video_info(canvas, &ovi)) {
			width = ovi.base_width;
			height = ovi.base_height;
			outWidth = ovi.output_width;
			outHeight = ovi.output_height;

			double fps = (double)ovi.fps_num / (double)ovi.fps_den;
			auto *fpsItem = new QTableWidgetItem(QString::number(fps, 'f', 2));
			m_table->setItem(row, 4, fpsItem);
		} else {
			obs_log(LOG_ERROR, "Failed to get video info");
			auto *fpsItem = new QTableWidgetItem("-");
			m_table->setItem(row, 4, fpsItem);
		}
		auto *resItem = new QTableWidgetItem(QString("%1x%2").arg(width).arg(height));
		m_table->setItem(row, 2, resItem);

		auto *outResItem = new QTableWidgetItem(QString("%1x%2").arg(outWidth).arg(outHeight));
		m_table->setItem(row, 3, outResItem);

		// Type (Main or Custom)
		QString typeStr = canvas == mainCanvas ? "Main" : "Extra";
		auto *typeItem = new QTableWidgetItem(typeStr);
		m_table->setItem(row, 5, typeItem);

		// Flags
		uint32_t flags = obs_canvas_get_flags(canvas);
		auto setFlagItem = [&](int col, uint32_t flag, bool eq) {
			auto *item = new QTableWidgetItem((eq ? flags == flag : flags & flag) ? "✓" : "");
			item->setTextAlignment(Qt::AlignCenter);
			m_table->setItem(row, col, item);
		};
		setFlagItem(6, OBS_CANVAS_PROGRAM, false);   // Program
		setFlagItem(7, OBS_CANVAS_PREVIEW, true);   // Preview
		setFlagItem(8, OBS_CANVAS_DEVICE, true);    // Device
		setFlagItem(9, OBS_CANVAS_MIX_AUDIO, false); // Mix Audio
		setFlagItem(10, OBS_CANVAS_EPHEMERAL, false); // Ephemeral

		// Store UUID in first column for later access
		nameItem->setData(Qt::UserRole, uuid ? QString::fromUtf8(uuid) : QString());
	}

	if (mainCanvas) {
		obs_canvas_release(mainCanvas);
	}

	updateButtonStates();
}

void CanvasManager::updateButtonStates()
{
	bool hasSelection = m_table->currentRow() >= 0;

	// Check if selected canvas is the main canvas (can't remove main)
	bool isMain = false;
	if (hasSelection) {
		auto *typeItem = m_table->item(m_table->currentRow(), 4);
		if (typeItem && typeItem->text() == "Main") {
			isMain = true;
		}
	}

	m_editBtn->setEnabled(hasSelection);
	m_removeBtn->setEnabled(hasSelection && !isMain);
}

void CanvasManager::addCanvas()
{
	// TODO: Implement canvas creation dialog
	QMessageBox::information(this, "Not Implemented", 
		"Canvas creation is not yet implemented.\n\n"
		"Use OBS Settings > Video to add canvases.");
}

void CanvasManager::editCanvas()
{
	if (m_table->currentRow() < 0) return;

	// TODO: Implement canvas editing dialog
	QMessageBox::information(this, "Not Implemented",
		"Canvas editing is not yet implemented.\n\n"
		"Use OBS Settings > Video to edit canvases.");
}

void CanvasManager::removeCanvas()
{
	if (m_table->currentRow() < 0) return;

	auto *nameItem = m_table->item(m_table->currentRow(), 0);
	QString uuid = nameItem->data(Qt::UserRole).toString();
	QString name = nameItem->text();

	if (uuid.isEmpty()) return;

	// Confirm deletion
	auto result = QMessageBox::question(this, 
		obs_module_text("CanvasManager.ConfirmRemove"),
		QString(obs_module_text("CanvasManager.ConfirmRemoveMsg")).arg(name),
		QMessageBox::Yes | QMessageBox::No);

	if (result != QMessageBox::Yes) return;

	// Get canvas by UUID and remove it
	if (obs_canvas_t *canvas = obs_get_canvas_by_uuid(uuid.toUtf8().constData())) {
		obs_canvas_remove(canvas);
		obs_canvas_release(canvas);
		refresh();
	}
}
