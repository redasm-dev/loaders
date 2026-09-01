#include "com.h"
#include "common/common.h"
#include "hooks.h"
#include <string.h>

#define COM_SEG_START 0x0100
#define COM_SEG_END 0x10000

static bool com_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    RD_UNUSED(ldr);
    if(rd_reader_expect_le16(req->input, MZ_DOS_SIGNATURE)) return false;

    return !rd_stricmp(req->ext, "COM") &&
           rd_reader_get_length(req->input) <= 0xff00;
}

static bool com_load(RDLoader* ldr, RDContext* ctx) {
    RD_UNUSED(ldr);
    mz_register_dos_hooks(ctx);

    RDReader* r = rd_get_input_reader(ctx);
    usize len = rd_reader_get_length(r);

    rd_map_segment(ctx, "MEM", COM_SEG_START, COM_SEG_END, RD_SP_RWX);
    rd_map_input_n(ctx, 0, COM_SEG_START, len);
    rd_set_entry_point(ctx, COM_SEG_START, NULL);
    return true;
}

static const char* com_get_name(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "COM Executable";
}

static const char* com_get_processor(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "x86_16_real";
}

const RDLoaderPlugin COM_LOADER = {
    .id = "dos_com",
    .get_name = com_get_name,
    .get_processor = com_get_processor,
    .parse = com_parse,
    .load = com_load,
};
