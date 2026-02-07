#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>

/**
 * @brief Test dock for verifying OBS C++ wrapper functionality
 * 
 * Provides buttons to exercise various wrapper operations
 * and displays the results in a log area.
 */
class WrapperTestDock : public QWidget {
	Q_OBJECT

public:
	explicit WrapperTestDock(QWidget *parent = nullptr);
	~WrapperTestDock() override;

private slots:
	void onTestSources();
	void onTestScenes();
	void onTestCanvases();
	void onTestWeakRefs();
	void onTestRefCounting();
	void onTestLiveness();
	void onClearLog();

private:
	void log(const QString &message);
	void logSection(const QString &title) const;
	
	QTextEdit *m_logArea;
};
