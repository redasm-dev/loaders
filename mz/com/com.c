#include "com.h"
#include "common.h"
#include <string.h>

#define COM_ENTRY 0x0100
#define COM_MEMSEG_SIZE 0x10000

static bool com_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    RD_UNUSED(ldr);
    if(rd_reader_expect_le16(req->input, IMAGE_DOS_SIGNATURE)) return false;

    return !rd_stricmp(req->ext, "COM") &&
           rd_reader_get_length(req->input) <= 0xff00;
}

static bool com_load(RDLoader* ldr, RDContext* ctx) {
    RD_UNUSED(ldr);

    RDReader* r = rd_get_input_reader(ctx);
    usize len = rd_reader_get_length(r);

    rd_map_segment_n(ctx, "MEM", 0, COM_MEMSEG_SIZE, RD_SP_RWX);
    rd_map_input_n(ctx, 0, COM_ENTRY, len);
    rd_set_entry_point(ctx, COM_ENTRY, NULL);
    return true;
}

static const char* com_get_processor(RDLoader* ldr, const RDContext* ctx) {
    RD_UNUSED(ldr);
    RD_UNUSED(ctx);
    return "x86_16_dos";
}

const RDLoaderPlugin COM_LOADER = {
    .level = RD_API_LEVEL,
    .id = "mz_com",
    .name = "COM Executable",
    .parse = com_parse,
    .load = com_load,
    .get_processor = com_get_processor,
};
