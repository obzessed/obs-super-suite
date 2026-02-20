#pragma once

// ============================================================================
// SMixerEffectsRack — Filter/Effects chain display
//
// Lists the OBS source filters applied to a channel. Features:
//   - Shows filter name and enabled/bypass state
//   - Toggle enable/disable per filter
//   - "+" button to open OBS filter dialog
//   - Drag-and-drop reordering of filters
//   - Selectable items with context menus
//   - Keyboard shortcuts (F2 rename, Del delete, Shift+Scroll move)
//   - Alt+Click to toggle enable/disable
//   - Copy/Paste filter clipboard
// ============================================================================

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QMimeData>
#include <QMap>
#include <obs.hpp>

namespace super {

class SMixerFilterControls;
class SMixerSidebarToggle;

class SMixerEffectsRack : public QWidget {
	Q_OBJECT

public:
	explicit SMixerEffectsRack(QWidget *parent = nullptr);

	// --- Source Binding ---
	void setSource(obs_source_t *source);
	void refresh();

	// --- Filter Clipboard (static, shared across channels) ---
	static void copyFilter(obs_source_t *source, obs_source_t *filter);
	static void copyAllFilters(obs_source_t *source);
	static bool hasClipboardFilters();
	static void pasteFilters(obs_source_t *source, int insertIndex = -1);

signals:
	void addFilterRequested();
	void filterClicked(obs_source_t *filter);

private slots:
	void onReorder();

	// --- Collapse ---
	void setExpanded(bool expanded);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;

private:
	void setupUi();
	void clearItems();
	void showAddFilterMenu();
	void addFilter(const QString &typeId);

	// --- Context Menus ---
	void showItemContextMenu(QListWidgetItem *item, const QPoint &globalPos);
	void showRackContextMenu(const QPoint &globalPos);

	// --- Accordion Controls ---
	void toggleFilterControls(QListWidgetItem *item);
	void collapseAllControls();

	// --- Filter Operations ---
	obs_source_t *filterFromItem(QListWidgetItem *item);
	void moveFilterUp(QListWidgetItem *item);
	void moveFilterDown(QListWidgetItem *item);
	void deleteFilter(QListWidgetItem *item, bool confirm = true);
	void renameFilter(QListWidgetItem *item);
	void toggleFilterEnabled(QListWidgetItem *item);
	void clearAllFilters();

	QLabel *m_header_label = nullptr;
	QPushButton *m_add_btn = nullptr;
	SMixerSidebarToggle *m_collapse_btn = nullptr;
	QListWidget *m_list = nullptr;

	obs_source_t *m_source = nullptr;
	bool m_updating_internal = false;
	bool m_is_expanded = true;

	// --- Accordion State ---
	QMap<QListWidgetItem*, QListWidgetItem*> m_controls_items; // filter item → controls item
	QMap<QListWidgetItem*, SMixerFilterControls*> m_controls_map; // filter item → controls widget

	// --- Static Clipboard ---
	struct ClipboardFilter {
		QString typeId;
		QString name;
		OBSData settings;
	};
	static QList<ClipboardFilter> s_clipboard;
};

} // namespace super
