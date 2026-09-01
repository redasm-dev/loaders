#include "sna/sna.h"
#include "tap/tap.h"
#include "z80/z80.h"
#include <redasm/redasm.h>

static void zx_plugin_load(void) {
    rd_register_loader(&SNA_LOADER);
    rd_register_loader(&Z80_LOADER);
    rd_register_loader(&TAP_LOADER);
}

RD_MODULE_EXPORT = {
    .api_version = RD_API_VERSION,
    .load = zx_plugin_load,
};
