#pragma once

// ============================================================================
// Package Manager — .obs-pack Bundle Format
//
// Defines the structure for shareable content packages:
//   • Hardware Profiles
//   • Graph Workflows
//   • Virtual Surface Schemas
//   • Lua Scripts
//   • Presets (Snapshots)
//
// Format: ZIP archive with manifest.json at root.
// ============================================================================

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QDateTime>

namespace super {

// ---------------------------------------------------------------------------
// PackageManifest — Describes the contents of an .obs-pack file.
// ---------------------------------------------------------------------------
struct PackageManifest {
	QString name;
	QString version;
	QString author;
	QString description;
	QDateTime created;

	QStringList hardware_profiles;	// "profiles/*.json"
	QStringList graph_workflows;	// "graphs/*.json"
	QStringList surface_schemas;	// "surfaces/*.json"
	QStringList lua_scripts;		// "scripts/*.lua"
	QStringList presets;			// "presets/*.json"

	QJsonObject to_json() const {
		QJsonObject obj;
		obj["name"] = name;
		obj["version"] = version;
		obj["author"] = author;
		obj["description"] = description;
		obj["created"] = created.toString(Qt::ISODate);

		auto to_arr = [](const QStringList &list) {
			QJsonArray arr;
			for (const auto &s : list) arr.append(s);
			return arr;
		};

		obj["hardware_profiles"] = to_arr(hardware_profiles);
		obj["graph_workflows"] = to_arr(graph_workflows);
		obj["surface_schemas"] = to_arr(surface_schemas);
		obj["lua_scripts"] = to_arr(lua_scripts);
		obj["presets"] = to_arr(presets);

		return obj;
	}

	static PackageManifest from_json(const QJsonObject &obj) {
		PackageManifest m;
		m.name = obj["name"].toString();
		m.version = obj["version"].toString("1.0.0");
		m.author = obj["author"].toString();
		m.description = obj["description"].toString();
		m.created = QDateTime::fromString(
			obj["created"].toString(), Qt::ISODate);

		auto to_list = [](const QJsonArray &arr) {
			QStringList list;
			for (const auto &v : arr) list.append(v.toString());
			return list;
		};

		m.hardware_profiles = to_list(obj["hardware_profiles"].toArray());
		m.graph_workflows = to_list(obj["graph_workflows"].toArray());
		m.surface_schemas = to_list(obj["surface_schemas"].toArray());
		m.lua_scripts = to_list(obj["lua_scripts"].toArray());
		m.presets = to_list(obj["presets"].toArray());

		return m;
	}

	static PackageManifest load(const QString &manifest_path) {
		QFile f(manifest_path);
		if (!f.open(QIODevice::ReadOnly))
			return {};
		return from_json(QJsonDocument::fromJson(f.readAll()).object());
	}

	bool save(const QString &path) const {
		QFile f(path);
		if (!f.open(QIODevice::WriteOnly))
			return false;
		f.write(QJsonDocument(to_json()).toJson(QJsonDocument::Indented));
		return true;
	}
};

// ---------------------------------------------------------------------------
// PackageManager — Discovers and loads installed packages.
// ---------------------------------------------------------------------------
class PackageManager : public QObject {
	Q_OBJECT

public:
	static PackageManager &instance() {
		static PackageManager s;
		return s;
	}

	// Set the root directory where packages are stored.
	void set_packages_dir(const QString &dir) {
		m_packages_dir = dir;
	}

	// Scan for installed packages (directories with manifest.json).
	QList<PackageManifest> scan() const {
		QList<PackageManifest> result;
		QDir dir(m_packages_dir);
		if (!dir.exists())
			return result;

		for (const auto &entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
			QString manifest_path = dir.filePath(entry + "/manifest.json");
			if (QFile::exists(manifest_path))
				result.append(PackageManifest::load(manifest_path));
		}
		return result;
	}

	// Get the packages directory.
	QString packages_dir() const { return m_packages_dir; }

signals:
	void package_installed(const QString &name);
	void package_removed(const QString &name);

private:
	PackageManager() : QObject(nullptr) {}
	QString m_packages_dir;
};

} // namespace super
