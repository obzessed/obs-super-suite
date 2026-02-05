#pragma once

#include "obs-module.h"

#include <QDialog>
#include <QTreeWidget>
#include <QPushButton>

class ChannelsView : public QDialog {
	Q_OBJECT

public:
	explicit ChannelsView(QWidget *parent = nullptr);
	~ChannelsView();

	void refresh();

	// Public for callback access
	void addCanvasGroup(obs_canvas_t *canvas);

protected:
	void showEvent(QShowEvent *event) override;

private:
	void setupUi();
	void addChannelItem(QTreeWidgetItem *parent, int channel, obs_source_t *source);

	QTreeWidget *m_tree;
	QPushButton *m_refreshBtn;
	QPushButton *m_closeBtn;
};
