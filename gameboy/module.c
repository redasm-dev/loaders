#include "gb.h"
#include "gba.h"
#include "sm83/sm83.h"
#include <redasm/redasm.h>

static void gameboy_module_load(void) {
    rd_register_processor(&SM83);

    rd_register_loader(&GB);
    rd_register_loader(&GBA);
}

RD_MODULE_EXPORT = {
    .api_version = RD_API_VERSION,
    .load = gameboy_module_load,
};
