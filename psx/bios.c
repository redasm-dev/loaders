#include "bios.h"
#include "biosfunc.h"

// 512KB, invariant across all versions
#define PSX_BIOS_SIZE 0x80000

// KSEG1 uncached ROM mirror, hardware reset vector
#define PSX_BIOS_BASE 0xBFC00000

// boot + A-function syscalls
#define PSX_BIOS_KERNEL1 0xBFC00000

// relocated to RAM at 0x500, B/C-function syscalls
#define PSX_BIOS_KERNEL2 0xBFC10000

// intro/bootmenu, decompressed to RAM
#define PSX_BIOS_BOOTMENU 0xBFC18000

// font/character sets, data only
#define PSX_BIOS_CHARACTERS 0xBFC64000

// 2MB user RAM
#define PSX_BIOS_RAM_START 0x00000000
#define PSX_BIOS_RAM_END 0x00200000

static bool psxbios_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    RD_UNUSED(ldr);
    if(rd_reader_get_length(req->input) != PSX_BIOS_SIZE) return false;

    return !rd_strnicmp(req->name, "scph", 4) ||
           !rd_strnicmp(req->name, "dtl", 3);
}

static bool psxbios_load(RDLoader* ldr, RDContext* ctx) {
    RD_UNUSED(ldr);

    // RAM - BSS, kernel uses first 64KB, user code uses rest
    if(!rd_map_segment(ctx, "RAM", PSX_BIOS_RAM_START, PSX_BIOS_RAM_END,
                       RD_SP_RWX))
        return false;

    // KERNEL1 - boot code + A-function table, executed in uncached ROM
    if(!rd_map_segment(ctx, "KERNEL1", PSX_BIOS_KERNEL1, PSX_BIOS_KERNEL2,
                       RD_SP_RX))
        return false;
    if(!rd_map_input_n(ctx, 0x00000, PSX_BIOS_KERNEL1,
                       PSX_BIOS_KERNEL2 - PSX_BIOS_KERNEL1))
        return false;

    // KERNEL2 - B/C-function table, relocated to RAM at runtime
    if(!rd_map_segment(ctx, "KERNEL2", PSX_BIOS_KERNEL2, PSX_BIOS_BOOTMENU,
                       RD_SP_RX))
        return false;
    if(!rd_map_input_n(ctx, 0x10000, PSX_BIOS_KERNEL2,
                       PSX_BIOS_BOOTMENU - PSX_BIOS_KERNEL2))
        return false;

    // BOOTMENU - intro/shell, decompressed to RAM at runtime
    if(!rd_map_segment(ctx, "BOOTMENU", PSX_BIOS_BOOTMENU, PSX_BIOS_CHARACTERS,
                       RD_SP_RX))
        return false;
    if(!rd_map_input_n(ctx, 0x18000, PSX_BIOS_BOOTMENU,
                       PSX_BIOS_CHARACTERS - PSX_BIOS_BOOTMENU))
        return false;

    // CHARACTERS - font data, no code
    if(!rd_map_segment(ctx, "CHARACTERS", PSX_BIOS_CHARACTERS,
                       PSX_BIOS_BASE + PSX_BIOS_SIZE, RD_SP_R))
        return false;
    if(!rd_map_input_n(ctx, 0x64000, PSX_BIOS_CHARACTERS,
                       PSX_BIOS_SIZE - 0x64000))
        return false;

    // name the known RAM vectors - version-agnostic fixed addresses
    rd_library_name(ctx, 0x000000A0, "PSX_VectorA");
    rd_library_name(ctx, 0x000000B0, "PSX_VectorB");
    rd_library_name(ctx, 0x000000C0, "PSX_VectorC");

    rd_register_hook(ctx, RD_HOOK_GENERAL, "redasm.finalized",
                     psx_bios_autorename_hook, NULL);
    return rd_set_entry_point(ctx, PSX_BIOS_BASE, "PSX_Reset");
}

static const char* psxbios_get_name(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "Sony PlayStation 1 BIOS";
}

static const char* psxbios_get_processor(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "mips32_le";
}

const RDLoaderPlugin PSX_BIOS_LOADER = {
    .id = "psx_bios",
    .get_name = psxbios_get_name,
    .get_processor = psxbios_get_processor,
    .parse = psxbios_parse,
    .load = psxbios_load,
};
