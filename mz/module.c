#include "com/com.h"
#include "common/dos.h"
#include "le/le.h"
#include "mz/mz.h"
#include "ne/ne.h"
#include "pe/pe.h"

static void mz_module_load(void) {
    rd_register_platform(&DOS);

    rd_register_loader(&COM);
    rd_register_loader(&MZ);
    rd_register_loader(&NE);
    rd_register_loader(&LE);
    rd_register_loader(&PE);
}

RD_MODULE_EXPORT = {
    .api_version = RD_API_VERSION,
    .load = mz_module_load,
};
