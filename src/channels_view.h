#pragma once

#include "obs-module.h"

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>

class ChannelsView : public QDialog {
	Q_OBJECT

public:
	explicit ChannelsView(QWidget *parent = nullptr);
	~ChannelsView();

	void refresh();

protected:
	void showEvent(QShowEvent *event) override;

private:
	void setupUi();
	void addRow(int channel, obs_source_t *source);

	QTableWidget *m_table;
	QPushButton *m_refreshBtn;
	QPushButton *m_closeBtn;
};
