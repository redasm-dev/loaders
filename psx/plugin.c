#include "bios.h"
#include "exe.h"

void rd_plugin_create(void) {
    rd_register_loader(&PSX_BIOS_LOADER);
    rd_register_loader(&PSX_EXE_LOADER);
}
