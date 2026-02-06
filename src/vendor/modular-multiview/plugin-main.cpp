#include "./plugin-main.h"

#include <obs-module.h>
#include <obs.h>
#include "level_calc.h"
#include "meter_widget.h"
#include <QApplication>
#include <QTimer>
#include <QSettings>
#include <obs-frontend-api.h>
#include "media-io/audio-io.h"
#include <cstdint>
#include "util/config-file.h"

#ifdef __cplusplus
extern "C" {
#endif

bool mmv_on_obs_module_load() {

    return true;
}

void mmv_on_obs_module_unload() {

}

#ifdef __cplusplus
}
#endif