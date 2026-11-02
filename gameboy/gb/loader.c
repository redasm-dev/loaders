#include "loader.h"
#include "gb/cartridge.h"
#include "gb/sym.h"
#include <string.h>

#define GB_BLOCK_SIZE 512
#define GB_HEADER_CHECKSUM_START 0x134
#define GB_HEADER_CHECKSUM_END 0x14d

#define GB_MIN_ROM_SIZE 0x150
#define GB_ROM_BANK_SIZE 0x4000

#define GB_ROM0_END 0x4000
#define GB_ROMX_START 0x4000
#define GB_VRAM_START 0x8000
#define GB_SRAM_START 0xA000
#define GB_SRAM_SIZE 0x2000
#define GB_WRAM0_START 0xC000
#define GB_WRAMX_START 0xD000
#define GB_WRAMX_SIZE 0x1000
#define GB_VRAM_SIZE 0x2000

#define GB_CGB_BANKS_WRAM 8

#define GB_OPT_LOAD_SYM "gb_load_sym"

#define GB_MAPPER_CASE_X(id, val, name)                                        \
    case val: return #name;

static const char* _gb_get_mapper(const GBCartridge* gb) {
    switch(gb->cartridge_type) {
        GB_MAPPERS(GB_MAPPER_CASE_X)
        default: break;
    }

    return NULL;
}

static const char* _gb_get_rom_size(const GBCartridge* gb) {
    switch(gb->rom_size) {
        case 0: return "256Kbit";
        case 1: return "512Kbit";
        case 2: return "1Mbit";
        case 3: return "2Mbit";
        case 4: return "4Mbit";
        case 5: return "8Mbit";
        case 6: return "16Mbit";
        case 7: return "32Mbit";
        case 0x52: return "9Mbit";
        case 0x53: return "10Mbit";
        case 0x54: return "12Mbit";
        default: break;
    }

    return NULL;
}

static bool _gb_is_cgb(const GBCartridge* gb) { return gb->cgb_flag & 0x80; }

static void _gb_read_global_checksum_block(const RDReaderBlock* b) {
    u32* checksum = (u32*)b->userdata;

    for(usize i = 0; i < b->length; i++) {
        usize off = b->pos + i;

        // ignore locations where global checksum is stored
        if(off == 0x014E || off == 0x014F) continue;

        *checksum += (u8)b->data[i];
    }
}

static u16 _gb_calculate_global_checksum(RDReader* r) {
    u32 checksum = 0;

    rd_reader_save(r);
    rd_reader_seek(r, 0);
    rd_reader_read_block(r, rd_reader_get_length(r), GB_BLOCK_SIZE,
                         _gb_read_global_checksum_block, &checksum);
    rd_reader_restore(r);

    return (u16)checksum;
}

static bool _gb_validate_global_checksum(RDReader* r, const GBCartridge* gb) {
    u16 calculated = _gb_calculate_global_checksum(r);
    return calculated == gb->global_checksum;
}

static bool _gb_validate_header_checksum(RDReader* r, const GBCartridge* gb) {
    rd_reader_save(r);
    rd_reader_seek(r, GB_HEADER_CHECKSUM_START);

    u8 chunk[GB_HEADER_CHECKSUM_END - GB_HEADER_CHECKSUM_START];

    if(!rd_reader_read_exact(r, &chunk, sizeof(chunk))) {
        rd_reader_restore(r);
        return false;
    }

    u8 checksum = 0;
    for(usize i = 0; i < sizeof(chunk); i++)
        checksum = checksum - chunk[i] - 1;

    rd_reader_restore(r);
    return checksum == gb->header_checksum;
}

// Number of SRAM banks declared by the header (0 = none / unknown).
static usize _gb_sram_banks(const GBCartridge* gb) {
    switch(gb->ram_size) {
        case 2: return 1;  // 8 KB
        case 3: return 4;  // 32 KB
        case 4: return 16; // 128 KB
        case 5: return 8;  // 64 KB
        default: break;
    }

    return 0;
}

static void _gb_map_rom(RDContext* ctx, usize n_banks) {
    // bank 0 is fixed at 0x0000, banks 1..N live in the 0x4000 window
    rd_map_segment_n(ctx, "ROM00", 0, GB_ROM_BANK_SIZE, RD_SP_RX);
    rd_map_input_n(ctx, 0, 0, GB_ROM_BANK_SIZE);

    for(usize i = 1; i < n_banks; i++) {
        const char* id = rd_format("ROM%02X", (unsigned int)i);
        RDRelAddress start = GB_BANKED(i, GB_ROMX_START);

        RDSegment* seg =
            rd_map_segment_n(ctx, id, start, GB_ROM_BANK_SIZE, RD_SP_RX);
        if(seg) rd_segment_set_bank(seg, i, GB_BANKED(i, 0), ctx);
        rd_map_input_n(ctx, (RDOffset)(i * GB_ROM_BANK_SIZE), start,
                       GB_ROM_BANK_SIZE);
    }
}

static void _gb_map_ram(RDContext* ctx, const GBCartridge* gb) {
    rd_map_segment_n(ctx, "VRAM", GB_VRAM_START, GB_VRAM_SIZE, RD_SP_RW);
    rd_map_segment_n(ctx, "ERAM", GB_SRAM_START, GB_SRAM_SIZE, RD_SP_RW);
    rd_map_segment_n(ctx, "WRAM00", GB_WRAM0_START, GB_WRAMX_SIZE, RD_SP_RW);
    rd_map_segment_n(ctx, "WRAM01", GB_WRAMX_START, GB_WRAMX_SIZE, RD_SP_RW);

    // SRAM banks: bank 0 is the plain one
    usize sram_banks = _gb_sram_banks(gb);

    for(usize i = 1; i < sram_banks; i++) {
        rd_map_segment_n(ctx, rd_format("ERAM%02X", (unsigned int)i),
                         GB_BANKED(i, GB_SRAM_START), GB_SRAM_SIZE, RD_SP_RW);
    }

    if(!_gb_is_cgb(gb)) return;

    // CGB: second VRAM bank, WRAM banks 2..7 (bank 1 is the plain WRAM01)
    rd_map_segment_n(ctx, "VRAM01", GB_BANKED(1, GB_VRAM_START), GB_VRAM_SIZE,
                     RD_SP_RW);

    for(usize i = 2; i < GB_CGB_BANKS_WRAM; i++) {
        rd_map_segment_n(ctx, rd_format("WRAM%02X", (unsigned int)i),
                         GB_BANKED(i, GB_WRAMX_START), GB_WRAMX_SIZE, RD_SP_RW);
    }
}

static bool gb_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    if(rd_reader_get_length(req->input) < GB_MIN_ROM_SIZE) return false;

    GBCartridge* gb = (GBCartridge*)ldr;
    rd_reader_seek(req->input, GB_BASE);

    if(!gb_read_cartridge(req->input, gb)) return false;
    if(!_gb_validate_header_checksum(req->input, gb)) return false;
    if(!_gb_validate_global_checksum(req->input, gb)) return false;
    if(!_gb_get_mapper(gb)) return false;
    if(!_gb_get_rom_size(gb)) return false;

    return true;
}

static bool gb_load(RDLoader* ldr, RDContext* ctx) {
    RDReader* r = rd_get_input_reader(ctx);
    usize size = (usize)rd_align_up(rd_reader_get_length(r), GB_ROM_BANK_SIZE);

    _gb_map_rom(ctx, size / GB_ROM_BANK_SIZE);
    _gb_map_ram(ctx, (const GBCartridge*)ldr);

    rd_map_segment(ctx, "OAM", 0xFE00, 0xFEA0, RD_SP_RW);
    rd_map_segment(ctx, "IOPORTS", 0xFF00, 0xFF80, RD_SP_RW);
    rd_map_segment(ctx, "HRAM", 0xFF80, 0x10000, RD_SP_RW);

    rd_kb_load(ctx, "platform/gameboy/gb");
    rd_library_type(ctx, GB_BASE + sizeof(u32), "GB_CARTRIDGE", 0,
                    RD_TYPE_NONE);

    rd_set_entry_point(ctx, GB_BASE, NULL);

    bool load_sym = true;
    if(rd_loader_options_get_bool(ctx, GB_OPT_LOAD_SYM, &load_sym) && load_sym)
        gb_sym_load(ctx);

    return true;
}

static const char* gb_get_processor(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "sm83";
}

static const char* gb_get_platform(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "gameboy";
}

static const char* gb_get_name(const RDLoader* ldr) {
    const GBCartridge* gb = (const GBCartridge*)ldr;

    char title[sizeof(gb->title) + 1] = {0};
    memcpy(title, gb->title, sizeof(gb->title));

    if(_gb_is_cgb(gb)) {
        return rd_format("Gameboy Color Image \"%s\" (Mapper: %s, ROM: %s)",
                         title, _gb_get_mapper(gb), _gb_get_rom_size(gb));
    }

    return rd_format("Gameboy Image \"%s\" (Mapper: %s, ROM: %s)", title,
                     _gb_get_mapper(gb), _gb_get_rom_size(gb));
}

static void gb_get_options(RDLoader* ldr, RDLoaderOptionBuilder* b) {
    RD_UNUSED(ldr);

    rd_loader_options_add_bool(b, GB_OPT_LOAD_SYM, "Load symbol file",
                               ".sym file will be parsed (if found)", true);
}

const RDLoaderPlugin GB_LOADER = {
    .id = "gameboy_gb",
    .instance_size = sizeof(GBCartridge),
    .parse = gb_parse,
    .load = gb_load,
    .get_processor = gb_get_processor,
    .get_platform = gb_get_platform,
    .get_name = gb_get_name,
    .get_options = gb_get_options,
};
