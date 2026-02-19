#include "tweaks_panel.hpp"
#include <QDialog>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>

TweaksPanel::TweaksPanel(TweaksImpl *impl, QWidget *parent) : QDialog(parent), impl(impl)
{
	setWindowTitle("Super Suite Tweaks");
	setMinimumWidth(600);
	SetupUi();
}

TweaksPanel::~TweaksPanel() {}

void TweaksPanel::SetupUi()
{
	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	QGroupBox *group = new QGroupBox("Studio Mode UI Controls", this);
	QGridLayout *layout = new QGridLayout(group);

	// Program Options
	layout->addWidget(new QLabel("Program Options:"), 0, 0);
	comboProgramOptions = new QComboBox(this);
	comboProgramOptions->addItems({"Default", "Hide", "Dock"});
	layout->addWidget(comboProgramOptions, 0, 1);

	// Program Layout
	layout->addWidget(new QLabel("Program Layout:"), 1, 0);
	comboProgramLayout = new QComboBox(this);
	comboProgramLayout->addItems({"Default", "Hide", "Dock"});
	layout->addWidget(comboProgramLayout, 1, 1);

	// Preview Layout
	layout->addWidget(new QLabel("Preview Layout:"), 2, 0);
	comboPreviewLayout = new QComboBox(this);
	comboPreviewLayout->addItems({"Default", "Hide", "Dock"});
	layout->addWidget(comboPreviewLayout, 2, 1);

	// Main Program Preview Layout
	layout->addWidget(new QLabel("Main Program Preview Layout:"), 3, 0);
	comboMainProgramPreviewLayout = new QComboBox(this);
	comboMainProgramPreviewLayout->addItems({"Default", "Hide", "Dock"});
	layout->addWidget(comboMainProgramPreviewLayout, 3, 1);

	// Connect signals
	// Connect signals
	if (impl) {
		connect(comboProgramOptions, QOverload<int>::of(&QComboBox::currentIndexChanged),
			[this](int index) { impl->SetProgramOptionsState(index); });

		connect(comboProgramLayout, QOverload<int>::of(&QComboBox::currentIndexChanged),
			[this](int index) { impl->SetProgramLayoutState(index); });

		connect(comboPreviewLayout, QOverload<int>::of(&QComboBox::currentIndexChanged),
			[this](int index) { impl->SetPreviewLayoutState(index); });

		connect(comboMainProgramPreviewLayout, QOverload<int>::of(&QComboBox::currentIndexChanged),
			[this](int index) { impl->SetMainProgramPreviewLayoutState(index); });

		// Set initial values
		QSignalBlocker b1(comboProgramOptions);
		QSignalBlocker b2(comboProgramLayout);
		QSignalBlocker b3(comboPreviewLayout);
		QSignalBlocker b4(comboMainProgramPreviewLayout);
		comboProgramOptions->setCurrentIndex(impl->GetProgramOptionsState());
		comboProgramLayout->setCurrentIndex(impl->GetProgramLayoutState());
		comboPreviewLayout->setCurrentIndex(impl->GetPreviewLayoutState());
		comboMainProgramPreviewLayout->setCurrentIndex(impl->GetMainProgramPreviewLayoutState());
	}

	mainLayout->addWidget(group);

	QHBoxLayout *btnLayout = new QHBoxLayout();
	mainLayout->addLayout(btnLayout);

	QPushButton *applyBtn = new QPushButton("Apply", this);
	connect(applyBtn, &QPushButton::clicked, [this]() {
		if (impl)
			impl->ApplyTweaks();
	});
	btnLayout->addWidget(applyBtn);

	QPushButton *closeBtn = new QPushButton("Close", this);
	connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
	btnLayout->addWidget(closeBtn);
}
