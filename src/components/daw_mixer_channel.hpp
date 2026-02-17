#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QDial>
#include <QMutex>
#include <obs.hpp>

class DawMixerMeter;

class DawMixerChannel : public QWidget {
	Q_OBJECT

public:
	explicit DawMixerChannel(QWidget *parent = nullptr, obs_source_t *source = nullptr);
	~DawMixerChannel() override;

	void setSource(obs_source_t *source);
	QString sourceName() const;

signals:
	void volumeChanged(float volume);
	void muteChanged(bool muted);
	void settingsRequested();

protected:
	void paintEvent(QPaintEvent *event) override;

private slots:
	void on_fader_changed(int value);
	void on_mute_clicked();
	void on_pan_changed(int value);

private:
	void setup_ui();
	void connect_source();
	void disconnect_source();
	void update_db_label();

	static void obs_volmeter_cb(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
				    const float peak[MAX_AUDIO_CHANNELS], const float input_peak[MAX_AUDIO_CHANNELS]);
	static void obs_source_volume_cb(void *data, calldata_t *cd);
	static void obs_source_mute_cb(void *data, calldata_t *cd);
	static void obs_source_rename_cb(void *data, calldata_t *cd);

	obs_source_t *source = nullptr;
	obs_volmeter_t *volmeter = nullptr;

	// UI
	QLabel *name_label = nullptr;
	QPushButton *settings_btn = nullptr;
	QLabel *pan_label = nullptr;
	QDial *pan_dial = nullptr;
	QLabel *pan_value_label = nullptr;
	QPushButton *clip_led = nullptr;
	QLabel *ovr_label = nullptr;
	QSlider *fader = nullptr;
	DawMixerMeter *meter_l = nullptr;
	DawMixerMeter *meter_r = nullptr;

	// State
	bool updating_from_source = false;
	bool clipping = false;

	// Meter data (written from audio thread, read from paint thread)
	QMutex meter_mutex;
	float peak_l = -60.0f;
	float peak_r = -60.0f;
	float mag_l = -60.0f;
	float mag_r = -60.0f;

	// Display values (decayed)
	float disp_peak_l = -60.0f;
	float disp_peak_r = -60.0f;
	float disp_mag_l = -60.0f;
	float disp_mag_r = -60.0f;
};

// Simple vertical meter bar used inside DawMixerChannel
class DawMixerMeter : public QWidget {
	Q_OBJECT

public:
	explicit DawMixerMeter(QWidget *parent = nullptr);
	void set_level(float peak_db, float mag_db);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	float peak_db = -60.0f;
	float mag_db = -60.0f;

	float map_db(float db) const;
};
