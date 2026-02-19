#pragma once

#include <QDialog>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <obs.h>

class AudioMatrix : public QDialog {
	Q_OBJECT

public:
	explicit AudioMatrix(QWidget *parent = nullptr);
	~AudioMatrix() override;

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private slots:
	void FullRefresh();
	void OnItemChanged(QTreeWidgetItem *item, int column);
	
	// Signal handlers (called from static proxy)
	void SourceCreated(const QString &name);
	void SourceRemoved(const QString &name);
	void SourceRenamed(const QString &oldName, const QString &newName);
	void SourceMixersChanged(const QString &name, uint32_t mixers);

private:
	void SetupUi();
	void ConnectGlobalSignals(bool connect);
	
	// Per-source connection helper
	void ConnectSource(obs_source_t *source, bool connect);

	static void OBS_SourceCreated(void *data, calldata_t *cd);
	static void OBS_SourceRemoved(void *data, calldata_t *cd);
	static void OBS_SourceRenamed(void *data, calldata_t *cd);
	static void OBS_SourceAudioMixers(void *data, calldata_t *cd);

	QTreeWidget *m_tree;
	bool m_updating = false;
};
