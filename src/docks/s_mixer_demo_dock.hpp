#pragma once

#include "super/ui/widgets/s_mixer_dock.hpp"

class SMixerDemoDock : public super::SMixerDock {
	Q_OBJECT

public:
	explicit SMixerDemoDock(QWidget *parent = nullptr);
	~SMixerDemoDock() override;
};
