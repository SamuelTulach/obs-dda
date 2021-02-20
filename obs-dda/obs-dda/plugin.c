#include <obs-module.h>
#include "log.h"

OBS_DECLARE_MODULE()

extern struct obs_source_info dda_source;

bool obs_module_load(void)
{
    info("Module loaded. Build on %s.", __DATE__);

    obs_register_source(&dda_source);
    return true;
}