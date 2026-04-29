#include "com/com.h"
#include "env/dos.h"
#include "pe/pe.h"

void rd_plugin_create(void) {
    rd_register_processor(&X86_16_DOS);
    rd_register_loader(&COM_LOADER);
    rd_register_loader(&PE_LOADER);
}
