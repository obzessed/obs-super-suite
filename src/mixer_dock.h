#pragma once

#include <QScrollArea>
#include <QWidget>
#include <QVBoxLayout>
#include <QVector>

class MixerChannel;

class MixerDock : public QWidget {
	Q_OBJECT

public:
	explicit MixerDock(QWidget *parent = nullptr);
	~MixerDock();

	void refresh();

	// Update methods for signal handlers
	void updateSourceVolume(const QString &sourceUuid, float volume);
	void updateSourceMute(const QString &sourceUuid, bool muted);
	void updateSourceBalance(const QString &sourceUuid, float balance);

protected:
	void showEvent(QShowEvent *event) override;

private:
	void setupUi();
	void clearChannels();
	MixerChannel *findChannelByUuid(const QString &uuid);

	QHBoxLayout *m_channelsLayout;
	QScrollArea *m_scrollArea;
	QVector<MixerChannel*> m_channels;
};
