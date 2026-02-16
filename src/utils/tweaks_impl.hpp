#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>
#include <QLayout>
#include <QDockWidget>

// TweaksImpl handles the logic of applying tweaks, independent of the UI panel.
// This allows tweaks to be applied automatically on startup if configured.
class TweaksImpl : public QObject {
	Q_OBJECT

public:
	TweaksImpl();
	~TweaksImpl() override;
	
	// Called when OBS frontend is fully loaded
	void FrontendReady();

	// Tweak Operations
	void ApplyTweaks();
	void SetProgramOptionsState(int state);
	void SetProgramLayoutState(int state);
	void SetPreviewLayoutState(int state);
	
	int GetProgramOptionsState() const { return programOptionsStateVal; }
	int GetProgramLayoutState() const { return programLayoutStateVal; }
	int GetPreviewLayoutState() const { return previewLayoutStateVal; }

private:
	struct WidgetState {
		QPointer<QWidget> widget;
		QPointer<QWidget> originalParent;
		QPointer<QLayout> originalLayout;
		int originalIndex = -1;
		QPointer<QDockWidget> dock;
	};

	WidgetState programOptionsState;
	WidgetState programLayoutState;
	WidgetState previewLayoutState;
	
	int programOptionsStateVal = 0;
	int programLayoutStateVal = 0;
	int previewLayoutStateVal = 0;

	QWidget *FindWidget(const QString &name);
	void SetWidgetState(WidgetState &ctx, const QString &name, int state, const QString &dockTitle);
};
