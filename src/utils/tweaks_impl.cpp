#include "tweaks_impl.hpp"

#include "extras/frontend_tweaks.hpp"

#include <plugin-support.h>

#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QLayout>
#include <QFrame>
#include <QSlider>
#include <QSplitter>
#include <QLabel>
#include <QDockWidget>
#include <obs-module.h>

static bool g_initialized = false;
static int g_instances = 0;
static bool g_tweaked_x = false;
static QDockWidget* programOptionsDock = nullptr;

TweaksImpl::TweaksImpl()
{
	g_instances++;

	if (g_initialized) {
		obs_log(LOG_WARNING, "TweaksImpl: Already initialized!");
		return;
	}

	obs_log(LOG_INFO, "TweaksImpl: Initializing, instance count: %d", g_instances);

	// Register callback for when frontend is ready
	obs_frontend_add_event_callback(
		[](obs_frontend_event event, void *private_data) {
			auto impl = static_cast<TweaksImpl *>(private_data);

			// if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
			// 	static_cast<TweaksImpl *>(private_data)->FrontendReady();
			// }
			if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
				impl->FrontendReady();
			}
			if (event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED || event == OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED) {
				// impl->ApplyTweaks(true);

				// enable only when experimentation
				return;

				// Main Window
				// OBSBasic <- OBSMainWindow <- QMainWindow
				// - centralWidget: centralwidget(QWidget)
				//   - layout: verticalLayout(QVBoxLayout)
				//     - canvasEditor(QWidget)
				//       - layout: previewLayout(QHBoxLayout)
				//         - previewDisabledWidget(QFrame)
				//	   - previewContainer(QWidget)
				//           - layout: previewTextLayout(QVBoxLayout)
				//             - previewLabel(QLabel)
				//             - gridLayout(QGridLayout)
				//         - programOptions(QWidget)
				//         - programWidget(QWidget)
				//           - layout: programLayout(QVBoxLayout)
				//             - programLabel(QLabel)
				//             - program(OBSQTDisplay)
				//     - contextContainer(QFrame)

				// EXPERIMENTATION: to modify the OBSBasic Preview and Program.
				{
					// find canvasEditor object in main window children
					auto *mainWin = static_cast<QMainWindow *>(obs_frontend_get_main_window());
					auto *centralWidget = mainWin->centralWidget();
					obs_log(LOG_ERROR, "Found centralWidget: %s", centralWidget ? "Yes" : "No");
					auto *canvasEditor = mainWin->findChild<QWidget *>(QStringLiteral("canvasEditor"));
					obs_log(LOG_ERROR, "Found canvasEditor: %s", canvasEditor ? "Yes" : "No");
					if (canvasEditor) {
						const auto *previewLayout = canvasEditor->findChild<QBoxLayout *>(QStringLiteral("previewLayout"));
						obs_log(LOG_ERROR, "Found previewLayout: %s", !!previewLayout ? "Yes" : "No");

						const auto *previewDisabledWidget = canvasEditor->findChild<QFrame *>(QStringLiteral("previewDisabledWidget"));
						obs_log(LOG_ERROR, "Found previewDisabledWidget: %s", !!previewDisabledWidget ? "Yes" : "No");

						// previewLayout has "programOptions" using addWidget
						if (previewLayout) {
							QWidget* programOptions = nullptr;
							obs_log(LOG_WARNING, "previewLayout has %d children", previewLayout->count());

							for (int i = 0; i < previewLayout->count(); i++) {
								const auto child = qobject_cast<QWidget *>(previewLayout->itemAt(i)->widget());

								obs_log(LOG_ERROR, "Found child: %s", child->objectName().toStdString().c_str());

								// find which one has tBar->setProperty("class", "slider-tbar"); where tBar is a QSlider
								for (const auto subChild : child->findChildren<QSlider *>()) {
									if (subChild->property("class").toString() == "slider-tbar") {
										obs_log(LOG_ERROR, "Found programOptions with T-Bar slider!");
										// child -> programOptions, subChild - > tBar
										programOptions = child;
									}
								}
							}

							if (event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED) {
								if (!g_tweaked_x) {
									{
										// Force all corners to be owned by the vertical side docks
										// This collapses the horizontal span of the Top/Bottom/Center areas
										// mainWin->setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
										// mainWin->setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
										// mainWin->setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
										// mainWin->setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

										// mainWin->setCentralWidget(nullptr);

										// Trigger a refresh of the internal QMainWindowLayoutSeparator
										foreach (auto w, mainWin->findChildren<QSplitter *>()) {
											obs_log(LOG_ERROR, "Found child: %s", w->metaObject()->className());
										 //    if (separator->metaObject()->className() == QStringLiteral("QMainWindowLayoutSeparator")) {
											// separator->hide(); // Forces the ghost bars to stay hidden
										 //    }
										}
									}

									// mainWin->setDockOptions(
									// 	QMainWindow::AnimatedDocks |
									// 	QMainWindow::AllowNestedDocks |
									// 	QMainWindow::AllowTabbedDocks |
									// 	QMainWindow::GroupedDragging
									// );

									// auto instructionLabel = new QLabel(
									// 	obs_module_text("SecondaryWindow.Instruction"),
									// 	mainWin
									// );
									// if (instructionLabel->text().isEmpty()) instructionLabel->setText("Right click to import a dock\nfrom another window.");
									// instructionLabel->setAlignment(Qt::AlignCenter);
									// instructionLabel->setStyleSheet(
									// 	"QLabel {"
									// 	"  color: #888;"
									// 	"  font-size: 16px;"
									// 	"  background-color: transparent;"
									// 	"}"
									// );
									//
									// auto currentCW = mainWin->takeCentralWidget();
									// mainWin->setCentralWidget(instructionLabel);
									// instructionLabel->setVisible(false);

									// centralWidget->hide();
									// centralWidget->setParent(nullptr);
									// mainWin->setDockNestingEnabled(true);

									// mainWin->setCentralWidget(new QWidget(mainWin));
									// mainWin->layout()->activate();  // Forces a full layout recalculation
									// mainWin->updateGeometry();      // Updates size hints and triggers a repaint
									// mainWin->repaint();             // Ensures visual updates

									// {
									// 	// 1. Remove the complex OBS central UI
									// 	QWidget* oldCenter = mainWin->takeCentralWidget();
									//
									// 	// 2. IMMEDIATELY insert a dummy widget
									// 	// This prevents the resizing "misbehavior"
									// 	auto* dummy = new QWidget();
									// 	dummy->setMaximumSize(0, 0); // Make it invisible but present
									// 	mainWin->setCentralWidget(dummy);
									//
									// 	// Now your docks will behave correctly because they have a 0,0 center to anchor to.
									// }

									// {
									// 	mainWin->centralWidget()->setParent(nullptr);
									//
									// 	// mainWin->takeCentralWidget();
									// 	// mainWin->layout()->invalidate();
									// 	// mainWin->layout()->activate();
									// 	// mainWin->layout()->update();
									// 	// obs_log(LOG_ERROR, "Owners: %p, %p, %p", mainWin, mainWin->layout(), mainWin->centralWidget());
									// 	// foreach (QDockWidget *dw, mainWin->findChildren<QDockWidget*>()) {
									// 	//     dw->updateGeometry();
									// 	// 	obs_log(LOG_ERROR, "Dock owner: %p:%p", dw->parent(), dw->parentWidget());
									// 	// }
									//
									// 	// mainWin->update();
									// }

									// {
									// 	mainWin->setCentralWidget(new QWidget(mainWin));
									// 	mainWin->setDockNestingEnabled(true);
									// }

									// {
									// 	mainWin->setCentralWidget(nullptr);
									// 	mainWin->setDockNestingEnabled(true);
									//
									// 	QByteArray state = mainWin->saveState();
									// 	mainWin->restoreState(state);
									//
									// 	// Force a full update of the layout engine
									// 	if (mainWin->layout()) {
									// 		mainWin->layout()->activate();
									// 	}
									// }

									{
										mainWin->setDockNestingEnabled(true);

										QWidget* oldCenter = mainWin->takeCentralWidget();

										// auto dock = new QDockWidget("Main Central", mainWin);
										// dock->setWidget(oldCenter);
										// mainWin->addDockWidget(Qt::RightDockWidgetArea, dock);
										// mainWin->setCentralWidget(new QWidget(mainWin)); // Set a new central widget to anchor docks properly
									}

									{
										// // 1. Take the widget
										//     QWidget* old = mainWin->takeCentralWidget();
										//     // if (old) old->deleteLater();
										//
										//     // 2. Set center to null so it's logically gone
										//     mainWin->setCentralWidget(nullptr);
										//
										//     // 3. THE FIX: Toggle Side Docks (Vertical Docks)
										//     // This forces the layout engine to throw away the 3-column splitter logic
										//     // and rebuild the entire dock tree.
										//     bool sideDocks = mainWin->isAnimated(); // Temporary bit to trigger a refresh
										//
										//     // This is the OBS-specific way to trigger the refresh:
										//     // We toggle the side dock corner occupation.
										//     mainWin->setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
										//     mainWin->setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea); // Toggle it back
										//
										//     // 4. Force a manual layout activation
										//     if (mainWin->layout()) {
										// 	mainWin->layout()->invalidate();
										// 	mainWin->layout()->activate();
										//     }
									}

									obs_log(LOG_INFO, "Applied main window tweaks.");

									g_tweaked_x = true;
								}

								// canvasEditor->hide();

								if (programOptions) {
									// ideas:
									//	- toggle visibility
									//      - move to a dock

									// FIXME: this might mess with the tBar fixup workaround, take a look at that too,

									// TODO: When Set to dock mode, Add a programOptions dock to obs frontend.
									// Show text when studio mode is disabled else the programOptions widget

									if (impl->programOptionsStateVal == 1) { // Hide
										programOptions->hide();
										obs_log(LOG_ERROR, "Hiding programOptions");
									} else if (impl->programOptionsStateVal == 2) { // Dock
										programOptions->hide();
										programOptions->setParent(nullptr);

										if (!programOptionsDock) {
											auto *dock = new QDockWidget("Program Options", mainWin);
											dock->setFloating(false); // Default docked
											mainWin->addDockWidget(Qt::RightDockWidgetArea, dock);
											programOptionsDock = dock;
										}

										programOptionsDock->setWidget(programOptions);
										programOptionsDock->show();
									} else {
										// Default state
									}

									// programOptions->hide();
									// if we don't hide it, it becomes a window and pops out when we detach it from the widget tree.

									// programOptions->setParent(nullptr);
									// obs_log(LOG_ERROR, "Hiding original programOptions");
								}
							} else {
								if (programOptionsDock) {
									programOptionsDock->hide();
									programOptionsDock->setWidget(nullptr);
								}
							}
						}
					}
					auto *previewContainer = mainWin->findChild<QWidget *>(QStringLiteral("previewContainer"));
					obs_log(LOG_ERROR, "Found previewContainer: %s", previewContainer ? "Yes" : "No");
					if (previewContainer) {
						auto *previewTextLayout = previewContainer->findChild<QVBoxLayout *>(QStringLiteral("previewTextLayout"));
						auto *previewLabel = previewContainer->findChild<QLabel *>(QStringLiteral("previewLabel"));
						obs_log(LOG_ERROR, "Found previewTextLayout: %s", !!previewTextLayout ? "Yes" : "No");
						obs_log(LOG_ERROR, "Found previewLabel: %s", !!previewLabel ? "Yes" : "No");
					}
					auto *contextContainer = mainWin->findChild<QFrame *>(QStringLiteral("contextContainer"));
					obs_log(LOG_ERROR, "Found contextContainer: %s", !!contextContainer ? "Yes" : "No");
					if (contextContainer) {
						// ...
					}

				}
			}
		},
		this);

	g_initialized = true;
}

TweaksImpl::~TweaksImpl()
{
	g_instances--;

	// Clean up if needed
	if (g_initialized && g_instances <= 0) {
		obs_log(LOG_WARNING, "TweaksImpl: cleaning up, no more instances remain.");
		g_initialized = false;
	}
}



void TweaksImpl::FrontendReady()
{
	// Called when OBS frontend is ready.
	// We can apply saved tweaks here if we implement saving.
	// For now, it just signals readiness for any lazy operations.
	// And ensures we can find widgets.
	ApplyTweaks();
}

void TweaksImpl::ApplyTweaks(bool force)
{
	if (force || (mainProgramPreviewLayoutState.currentState != mainProgramPreviewLayoutStateVal)) {
		// Default: 0, Hide: 1, Dock: 2
		switch (mainProgramPreviewLayoutStateVal) {
		case 0: OBSFrontendTweaker::CentralWidgetReset(); break;
		case 1: OBSFrontendTweaker::CentralWidgetSetVisible(false); break;
		case 2: OBSFrontendTweaker::CentralWidgetMakeDockable(); break;
		default: break;
		}
	}

	if (force || (programOptionsState.currentState != programOptionsStateVal)) {
		// Default: 0, Hide: 1, Dock: 2
		switch (programOptionsStateVal) {
		case 0: OBSFrontendTweaker::ProgramOptionsReset(); break;
		case 1: OBSFrontendTweaker::ProgramOptionsSetVisible(false); break;
		case 2: OBSFrontendTweaker::ProgramOptionsMakeDockable(); break;
		default: break;
		}
	}

	SetWidgetState(programOptionsState, "programOptions", programOptionsStateVal, "Program Options");
	SetWidgetState(programLayoutState, "programLabel", programLayoutStateVal, "Program Layout");
	SetWidgetState(previewLayoutState, "previewLabel", previewLayoutStateVal, "Preview Layout");
	SetWidgetState(mainProgramPreviewLayoutState, "canvasEditor", mainProgramPreviewLayoutStateVal, "Main Program Preview Layout");
}

void TweaksImpl::SetProgramOptionsState(int state)
{
	programOptionsStateVal = state;
}

void TweaksImpl::SetProgramLayoutState(int state)
{
	programLayoutStateVal = state;
}

void TweaksImpl::SetPreviewLayoutState(int state)
{
	previewLayoutStateVal = state;
}

void TweaksImpl::SetMainProgramPreviewLayoutState(int state)
{
	mainProgramPreviewLayoutStateVal = state;
}

QWidget *TweaksImpl::FindWidget(const QString &name)
{
	QMainWindow *mainWin = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!mainWin) return nullptr;
	return mainWin->findChild<QWidget *>(name);
}

void TweaksImpl::SetWidgetState(WidgetState &ctx, const QString &name, int state, const QString &dockTitle)
{

	ctx.currentState = state;

	return;

	QWidget *w = nullptr;
	
	// Initial finding logic
	if (ctx.widget) {
		w = ctx.widget;
	} else {
		// Try to find the target widget fresh
		QWidget *found = FindWidget(name);
		
		// Logic to find container if searching by label
		if (name.contains("Label") && found) {
			// If we found the label, we usually want its parent container
			// e.g. previewContainer or programContainer
			if (found->parentWidget() && found->parentWidget()->objectName().contains("Container")) {
				w = found->parentWidget();
			} else {
				// Fallback: try to find by specific container names if label parent isn't obvious
				if (name == "programLabel") w = FindWidget("programContainer");
				if (name == "previewLabel") w = FindWidget("previewContainer");
				
				// Final fallback: use label parent
				if (!w && found->parentWidget()) w = found->parentWidget();
				if (!w) w = found;
			}
		} else {
			w = found;
		}
	}

	if (!w) {
		obs_log(LOG_WARNING, "TweaksImpl: Could not find widget '%s'", name.toStdString().c_str());
		return;
	}

	// 0=Default, 1=Hide, 2=Dock

	// Clean up previous state (Revert logic)
	// If currently docked, close dock
	if (ctx.dock) {
		// Restore to original parent
		if (ctx.originalParent && ctx.widget) {
			// Detach from dock first
			ctx.dock->setWidget(nullptr); 
			ctx.dock->close();
			delete ctx.dock;
			ctx.dock = nullptr;

			// Re-insert into layout
			if (ctx.originalLayout) {
				// Try to cast to box layout
				if (auto box = qobject_cast<QBoxLayout*>(ctx.originalLayout)) {
					// Insert at index if valid
					if (ctx.originalIndex >= 0 && ctx.originalIndex <= box->count())
						box->insertWidget(ctx.originalIndex, w);
					else
						box->addWidget(w);
				} else if (auto grid = qobject_cast<QGridLayout*>(ctx.originalLayout)) {
					// Grid is harder. Just addWidget.
					grid->addWidget(w); // We lose row/col info unless stored.
				} else {
					ctx.originalLayout->addWidget(w);
				}
			}
			w->show();
		}
	}
	
	// Ensure visible if it was hidden
	w->setVisible(true);

	// Now apply new state
	if (state == 0) { // Default
		// Already reverted above.
	} else if (state == 1) { // Hide
		w->setVisible(false);
	} else if (state == 2) { // Dock
		// Save state for restoring
		ctx.widget = w;
		ctx.originalParent = w->parentWidget();
		if (ctx.originalParent) {
			ctx.originalLayout = ctx.originalParent->layout();
			if (ctx.originalLayout) {
				ctx.originalIndex = ctx.originalLayout->indexOf(w);
			}
		}

		// Create Dock
		QMainWindow *mainWin = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		QDockWidget *dock = new QDockWidget(dockTitle, mainWin);
		dock->setObjectName(QString(dockTitle).remove(" ") + "Dock");
		dock->setWidget(w);
		dock->setFloating(false); // Default docked
		mainWin->addDockWidget(Qt::RightDockWidgetArea, dock);
		dock->show();

		ctx.dock = dock;
	}
}
