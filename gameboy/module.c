#include "gb/loader.h"
#include "gb/platform.h"
#include "gba.h"
#include "sm83/sm83.h"
#include <redasm/redasm.h>

static void gameboy_module_load(void) {
    rd_register_processor(&SM83);

    rd_register_platform(&GB_PLATFORM);

    rd_register_loader(&GB_LOADER);
    rd_register_loader(&GBA_LOADER);
}

RD_MODULE_EXPORT = {
    .api_version = RD_API_VERSION,
    .load = gameboy_module_load,
};
