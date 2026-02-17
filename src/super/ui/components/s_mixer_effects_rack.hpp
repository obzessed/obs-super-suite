#pragma once

// ============================================================================
// SMixerEffectsRack — Filter/Effects chain display
//
// Lists the OBS source filters applied to a channel. Features:
//   - Shows filter name and enabled/bypass state
//   - Toggle enable/disable per filter
//   - "+" button to open OBS filter dialog
//   - Drag-and-drop reordering of filters
// ============================================================================

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <obs.hpp>

namespace super {

class SMixerEffectsRack : public QWidget {
	Q_OBJECT

public:
	explicit SMixerEffectsRack(QWidget *parent = nullptr);

	// --- Source Binding ---
	void setSource(obs_source_t *source);
	void refresh();

signals:
	void addFilterRequested();
	void filterClicked(obs_source_t *filter);

private slots:
	void onReorder();

	// --- Collapse ---
	void setExpanded(bool expanded);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;

private:
	void setupUi();
	void clearItems();

	QLabel *m_header_label = nullptr;
	QPushButton *m_add_btn = nullptr;
	QPushButton *m_collapse_btn = nullptr;
	QListWidget *m_list = nullptr;

	obs_source_t *m_source = nullptr;
	bool m_updating_internal = false;
	bool m_is_expanded = true;

};

} // namespace super
