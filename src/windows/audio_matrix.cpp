#include "audio_matrix.hpp"

#include <QVBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QCheckBox>
#include <QPair>
#include <QList>
#include <QMap>

AudioMatrix::AudioMatrix(QWidget *parent) : QDialog(parent)
{
	setWindowTitle("Audio Matrix Router");
	resize(800, 600);
	SetupUi();
	FullRefresh();
}

AudioMatrix::~AudioMatrix()
{
	ConnectGlobalSignals(false);
}

void AudioMatrix::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	ConnectGlobalSignals(true);
	FullRefresh();
}

void AudioMatrix::hideEvent(QHideEvent *event)
{
	QDialog::hideEvent(event);
	ConnectGlobalSignals(false);

	// Disconnect from all sources to avoid overhead/crashes
	obs_enum_all_sources(
		[](void *param, obs_source_t *source) {
			auto *self = static_cast<AudioMatrix *>(param);
			self->ConnectSource(source, false);
			return true;
		},
		this);
}

void AudioMatrix::ConnectGlobalSignals(bool connect)
{
	signal_handler_t *sh = obs_get_signal_handler();
	if (connect) {
		signal_handler_connect(sh, "source_create", OBS_SourceCreated, this);
		signal_handler_connect(sh, "source_remove", OBS_SourceRemoved, this);
		signal_handler_connect(sh, "source_rename", OBS_SourceRenamed, this);
	} else {
		signal_handler_disconnect(sh, "source_create", OBS_SourceCreated, this);
		signal_handler_disconnect(sh, "source_remove", OBS_SourceRemoved, this);
		signal_handler_disconnect(sh, "source_rename", OBS_SourceRenamed, this);
	}
}

void AudioMatrix::ConnectSource(obs_source_t *source, bool connect)
{
	signal_handler_t *sh = obs_source_get_signal_handler(source);
	if (!sh)
		return;

	if (connect) {
		signal_handler_connect(sh, "audio_mixers", OBS_SourceAudioMixers, this);
	} else {
		signal_handler_disconnect(sh, "audio_mixers", OBS_SourceAudioMixers, this);
	}
}

void AudioMatrix::OBS_SourceCreated(void *data, calldata_t *cd)
{
	AudioMatrix *window = static_cast<AudioMatrix *>(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	const char *name = obs_source_get_name(source);
	QString qName = QString::fromUtf8(name);
	QMetaObject::invokeMethod(window, "SourceCreated", Qt::QueuedConnection, Q_ARG(QString, qName));
}

void AudioMatrix::OBS_SourceRemoved(void *data, calldata_t *cd)
{
	AudioMatrix *window = static_cast<AudioMatrix *>(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	const char *name = obs_source_get_name(source);
	QString qName = QString::fromUtf8(name);
	QMetaObject::invokeMethod(window, "SourceRemoved", Qt::QueuedConnection, Q_ARG(QString, qName));
}

void AudioMatrix::OBS_SourceRenamed(void *data, calldata_t *cd)
{
	AudioMatrix *window = static_cast<AudioMatrix *>(data);
	const char *newName = calldata_string(cd, "new_name");
	const char *oldName = calldata_string(cd, "prev_name");
	QMetaObject::invokeMethod(window, "SourceRenamed", Qt::QueuedConnection,
				  Q_ARG(QString, QString::fromUtf8(oldName)),
				  Q_ARG(QString, QString::fromUtf8(newName)));
}

void AudioMatrix::OBS_SourceAudioMixers(void *data, calldata_t *cd)
{
	AudioMatrix *window = static_cast<AudioMatrix *>(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	uint32_t mixers = (uint32_t)calldata_int(cd, "mixers");
	const char *name = obs_source_get_name(source);

	QMetaObject::invokeMethod(window, "SourceMixersChanged", Qt::QueuedConnection,
				  Q_ARG(QString, QString::fromUtf8(name)), Q_ARG(uint32_t, mixers));
}

// Helper for filter enumeration
struct FilterEnumData {
    AudioMatrix *self;
    QTreeWidgetItem *parentItem;
};

void AudioMatrix::SetupUi()
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	m_tree = new QTreeWidget(this);
	m_tree->setColumnCount(9); // Name, ID, Type, 6 Tracks
	m_tree->setHeaderLabels({"Source", "ID", "Type", "1", "2", "3", "4", "5", "6"});
	m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	m_tree->setIndentation(20);
    m_tree->setRootIsDecorated(true);
    m_tree->setItemsExpandable(true);
    
	for (int i = 3; i < 9; i++) {
		m_tree->header()->setSectionResizeMode(i, QHeaderView::Fixed);
		m_tree->setColumnWidth(i, 30);
	}

	connect(m_tree, &QTreeWidget::itemChanged, this, &AudioMatrix::OnItemChanged);

	layout->addWidget(m_tree);
}

void AudioMatrix::FullRefresh()
{
	m_updating = true;
	m_tree->clear();
	m_tree->blockSignals(true);

	// Enumerate ALL sources (Inputs, Scenes, Groups, etc.)
    // We use obs_enum_all_sources to ensure we catch everything, including inactive ones?
    // User wants "input sources". obs_enum_all_sources includes type checks.
    
	obs_enum_all_sources(
		[](void *param, obs_source_t *source) {
            auto *self = static_cast<AudioMatrix *>(param);

            // Skip filters in main loop (they will be added via parents)
            if (obs_source_get_type(source) == OBS_SOURCE_TYPE_FILTER) 
                return true;

            // Filter by audio capability
			auto flags = obs_source_get_output_flags(source);
			if (!(flags & OBS_SOURCE_AUDIO)) return true;

            // Create Parent Item
            const char *name = obs_source_get_name(source);
            const char *id = obs_source_get_id(source);
            obs_source_type type = obs_source_get_type(source);
            const char *typeStr = "Unknown";
            if (type == OBS_SOURCE_TYPE_INPUT) typeStr = "Input";
            else if (type == OBS_SOURCE_TYPE_TRANSITION) typeStr = "Transition";
            else if (type == OBS_SOURCE_TYPE_SCENE) typeStr = "Scene";
            
            uint32_t mixers = obs_source_get_audio_mixers(source);
            
            QTreeWidgetItem *item = new QTreeWidgetItem(self->m_tree);
            item->setText(0, name);
            item->setData(0, Qt::UserRole, name);
            item->setText(1, id);
            item->setText(2, typeStr);
            item->setExpanded(true); // Default to expanded
            
            for (int i = 0; i < 6; i++) {
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                bool active = (mixers & (1 << i));
                item->setCheckState(i + 3, active ? Qt::Checked : Qt::Unchecked);
            }
            
            self->ConnectSource(source, true);

            // Enumerate Filters for this source
            FilterEnumData filterData = {self, item};
            obs_source_enum_filters(source, [](obs_source_t *, obs_source_t *filter, void *p) {
                auto *fd = static_cast<FilterEnumData*>(p);
                
                // Add Filter Item
                const char *fName = obs_source_get_name(filter);
                const char *fId = obs_source_get_id(filter);
                
                uint32_t fMixers = obs_source_get_audio_mixers(filter);
                
                QTreeWidgetItem *fItem = new QTreeWidgetItem(fd->parentItem);
                fItem->setText(0, fName);
                fItem->setData(0, Qt::UserRole, fName);
                fItem->setText(1, fId);
                fItem->setText(2, "Filter");
                
                for (int j = 0; j < 6; j++) {
                    fItem->setFlags(fItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                    bool fActive = (fMixers & (1 << j));
                    fItem->setCheckState(j + 3, fActive ? Qt::Checked : Qt::Unchecked);
                }
                
                fd->self->ConnectSource(filter, true);
                
            }, &filterData);

			return true;
		},
		this);

	m_tree->blockSignals(false);
	m_updating = false;
    // m_tree->expandAll(); // Removed to rely on setExpanded(true)
}

void AudioMatrix::SourceCreated(const QString &)
{
    // For simplicity with tree structure complexity, just FullRefresh
    // Handling dynamic insertion in hierarchy is error prone without full scan
    FullRefresh();
}

void AudioMatrix::SourceRemoved(const QString &)
{
   FullRefresh();
}

void AudioMatrix::SourceRenamed(const QString &, const QString &)
{
   FullRefresh();
}

void AudioMatrix::SourceMixersChanged(const QString &name, uint32_t mixers)
{
	if (m_updating)
		return;

    m_tree->blockSignals(true);
    
    // Find item
    auto items = m_tree->findItems(name, Qt::MatchExactly | Qt::MatchRecursive, 0);
    for (auto *item : items) {
         if (item->data(0, Qt::UserRole).toString() == name) {
             for (int i = 0; i < 6; i++) {
                 bool active = (mixers & (1 << i));
                 Qt::CheckState target = active ? Qt::Checked : Qt::Unchecked;
                 if (item->checkState(i + 3) != target)
                     item->setCheckState(i + 3, target);
             }
         }
    }

	m_tree->blockSignals(false);
}

void AudioMatrix::OnItemChanged(QTreeWidgetItem *item, int col)
{
	if (m_updating)
		return;
	if (col < 3)
		return; // Name/ID/Type columns

	QString name = item->data(0, Qt::UserRole).toString();

	obs_source_t *source = obs_get_source_by_name(name.toUtf8().constData());
	if (source) {
		uint32_t mixers = obs_source_get_audio_mixers(source);
		int trackIdx = col - 3; // Shifted by 3

		if (item->checkState(col) == Qt::Checked) {
			mixers |= (1 << trackIdx);
		} else {
			mixers &= ~(1 << trackIdx);
		}

		obs_source_set_audio_mixers(source, mixers);
		obs_source_release(source);
	}
}
