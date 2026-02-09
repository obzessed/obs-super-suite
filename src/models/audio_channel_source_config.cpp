#include <obs-frontend-api.h>
#include <obs-module.h>

#include <plugin-support.h>

#include "./audio_channel_source_config.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

static AudioChSrcConfig *instance = nullptr;

AudioChSrcConfig *AudioChSrcConfig::get()
{
	if (!instance) {
		instance = new AudioChSrcConfig();
	}
	return instance;
}

void AudioChSrcConfig::cleanup()
{
	if (instance) {
		delete instance;
		instance = nullptr;
	}
}

AudioChSrcConfig::AudioChSrcConfig()
{
	load();
}

QString AudioChSrcConfig::getConfigPath() const
{
	char *configPath = obs_module_config_path("audio-channels.json");
	QString path = QString::fromUtf8(configPath);
	bfree(configPath);
	return path;
}

void AudioChSrcConfig::load()
{
	sources.clear();

	QString configPath = getConfigPath();

	// Ensure directory exists
	if (QDir dir = QFileInfo(configPath).dir(); !dir.exists()) {
		dir.mkpath(".");
	}

	QFile file(configPath);
	if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
		// Config doesn't exist yet, create default
		obs_log(LOG_INFO, "ASIO config not found, creating default");

		// No config file - start with empty sources
		save();
		return;
	}

	QByteArray data = file.readAll();
	file.close();

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

	if (parseError.error != QJsonParseError::NoError) {
		obs_log(LOG_WARNING, "Failed to parse ASIO config JSON: %s",
			parseError.errorString().toUtf8().constData());

		// Parse error - start with empty sources
		save();
		return;
	}

	QJsonObject root = doc.object();
	QJsonArray sourcesArray = root["sources"].toArray();

	for (const QJsonValue &val : sourcesArray) {
		QJsonObject obj = val.toObject();

		AsioSourceConfig src;
		src.name = obj["name"].toString("Audio");
		src.sourceType = obj["sourceType"].toString("wasapi_input_capture"); // Default to Audio Input Capture
		src.canvas = obj["canvas"].toString("");  // Empty = main canvas
		src.outputChannel = obj["outputChannel"].toInt(1);
		src.enabled = obj["enabled"].toBool(true);
		src.sourceSettings = obj["sourceSettings"].toObject();
		src.sourceFilters = obj["sourceFilters"].toArray();
		
		// Audio control settings
		src.muted = obj["muted"].toBool(false);
		src.monitoringType = obj["monitoringType"].toInt(0);
		src.volume = (float)obj["volume"].toDouble(1.0);
		src.balance = (float)obj["balance"].toDouble(0.5);
		src.forceMono = obj["forceMono"].toBool(false);
		src.audioMixers = (uint32_t)obj["audioMixers"].toInt(0x3F); // Default all 6 tracks
		src.audioActive = obj["audioActive"].toBool(true); // Default show in mixer
		src.sourceUuid = obj["sourceUuid"].toString(""); // Persisted UUID for source matching

		if (src.name.isEmpty()) {
			src.name = QString("Audio %1").arg(sources.size() + 1);
		}
		// -1 means no channel (none), otherwise must be 1-MAX_CHANNELS
		if (src.outputChannel != -1 && (src.outputChannel < 1 || src.outputChannel > MAX_CHANNELS)) {
			src.outputChannel = -1; // Default to none for invalid values
		}

		sources.append(src);
	}

	obs_log(LOG_INFO, "ASIO config loaded: %d sources", sources.size());
}

void AudioChSrcConfig::save()
{
	QString configPath = getConfigPath();

	// Ensure directory exists
	if (QDir dir = QFileInfo(configPath).dir(); !dir.exists()) {
		dir.mkpath(".");
	}

	QJsonArray sourcesArray;
	for (const AsioSourceConfig &src : sources) {
		QJsonObject obj;
		obj["name"] = src.name;
		obj["sourceType"] = src.sourceType;
		obj["canvas"] = src.canvas;
		obj["outputChannel"] = src.outputChannel;
		obj["enabled"] = src.enabled;
		obj["sourceSettings"] = src.sourceSettings;
		obj["sourceFilters"] = src.sourceFilters;
		
		// Audio control settings
		obj["muted"] = src.muted;
		obj["monitoringType"] = src.monitoringType;
		obj["volume"] = src.volume;
		obj["balance"] = src.balance;
		obj["forceMono"] = src.forceMono;
		obj["audioMixers"] = (int)src.audioMixers;
		obj["audioActive"] = src.audioActive;
		obj["sourceUuid"] = src.sourceUuid; // Persist UUID for source matching
		
		sourcesArray.append(obj);
	}

	QJsonObject root;
	root["sources"] = sourcesArray;

	QJsonDocument doc(root);

	QFile file(configPath);
	if (!file.open(QIODevice::WriteOnly)) {
		obs_log(LOG_ERROR, "Failed to open ASIO config for writing: %s",
			configPath.toUtf8().constData());
		return;
	}

	file.write(doc.toJson(QJsonDocument::Indented));
	file.close();

	obs_log(LOG_INFO, "ASIO config saved: %d sources", sources.size());
}

void AudioChSrcConfig::addSource(const AsioSourceConfig &cfg)
{
	sources.append(cfg);
	save();
}

void AudioChSrcConfig::removeSource(int index)
{
	if (index >= 0 && index < sources.size()) {
		sources.removeAt(index);
		save();
	}
}

void AudioChSrcConfig::updateSource(int index, const AsioSourceConfig &cfg)
{
	if (index >= 0 && index < sources.size()) {
		sources[index] = cfg;
		save();
	}
}

void AudioChSrcConfig::moveSource(int from, int to)
{
	if (from < 0 || from >= sources.size()) return;
	if (to < 0 || to >= sources.size()) return;
	if (from == to) return;
	
	AsioSourceConfig item = sources.takeAt(from);
	sources.insert(to, item);
	save();
}

void AudioChSrcConfig::swapSources(const QString &uuid1, const QString &uuid2)
{
	int idx1 = -1;
	int idx2 = -1;
	
	for (int i = 0; i < sources.size(); i++) {
		if (sources[i].sourceUuid == uuid1) idx1 = i;
		if (sources[i].sourceUuid == uuid2) idx2 = i;
	}
	
	if (idx1 != -1 && idx2 != -1 && idx1 != idx2) {
		sources.swapItemsAt(idx1, idx2);
		save();
	}
}

bool AudioChSrcConfig::canMoveSourceLeft(const QString &uuid) const
{
	int idx = -1;
	for (int i = 0; i < sources.size(); i++) {
		if (sources[i].sourceUuid == uuid) {
			idx = i;
			break;
		}
	}
	
	if (idx <= 0) return false;
	
	for (int i = idx - 1; i >= 0; i--) {
		if (sources[i].enabled && !sources[i].sourceUuid.isEmpty()) return true;
	}
	
	return false;
}

bool AudioChSrcConfig::canMoveSourceRight(const QString &uuid) const
{
	int idx = -1;
	for (int i = 0; i < sources.size(); i++) {
		if (sources[i].sourceUuid == uuid) {
			idx = i;
			break;
		}
	}
	
	if (idx == -1 || idx >= sources.size() - 1) return false;
	
	for (int i = idx + 1; i < sources.size(); i++) {
		if (sources[i].enabled && !sources[i].sourceUuid.isEmpty()) return true;
	}
	
	return false;
}
