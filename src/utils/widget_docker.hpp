#pragma once

#include <QWidget>
#include <QDockWidget>
#include <QPointer>
#include <QLayout>
#include <QMainWindow>
#include <obs-frontend-api.h>

class WidgetDocker : public QObject {
	Q_OBJECT

public:
	explicit WidgetDocker(const QString &widgetName, const QString &dockTitle, QObject *parent = nullptr)
		: QObject(parent), targetName(widgetName), dockTitle(dockTitle)
	{
	}

	enum State { Default, Hide, Dock };

	void SetState(State state)
	{
		if (currentState == state)
			return;

		// If currently Docked, we must Undock first to be safe (restore to original)
		// If currently Hidden, we must Show first (if were default).
		// actually, easiest is to Revert to Default first, then Apply new state.
		
		RevertToDefault();

		currentState = state;

		if (state == Default) {
			// Already done by RevertToDefault
		} else if (state == Hide) {
			QWidget *w = FindTarget();
			if (w) w->hide();
		} else if (state == Dock) {
			DockWidget();
		}
	}

	State GetState() const { return currentState; }

private:
	QString targetName;
	QString dockTitle;
	State currentState = Default;

	// State to restore
	QPointer<QWidget> originalParent;
	QPointer<QLayout> originalLayout;
	int originalIndex = -1;
	
	QPointer<QDockWidget> createdDock;

	QWidget *FindTarget()
	{
		QMainWindow *mainWin = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		if (!mainWin) return nullptr;
		return mainWin->findChild<QWidget *>(targetName);
	}

	void RevertToDefault()
	{
		QWidget *w = FindTarget();
		if (!w) return;

		if (createdDock) {
			// It was docked.
			// Reparent back.
			if (originalLayout && originalParent) {
				// Remove from dock
				createdDock->setWidget(nullptr); // Detach
				
				// Add back to original layout
				// Checking layout type
				if (auto *box = qobject_cast<QBoxLayout *>(originalLayout)) {
					box->insertWidget(originalIndex, w);
				} else if (auto *grid = qobject_cast<QGridLayout *>(originalLayout)) {
					// Grid restore is harder if we didn't save row/col/span.
					// For now assume box layout or just addWidget.
					// Most OBS layouts are HBox/VBox.
					box->insertWidget(originalIndex, w);
				} else {
					originalLayout->addWidget(w);
				}
				
				w->setParent(originalParent);
				w->show();
			}
			
			createdDock->close();
			delete createdDock;
			createdDock = nullptr;
		}
		
		w->show(); // ensure visible if it was hidden
	}

	void DockWidget()
	{
		QWidget *w = FindTarget();
		if (!w) return;

		// Save original state
		originalParent = w->parentWidget();
		if (originalParent) {
			originalLayout = originalParent->layout();
			if (originalLayout) {
				originalIndex = originalLayout->indexOf(w);
			}
		}

		// Create Dock
		QMainWindow *mainWin = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		createdDock = new QDockWidget(dockTitle, mainWin);
		createdDock->setObjectName(targetName + "_Dock");
		createdDock->setWidget(w);
		mainWin->addDockWidget(Qt::RightDockWidgetArea, createdDock); // Default area
		createdDock->setFloating(true); // Default floating? Or docked? User said "Dock".
		createdDock->show();
	}
};
