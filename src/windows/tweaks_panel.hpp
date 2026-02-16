#pragma once

#include "utils/tweaks_impl.hpp"

#include <QComboBox>
#include <QDialog>

class TweaksPanel : public QDialog {
	Q_OBJECT

public:
	explicit TweaksPanel(TweaksImpl *impl, QWidget *parent = nullptr);
	~TweaksPanel() override;

private:
	void SetupUi();

	TweaksImpl *impl;

	QComboBox *comboProgramOptions;
	QComboBox *comboProgramLayout;
	QComboBox *comboPreviewLayout;
};
