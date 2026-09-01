#include "nrom.h"
#include <redasm/redasm.h>

static void nes_module_load(void) { rd_register_loader(&NROM_LOADER); }

RD_MODULE_EXPORT = {
    .api_version = RD_API_VERSION,
    .load = nes_module_load,
};
