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

class CanvasManager : public QDialog {
	Q_OBJECT

public:
	explicit CanvasManager(QWidget *parent = nullptr);
	~CanvasManager() override;

protected:
	void showEvent(QShowEvent *event) override;

private slots:
	void refresh();
	void addCanvas();
	void removeCanvas();
	void editCanvas();

private:
	void setupUi();
	void updateButtonStates();

	QTableWidget *m_table;
	QPushButton *m_addBtn;
	QPushButton *m_editBtn;
	QPushButton *m_removeBtn;
	QPushButton *m_refreshBtn;
	QPushButton *m_closeBtn;
};
