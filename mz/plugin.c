#include "com/com.h"
#include "le/le.h"
#include "mz/mz.h"
#include "ne/ne.h"
#include "pe/pe.h"

static void mz_plugin_load(void) {
    rd_register_loader(&COM_LOADER);
    rd_register_loader(&MZ_LOADER);
    rd_register_loader(&NE_LOADER);
    rd_register_loader(&LE_LOADER);
    rd_register_loader(&PE_LOADER);
}

RD_MODULE_EXPORT = {
    .api_version = RD_API_VERSION,
    .load = mz_plugin_load,
};
