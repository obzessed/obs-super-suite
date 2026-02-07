#include "./wrapper_test_dock.h"
#include "../lib/obs.hxx"

#include <QDateTime>
#include <QScrollBar>

WrapperTestDock::WrapperTestDock(QWidget *parent)
	: QWidget(parent)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(8, 8, 8, 8);
	layout->setSpacing(4);

	// Title
	auto *title = new QLabel("<b>OBS Wrapper Test Dock</b>");
	layout->addWidget(title);

	// Button row 1
	auto *btnRow1 = new QHBoxLayout();
	
	auto *btnSources = new QPushButton("Test Sources");
	connect(btnSources, &QPushButton::clicked, this, &WrapperTestDock::onTestSources);
	btnRow1->addWidget(btnSources);
	
	auto *btnScenes = new QPushButton("Test Scenes");
	connect(btnScenes, &QPushButton::clicked, this, &WrapperTestDock::onTestScenes);
	btnRow1->addWidget(btnScenes);
	
	auto *btnCanvases = new QPushButton("Test Canvases");
	connect(btnCanvases, &QPushButton::clicked, this, &WrapperTestDock::onTestCanvases);
	btnRow1->addWidget(btnCanvases);
	
	layout->addLayout(btnRow1);

	// Button row 2
	auto *btnRow2 = new QHBoxLayout();
	
	auto *btnWeak = new QPushButton("Test Weak Refs");
	connect(btnWeak, &QPushButton::clicked, this, &WrapperTestDock::onTestWeakRefs);
	btnRow2->addWidget(btnWeak);
	
	auto *btnRefCount = new QPushButton("Test Ref Counting");
	connect(btnRefCount, &QPushButton::clicked, this, &WrapperTestDock::onTestRefCounting);
	btnRow2->addWidget(btnRefCount);
	
	auto *btnLiveness = new QPushButton("Test Liveness");
	connect(btnLiveness, &QPushButton::clicked, this, &WrapperTestDock::onTestLiveness);
	btnRow2->addWidget(btnLiveness);
	
	auto *btnClear = new QPushButton("Clear");
	connect(btnClear, &QPushButton::clicked, this, &WrapperTestDock::onClearLog);
	btnRow2->addWidget(btnClear);
	
	layout->addLayout(btnRow2);

	// Log area
	m_logArea = new QTextEdit();
	m_logArea->setReadOnly(true);
	m_logArea->setFont(QFont("Consolas", 9));
	m_logArea->setStyleSheet("QTextEdit { background-color: #1e1e1e; color: #d4d4d4; }");
	layout->addWidget(m_logArea, 1);

	log("Wrapper Test Dock initialized.");
	log("Click buttons above to test OBS C++ wrappers.");
}

WrapperTestDock::~WrapperTestDock()
{
	log("WrapperTestDock destroyed.");
}

void WrapperTestDock::log(const QString &message)
{
	QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
	m_logArea->append(QString("[%1] %2").arg(timestamp, message));
	
	// Auto-scroll to bottom
	QScrollBar *sb = m_logArea->verticalScrollBar();
	sb->setValue(sb->maximum());
}

void WrapperTestDock::logSection(const QString &title) const
{
	m_logArea->append("");
	m_logArea->append(QString("=== %1 ===").arg(title));
}

void WrapperTestDock::onClearLog()
{
	m_logArea->clear();
	log("Log cleared.");
}

void WrapperTestDock::onTestSources()
{
	logSection("Testing Sources (obs::Source)");
	
	size_t count = 0;
	obs::Source::forEach([this, &count](const obs::Local<obs::Source>& source, const size_t idx) {
		const auto src = obs::Source(source.raw()); // released by obs::Local<obs::Source>
		
		log(QString("  [%1] %2 (type: %3, uuid: %4)")
			.arg(idx)
			.arg(
				QString::fromStdString(src.name()),
				QString::fromStdString(src.id()),
				QString::fromStdString(src.uuid())
			)
		);
		
		count++;
		return true; // Continue enumeration
	});
	
	log(QString("Total sources enumerated: %1").arg(count));
	log("Sources test complete - all Local<Source> handles should be released now.");
}

void WrapperTestDock::onTestScenes()
{
	logSection("Testing Scenes (obs::Scene)");
	
	size_t count = 0;
	obs::Scene::forEach([this, &count](obs::Local<obs::Source>& sceneSource, size_t idx) {
		auto name = obs::Source::getName(sceneSource.raw());
		auto uuid = obs::Source::getUuid(sceneSource.raw());
		
		log(QString("  [%1] Scene: %2 (uuid: %3)")
			.arg(idx)
			.arg(QString::fromStdString(name))
			.arg(QString::fromStdString(uuid)));
		
		count++;
		return true;
	});
	
	log(QString("Total scenes enumerated: %1").arg(count));
	
	// Test findByName
	log("Testing Scene::findByName...");
	auto scene = obs::Scene::findByName("Scene");
	if (scene) {
		obs_source_t* src = obs::Scene::getSource(scene->raw());
		log(QString("  Found scene 'Scene': %1").arg(QString::fromStdString(obs::Source::getName(src))));
	} else {
		log("  Scene 'Scene' not found (this is OK if you don't have a scene with that name)");
	}
	
	log("Scenes test complete.");
}

void WrapperTestDock::onTestCanvases()
{
	logSection("Testing Canvases (obs::Canvas)");
	
	// Get main canvas
	log("Getting main canvas...");
	auto mainCanvas = obs::Canvas::getMain();
	if (mainCanvas) {
		log("  Main canvas obtained successfully");
	} else {
		log("  WARNING: Main canvas is null!");
	}
	
	// Enumerate all canvases
	size_t count = 0;
	obs::Canvas::forEach([this, &count](obs::Local<obs::Canvas>& canvas, size_t idx) {
		auto name = obs::Canvas::getName(canvas.raw());
		auto uuid = obs::Canvas::getUuid(canvas.raw());
		log(QString("  [%1] Canvas: %2 (uuid: %3)").arg(idx).arg(QString::fromStdString(name)).arg(QString::fromStdString(uuid)));
		count++;
		return true;
	});
	
	log(QString("Total canvases enumerated: %1").arg(count));
	log("Canvases test complete.");
}

void WrapperTestDock::onTestWeakRefs()
{
	logSection("Testing Weak References");
	
	// Find a source and create weak ref
	obs::WeakRef<obs::Source> weakRef;
	
	{
		log("Creating Local<Source> and getting weak reference...");
		
		// Try to get the first source
		bool foundOne = false;
		obs::Source::forEach([this, &weakRef, &foundOne](obs::Local<obs::Source>& source, size_t idx) {
			if (!foundOne) {
				auto name = obs::Source::getName(source.raw());
				log(QString("  Got source: %1").arg(QString::fromStdString(name)));
				
				// Get weak reference
				weakRef = source.weak();
				log("  Created weak reference");
				foundOne = true;
			}
			return !foundOne; // Stop after first
		});
		
		if (!foundOne) {
			log("  No sources found to test weak refs");
			return;
		}
		
		// Test lock while source is still in scope (via enumeration callback)
		log("  Testing weak.lock() while strong ref exists...");
		if (auto locked = weakRef.lock()) {
			log(QString("    Locked successfully: %1")
				.arg(QString::fromStdString(obs::Source::getName(locked->raw()))));
		} else {
			log("    WARNING: lock() returned nullopt while source should be alive!");
		}
	}
	
	// Source should still be alive (OBS owns it)
	log("After scope exit, testing weak.lock()...");
	if (auto locked = weakRef.lock()) {
		log(QString("  Source still alive: %1")
			.arg(QString::fromStdString(obs::Source::getName(locked->raw()))));
	} else {
		log("  Source no longer available (this is expected for temporary sources)");
	}
	
	log("Weak reference test complete.");
}

void WrapperTestDock::onTestRefCounting()
{
	logSection("Testing Reference Counting");
	
	// Test clone() method
	log("Testing Local<T>::clone() method...");
	
	obs::Source::forEach([this](obs::Local<obs::Source>& source, size_t idx) {
		if (idx > 0) return false; // Only test first source
		
		auto name = obs::Source::getName(source.raw());
		log(QString("  Original source: %1").arg(QString::fromStdString(name)));
		
		{
			log("  Cloning source (should increment ref count)...");
			auto cloned = source.clone();
			
			if (cloned) {
				log(QString("    Clone created: %1").arg(QString::fromStdString(obs::Source::getName(cloned.raw()))));
				log("    Clone will be released when going out of scope...");
			} else {
				log("    WARNING: clone() returned empty handle!");
			}
		}
		
		log("  Clone scope exited - ref should be decremented");
		log("  Original should still be valid...");
		log(QString("    Original: %1").arg(QString::fromStdString(obs::Source::getName(source.raw()))));
		
		return false;
	});
	
	// Test Ref<T> (copyable shared ownership)
	log("");
	log("Testing Ref<T> (shared ownership)...");
	
	obs::Ref<obs::Source> sharedRef;
	
	obs::Source::forEach([this, &sharedRef](obs::Local<obs::Source>& source, size_t idx) {
		if (idx > 0) return false;
		
		log(QString("  Converting Local to Ref via share()..."));
		sharedRef = source.share();
		
		if (sharedRef) {
			log(QString("    Ref created: %1").arg(QString::fromStdString(obs::Source::getName(sharedRef.raw()))));
		}
		
		return false;
	});
	
	if (sharedRef) {
		log("  Creating copy of Ref (should increment ref count)...");
		{
			obs::Ref<obs::Source> copy = sharedRef;
			log(QString("    Copy: %1").arg(QString::fromStdString(obs::Source::getName(copy.raw()))));
		}
		log("  Copy destroyed");
		log(QString("  Original Ref still valid: %1").arg(QString::fromStdString(obs::Source::getName(sharedRef.raw()))));
	}
	
	log("Ref counting test complete.");
}

void WrapperTestDock::onTestLiveness()
{
	logSection("Testing Liveness (obs::debug)");
	
	log("Using obs::debug::isAlive() and RefCountProbe...");
	log("");
	
	// Test sources
	log("Checking source liveness:");
	obs::Source::forEach([this](obs::Local<obs::Source>& source, size_t idx) {
		if (idx >= 3) return false; // Only first 3
		
		auto name = obs::Source::getName(source.raw());
		bool alive = obs::debug::isAlive(source.raw());
		auto probe = obs::debug::RefCountProbe::probe(source.raw());
		
		log(QString("  [%1] %2: alive=%3, hasWeakRefs=%4")
			.arg(idx)
			.arg(QString::fromStdString(name))
			.arg(alive ? "true" : "false")
			.arg(probe.hasWeakRefs ? "true" : "false"));
		
		// Also log to OBS console
		obs::debug::logSourceState("WrapperTest", source.raw());
		
		return true;
	});
	
	log("");
	log("Checking canvas liveness:");
	obs::Canvas::forEach([this](obs::Local<obs::Canvas>& canvas, size_t idx) {
		auto name = obs::Canvas::getName(canvas.raw());
		bool alive = obs::debug::isAlive(canvas.raw());
		auto probe = obs::debug::RefCountProbe::probe(canvas.raw());
		
		log(QString("  [%1] %2: alive=%3, hasWeakRefs=%4")
			.arg(idx)
			.arg(QString::fromStdString(name))
			.arg(alive ? "true" : "false")
			.arg(probe.hasWeakRefs ? "true" : "false"));
		
		obs::debug::logCanvasState("WrapperTest", canvas.raw());
		
		return true;
	});
	
	log("");
	log("Testing pointer after scope exit...");
	
	// Demonstrate checking a dangling pointer scenario
	obs_source_t* rawPtr = nullptr;
	{
		obs::Source::forEach([&rawPtr](obs::Local<obs::Source>& source, size_t idx) {
			if (idx == 0) {
				rawPtr = source.raw();
			}
			return false;
		});
	}
	
	if (rawPtr) {
		bool stillAlive = obs::debug::isAlive(rawPtr);
		log(QString("  Raw pointer after Local destroyed: alive=%1")
			.arg(stillAlive ? "true (OBS still owns it)" : "false (freed)"));
	}
	
	log("");
	log("Liveness test complete.");
	log("Check OBS log for [obs::debug] entries.");
}
