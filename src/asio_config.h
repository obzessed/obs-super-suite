#pragma once

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

// Configuration for a single audio channel source
struct AsioSourceConfig {
	QString name;          // Display name
	QString sourceType;    // OBS source type ID (wasapi_output_capture, wasapi_input_capture, asio_input_capture)
	QString canvas;        // Canvas UUID (empty = main canvas)
	int outputChannel;     // Output channel index (1-MAX_CHANNELS)
	bool enabled;	       // Whether source is active
	QJsonObject sourceSettings; // obs_data settings for the source
	QJsonArray sourceFilters;   // Filter data array (from obs_source_backup_filters)
	
	// Audio control settings
	bool muted;            // Mute state
	int monitoringType;    // 0=off, 1=monitor only, 2=monitor and output
	float volume;          // Volume multiplier (0.0 to 1.0+, 1.0 = 0dB)
	float balance;         // Balance/pan (0.0=left, 0.5=center, 1.0=right)
	bool forceMono;        // Force mono downmix
	uint32_t audioMixers;  // Audio mixer tracks bitmask (1-6), default 0x3F = all tracks
	bool audioActive;      // Show in audio mixer (obs_source_audio_active)
	
	// Source identity (persisted for matching)
	QString sourceUuid;    // OBS source UUID for stable matching across restarts

	AsioSourceConfig()
		: name("Audio"),
		  sourceType("wasapi_input_capture"), // Default to Audio Input Capture
		  canvas(""),              // Empty = main canvas
		  outputChannel(-1),  // -1 = none (not bound to channel)
		  enabled(true),
		  sourceSettings(QJsonObject()),
		  sourceFilters(QJsonArray()),
		  muted(false),
		  monitoringType(0),
		  volume(1.0f),
		  balance(0.5f),
		  forceMono(false),
		  audioMixers(0x3F),   // All 6 tracks enabled by default
		  audioActive(true),   // Show in mixer by default
		  sourceUuid("")
	{
	}
};

// Singleton configuration manager for ASIO sources
class AsioConfig {
public:
	static AsioConfig *get();
	static void cleanup();

	void load();
	void save();

	QVector<AsioSourceConfig> &getSources() { return sources; }
	const QVector<AsioSourceConfig> &getSources() const { return sources; }

	void addSource(const AsioSourceConfig &config);
	void removeSource(int index);
	void updateSource(int index, const AsioSourceConfig &config);

private:
	AsioConfig();
	~AsioConfig() = default;

	QVector<AsioSourceConfig> sources;

	QString getConfigPath() const;
};
