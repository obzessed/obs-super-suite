#pragma once

#include <obs.h>
#include <obs-module.h>

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>

class OutputsViewer : public QDialog {
	Q_OBJECT

public:
	explicit OutputsViewer(QWidget *parent = nullptr);
	~OutputsViewer();

protected:
	void showEvent(QShowEvent *event) override;

private slots:
	void refresh();

private:
	void setupUi();
	void addOutputRow(obs_output_t *output);

	QTableWidget *m_table;
	QPushButton *m_refreshBtn;
	QPushButton *m_closeBtn;
};
