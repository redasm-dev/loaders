#include "com/com.h"
#include "le/le.h"
#include "mz/mz.h"
#include "ne/ne.h"
#include "pe/pe.h"

void rd_plugin_create(void) {
    rd_register_loader(&COM_LOADER);
    rd_register_loader(&MZ_LOADER);
    rd_register_loader(&NE_LOADER);
    rd_register_loader(&LE_LOADER);
    rd_register_loader(&PE_LOADER);
}
