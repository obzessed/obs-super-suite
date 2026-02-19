#pragma once

#include <QWidget>
#include <QMainWindow>
#include <QTreeWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QPointer>

class QtInspector : public QMainWindow {
	Q_OBJECT

public:
	explicit QtInspector(QWidget *parent = nullptr);
	~QtInspector() override;

private slots:
	void refreshTree();
	void onItemSelected(QTreeWidgetItem *item, int column);
	void filterTree(const QString &text);
	void onPropertyChanged(QTableWidgetItem *item);

private:
	void scanObject(QObject *obj, QTreeWidgetItem *parentItem);
	void updateProperties(QObject *obj);

	QTreeWidget *m_tree;
	QTableWidget *m_props;
	QLineEdit *m_filter;
	QPointer<QObject> m_current_widget;
	bool m_updating = false;
};
