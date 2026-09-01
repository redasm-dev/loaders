#include "z80.h"
#include "common.h"
#include "z80/z80_format.h"

static bool z80_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    if(rd_stricmp(req->ext, "z80") != 0) return false;

    Z80Format* z80 = (Z80Format*)ldr;
    if(!z80_read_header_base(req->input, &z80->hdr_base)) return false;

    u8 int_mode = z80->hdr_base.im_flags & 0x03;
    if(int_mode > 2) return false;

    if(z80->hdr_base.pc != 0) {
        z80->version = 1;
    }
    else {
        if(!z80_read_header_ext(req->input, &z80->hdr_ext)) return false;

        if(z80->hdr_ext.hw_mode > 1) {
            RD_LOG_WARN(
                "z80: hardware mode %d not supported yet (48k-class only)",
                z80->hdr_ext.hw_mode);
            return false;
        }

        if(z80->hdr_ext.length == Z80_HEADER_LEN_V2)
            z80->version = 2;
        else if(z80->hdr_ext.length == Z80_HEADER_LEN_V3 ||
                z80->hdr_ext.length == Z80_HEADER_LEN_V3_EXT)
            z80->version = 3;
        else
            return false;
    }

    z80->body_start = rd_reader_tell(req->input);
    return true;
}

static bool z80_load(RDLoader* ldr, RDContext* ctx) {
    zx_setup_string_terminators(ctx);

    Z80Format* z80 = (Z80Format*)ldr;
    RDReader* r = rd_get_input_reader(ctx);

    z80_init_registers(ctx, z80);

    switch(z80->version) {
        case 1: return z80_load_v1(ctx, r, z80);

        case 2:
        case 3: return z80_load_v2_v3(ctx, r, z80);

        default: break;
    }

    RD_LOG_FAIL("unsupported version %d", z80->version);
    return false;
}

static const char* z80_get_processor(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "z80";
}

static const char* z80_get_name(const RDLoader* ldr) {
    const Z80Format* z80 = (const Z80Format*)ldr;
    return rd_format("ZX Spectrum Z80 v%d snapshot", z80->version);
}

const RDLoaderPlugin Z80_LOADER = {
    .id = "zx_spectrum_z80",
    .instance_size = sizeof(Z80Format),
    .parse = z80_parse,
    .load = z80_load,
    .get_processor = z80_get_processor,
    .get_name = z80_get_name,
};
