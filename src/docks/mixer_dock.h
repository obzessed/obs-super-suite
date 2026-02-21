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
	~MixerDock() override;

	void refresh();
	void clearChannels();

	// Update methods for signal handlers
	void updateSourceVolume(const QString &sourceUuid, float volume);
	void updateSourceMute(const QString &sourceUuid, bool muted);
	void updateSourceBalance(const QString &sourceUuid, float balance);

protected:
	void showEvent(QShowEvent *event) override;

private:
	void setupUi();
	MixerChannel *findChannelByUuid(const QString &uuid);

	QHBoxLayout *m_channelsLayout;
	QScrollArea *m_scrollArea;
	QVector<MixerChannel*> m_channels;

	bool m_effectsVisible = true;
	bool m_fadersVisible = true;

protected:
	void contextMenuEvent(QContextMenuEvent *event) override;
};
