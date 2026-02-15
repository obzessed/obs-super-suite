#pragma once

#include <obs-module.h>

class OBSModule {
	obs_module_t *inner_;

protected:
	OBSModule(obs_module_t *inner);
	~OBSModule();
};