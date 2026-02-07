#include "./plugin-main.h"

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