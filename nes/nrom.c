#include "nrom.h"
#include "common.h"
#include "ines.h"
#include <inttypes.h>

#define NROM_CPU_BASE 0x8000
#define NROM_CPU_END 0x10000    // exclusive, covers up to $FFFF
#define NROM_BANK_SIZE 0x4000UL // 16KB

static bool nrom_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    if(rd_stricmp(req->ext, "nes") != 0) return false;

    INesHeader* hdr = (INesHeader*)ldr;
    if(!ines_parse_header(req->input, hdr)) return false;

    if(hdr->mapper != 0) {
        RD_LOG_WARN("mapper #%" PRId16 " not supported yet", hdr->mapper);
        return false;
    }

    return true;
}

static bool nrom_load(RDLoader* ldr, RDContext* ctx) {
    INesHeader* hdr = (INesHeader*)ldr;
    rd_map_segment(ctx, "PRG", NROM_CPU_BASE, NROM_CPU_END, RD_SP_RX);
    nes_map_memory(ctx);

    if(hdr->prg_size >= NROM_BANK_SIZE * 2) {
        // NROM-256: full 32KB PRG, no mirroring needed.
        rd_map_input_n(ctx, hdr->prg_offset, NROM_CPU_BASE, NROM_BANK_SIZE * 2);
    }
    else {
        // NROM-128: only 16KB PRG.
        // Real hardware mirrors it into both $8000-$FFFF and $C000-$FFFF
        rd_map_input_n(ctx, hdr->prg_offset, NROM_CPU_BASE, NROM_BANK_SIZE);
        rd_map_input_n(ctx, hdr->prg_offset, NROM_CPU_BASE + NROM_BANK_SIZE,
                       NROM_BANK_SIZE);
    }

    // CHR-ROM lives on the PPU bus, not the CPU's: there is no 6502
    // address that maps to it, so there's nothing to map here.
    // Deliberately left unmapped rather than silently dropped: logged so it's
    // visible this is a scope cut, not a bug, if someone comes looking for tile
    // data.
    if(hdr->chr_size) {
        RD_LOG_INFO("NROM: %u bytes of CHR-ROM present, not mapped (PPU "
                    "bus, out of scope for the 6502 CPU loader)",
                    hdr->chr_size);
    }

    u16 reset;
    if(!rd_read_le16(ctx, 0xFFFC, &reset))
        return false; // no RESET vector, no entry point
    rd_set_entry_point(ctx, reset, "RESET");

    u16 nmi, irq;
    if(rd_read_le16(ctx, 0xFFFA, &nmi) && nmi) {
        rd_set_function(ctx, nmi);
        rd_library_name(ctx, nmi, "NMI_Handler");
    }

    if(rd_read_le16(ctx, 0xFFFE, &irq) && irq) {
        rd_set_function(ctx, irq);
        rd_library_name(ctx, irq, "IRQ_BRK_Handler");
    }

    return true;
}

static const char* nrom_get_processor(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "mos6502";
}

static const char* nrom_get_name(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "NES ROM (NROM / Mapper 0)";
}

const RDLoaderPlugin NROM_LOADER = {
    .id = "nes_nrom",
    .instance_size = sizeof(INesHeader),
    .parse = nrom_parse,
    .load = nrom_load,
    .get_processor = nrom_get_processor,
    .get_name = nrom_get_name,
};
