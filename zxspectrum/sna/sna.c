#include "sna.h"
#include "common.h"
#include "sna_format.h"

static bool sna_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    if(rd_stricmp(req->ext, "sna") != 0) return false;

    usize len = rd_reader_get_length(req->input);

    switch(len) {
        case SNA_48K_SNAPSHOT:
        case SNA_128K_SNAPSHOT:
        case SNA_128K_EXT_SNAPSHOT: break;

        default: return false;
    }

    SNAFormat* sna = (SNAFormat*)ldr;
    if(!sna_read_header(req->input, &sna->header)) return false;
    if(sna->header.int_flag & ~0x04) return false;
    if(sna->header.int_mode > 2) return false;

    if(sna->header.border_color > 7 && sna->header.border_color != 0x71 &&
       sna->header.border_color != 0xC9)
        return false;

    sna->length = len;
    sna->ram_start = rd_reader_tell(req->input);
    sna->ram_length = len - rd_reader_tell(req->input);
    return true;
}

static bool sna_load(RDLoader* ldr, RDContext* ctx) {
    zx_setup_string_terminators(ctx);

    SNAFormat* sna = (SNAFormat*)ldr;
    RDReader* r = rd_get_input_reader(ctx);
    sna_init_registers(ctx, sna);

    switch(sna->length) {
        case SNA_48K_SNAPSHOT: return sna_load_48k(ctx, sna);
        // case SNA_128K_SNAPSHOT: break;
        // case SNA_128K_EXT_SNAPSHOT: break;
        default: break;
    }

    return true;
}

static const char* sna_get_processor(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "z80";
}

static const char* sna_get_name(const RDLoader* ldr) {
    const SNAFormat* sna = (const SNAFormat*)ldr;

    switch(sna->length) {
        case SNA_48K_SNAPSHOT: return "ZX Spectrum 48K snapshot";
        case SNA_128K_SNAPSHOT: return "ZX Spectrum 128K snapshot";

        case SNA_128K_EXT_SNAPSHOT:
            return "ZX Spectrum 128K snapshot (extended)";

        default: break;
    }

    return "ZX Spectrum Snapshot";
}

const RDLoaderPlugin SNA_LOADER = {
    .id = "zx_spectrum_sna",
    .instance_size = sizeof(SNAFormat),
    .parse = sna_parse,
    .load = sna_load,
    .get_processor = sna_get_processor,
    .get_name = sna_get_name,
};
