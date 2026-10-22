#include "bios.h"
#include "exe.h"
#include "psx.h"

static void psx_module_load(void) {
    rd_register_platform(&PSX);
    rd_register_loader(&PSX_BIOS);
    rd_register_loader(&PSX_EXE);
}

RD_MODULE_EXPORT = {
    .api_version = RD_API_VERSION,
    .load = psx_module_load,
};
