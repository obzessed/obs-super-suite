#pragma once

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

// Channel configuration constants
constexpr int ASIO_START_CHANNEL = 41;
constexpr int ASIO_MAX_SOURCES = 10;
constexpr int ASIO_END_CHANNEL = ASIO_START_CHANNEL + ASIO_MAX_SOURCES - 1; // 50

// Configuration for a single ASIO source
struct AsioSourceConfig {
	QString name;	       // Display name
	int outputChannel;     // Output channel index (1-63)
	bool enabled;	       // Whether source is active
	QJsonObject sourceSettings; // obs_data settings for asio_input_capture
	QJsonArray sourceFilters;   // Filter data array (from obs_source_backup_filters)

	AsioSourceConfig()
		: name("ASIO Audio"),
		  outputChannel(ASIO_START_CHANNEL),
		  enabled(true),
		  sourceSettings(QJsonObject()),
		  sourceFilters(QJsonArray())
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
