#include <obs-frontend-api.h>
#include <obs-module.h>

#include <plugin-support.h>

#include "asio_config.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

static AsioConfig *instance = nullptr;

AsioConfig *AsioConfig::get()
{
	if (!instance) {
		instance = new AsioConfig();
	}
	return instance;
}

void AsioConfig::cleanup()
{
	if (instance) {
		delete instance;
		instance = nullptr;
	}
}

AsioConfig::AsioConfig()
{
	load();
}

QString AsioConfig::getConfigPath() const
{
	char *configPath = obs_module_config_path("asio_channels.json");
	QString path = QString::fromUtf8(configPath);
	bfree(configPath);
	return path;
}

void AsioConfig::load()
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

		// Add one default source
		AsioSourceConfig defaultConfig;
		defaultConfig.name = "Audio 1 (ASIO)";
		defaultConfig.outputChannel = ASIO_START_CHANNEL;
		defaultConfig.enabled = true;
		sources.append(defaultConfig);

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

		// Create default
		AsioSourceConfig defaultConfig;
		sources.append(defaultConfig);
		save();
		return;
	}

	QJsonObject root = doc.object();
	QJsonArray sourcesArray = root["sources"].toArray();

	for (const QJsonValue &val : sourcesArray) {
		QJsonObject obj = val.toObject();

		AsioSourceConfig src;
		src.name = obj["name"].toString("ASIO Audio");
		src.outputChannel = obj["outputChannel"].toInt(ASIO_START_CHANNEL);
		src.enabled = obj["enabled"].toBool(true);
		src.sourceSettings = obj["sourceSettings"].toObject();
		src.sourceFilters = obj["sourceFilters"].toArray();
		
		// Audio control settings
		src.muted = obj["muted"].toBool(false);
		src.monitoringType = obj["monitoringType"].toInt(0);
		src.volume = (float)obj["volume"].toDouble(1.0);
		src.balance = (float)obj["balance"].toDouble(0.5);
		src.forceMono = obj["forceMono"].toBool(false);

		if (src.name.isEmpty()) {
			src.name = QString("Audio %1 (ASIO)").arg(sources.size() + 1);
		}
		if (src.outputChannel < ASIO_START_CHANNEL || src.outputChannel > ASIO_END_CHANNEL) {
			src.outputChannel = ASIO_START_CHANNEL + sources.size();
		}

		sources.append(src);
	}

	obs_log(LOG_INFO, "ASIO config loaded: %d sources", sources.size());
}

void AsioConfig::save()
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

void AsioConfig::addSource(const AsioSourceConfig &cfg)
{
	sources.append(cfg);
	save();
}

void AsioConfig::removeSource(int index)
{
	if (index >= 0 && index < sources.size()) {
		sources.removeAt(index);
		save();
	}
}

void AsioConfig::updateSource(int index, const AsioSourceConfig &cfg)
{
	if (index >= 0 && index < sources.size()) {
		sources[index] = cfg;
		save();
	}
}
