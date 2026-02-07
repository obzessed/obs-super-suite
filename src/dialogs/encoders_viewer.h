#pragma once

#include <obs.h>
#include <obs-module.h>

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

class EncodersViewer : public QDialog {
	Q_OBJECT

public:
	explicit EncodersViewer(QWidget *parent = nullptr);
	~EncodersViewer();

protected:
	void showEvent(QShowEvent *event) override;

private slots:
	void refresh();

private:
	void setupUi();
	void addEncoderRow(obs_encoder_t *encoder);

	QTableWidget *m_table;
	QPushButton *m_refreshBtn;
	QPushButton *m_closeBtn;
};
