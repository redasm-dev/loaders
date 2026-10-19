#include "gba/gba.h"
#include <redasm/redasm.h>

static void gameboy_module_load(void) { rd_register_loader(&GBA); }

RD_MODULE_EXPORT = {
    .api_version = RD_API_VERSION,
    .load = gameboy_module_load,
};
