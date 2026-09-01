#include "bios.h"
#include "exe.h"

static void psx_module_load(void) {
    rd_register_loader(&PSX_BIOS_LOADER);
    rd_register_loader(&PSX_EXE_LOADER);
}

RD_MODULE_EXPORT = {
    .api_version = RD_API_VERSION,
    .load = psx_module_load,
};
