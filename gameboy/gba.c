#include "gba.h"
#include <ctype.h>

#define GBA_ROM_HEADER_SIZE 192
#define GBA_NINTENDO_LOGO_SIZE 156
#define GBA_GAME_TITLE_SIZE 12
#define GBA_GAME_CODE_SIZE 4
#define GBA_MAKER_CODE_SIZE 2

// https://www.reinterpretcast.com/writing-a-game-boy-advance-game
// https://problemkaputt.de/gbatek.htm
#define GBA_EWRAM_START 0x02000000
#define GBA_EWRAM_SIZE 0x00040000
#define GBA_IWRAM_START 0x03000000
#define GBA_IWRAM_SIZE 0x00008000
#define GBA_IOREG_START 0x04000000
#define GBA_IOREG_SIZE 0x00000400
#define GBA_PALETTE_START 0x05000000
#define GBA_PALETTE_SIZE 0x00000400
#define GBA_VRAM_START 0x06000000
#define GBA_VRAM_SIZE 0x00018000
#define GBA_OAM_START 0x07000000
#define GBA_OAM_SIZE 0x00000400
#define GBA_ROM_START 0x08000000
#define GBA_ROM_SIZE 0x02000000

// http://problemkaputt.de/gbatek.htm#gbacartridgeheader
typedef struct GBACartridge {
    u32 entry_point;
    u8 nintendo_logo[GBA_NINTENDO_LOGO_SIZE];
    char game_title[GBA_GAME_TITLE_SIZE];
    char game_code[GBA_GAME_CODE_SIZE];
    char maker_code[GBA_MAKER_CODE_SIZE];
    u8 fixed_val;
    u8 main_unit_code;
    u8 device_type;
    u8 reserved_area[7];
    u8 software_version;
    u8 header_checksum;
    u8 reserved_area_2[2];

    struct {
        u32 ram_entry_point;
        u8 boot_mode;
        u8 slave_id;
        u8 unused[26];
        u32 joybus_entry_point;
    } multiboot;
} GBACartridge;

static bool _gba_read_cartridge(RDReader* r, GBACartridge* v) {
    rd_reader_read_le32(r, &v->entry_point);
    rd_reader_read_exact(r, &v->nintendo_logo, sizeof(v->nintendo_logo));
    rd_reader_read_exact(r, &v->game_title, sizeof(v->game_title));
    rd_reader_read_exact(r, &v->game_code, sizeof(v->game_code));
    rd_reader_read_exact(r, &v->maker_code, sizeof(v->maker_code));
    rd_reader_read_byte(r, &v->fixed_val);
    rd_reader_read_byte(r, &v->main_unit_code);
    rd_reader_read_byte(r, &v->device_type);
    rd_reader_read_exact(r, &v->reserved_area, sizeof(v->reserved_area));
    rd_reader_read_byte(r, &v->software_version);
    rd_reader_read_byte(r, &v->header_checksum);
    rd_reader_read_exact(r, &v->reserved_area_2, sizeof(v->reserved_area_2));

    // rd_reader_read_le32(r, &v->multiboot.ram_entry_point);
    // rd_reader_read_byte(r, &v->multiboot.boot_mode);
    // rd_reader_read_byte(r, &v->multiboot.slave_id);
    // rd_reader_read_exact(r, &v->multiboot.unused,
    // sizeof(v->multiboot.unused)); rd_reader_read_le32(r,
    // &v->multiboot.joybus_entry_point);

    return !rd_reader_has_error(r);
}

static bool _gba_validate_title_or_code(const char* s, usize n) {
    for(usize i = 0; s[i] && i < n; i++) {
        if(isupper(s[i]) || ispunct(s[i]) || isdigit(s[i]) || isspace(s[i]))
            continue;

        return false;
    }

    return true;
}

static bool _gba_calculate_checksum(RDReader* r, u8* checksum) {
    *checksum = 0;

    rd_reader_save(r);
    rd_reader_seek(r, 0xA0);

    while(rd_reader_tell(r) < 0xBD) {
        u8 b;
        if(!rd_reader_read_byte(r, &b)) goto fail;
        *checksum -= b;
    }

    rd_reader_restore(r);
    *checksum -= 0x19;
    return true;

fail:
    rd_reader_restore(r);
    return false;
}

// ARM "b <label>" with cond=AL is 0xEAxxxxxx
// the target is relative to pc+8
static bool _gba_parse_address(const GBACartridge* gba, u32* addr) {
    if((gba->entry_point >> 24) != 0xEA) return false;

    u32 imm = gba->entry_point & 0x00FFFFFF;
    if(imm & 0x00800000) imm |= 0xFF000000; // sign-extend 24 -> 32 bits

    *addr = GBA_ROM_START + 8 + (imm << 2); // u32 wrap-around is well-defined
    return true;
}

static bool gba_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    GBACartridge* gba = (GBACartridge*)ldr;

    if(!_gba_read_cartridge(req->input, gba) || gba->fixed_val != 0x96)
        return false;

    if(!_gba_validate_title_or_code(gba->game_title, sizeof(gba->game_title)))
        return false;

    if(!_gba_validate_title_or_code(gba->game_code, sizeof(gba->game_code)))
        return false;

    if(!_gba_validate_title_or_code(gba->maker_code, sizeof(gba->maker_code)))
        return false;

    u8 checksum;
    if(!_gba_calculate_checksum(req->input, &checksum) ||
       gba->header_checksum != checksum)
        return false;

    return true;
}

static bool gba_load(RDLoader* ldr, RDContext* ctx) {
    GBACartridge* gba = (GBACartridge*)ldr;
    RDReader* r = rd_get_input_reader(ctx);

    // clang-format off
    rd_map_segment_n(ctx, "EWRAM", GBA_EWRAM_START, GBA_EWRAM_SIZE, RD_SP_RW);
    rd_map_segment_n(ctx, "IWRAM", GBA_IWRAM_START, GBA_IWRAM_SIZE, RD_SP_RW);
    rd_map_segment_n(ctx, "IOREG", GBA_IOREG_START, GBA_IOREG_SIZE, RD_SP_RW);
    rd_map_segment_n(ctx, "PALETTE", GBA_PALETTE_START, GBA_PALETTE_SIZE, RD_SP_RW);
    rd_map_segment_n(ctx, "VRAM", GBA_VRAM_START, GBA_VRAM_SIZE, RD_SP_RW);
    rd_map_segment_n(ctx, "OAM", GBA_OAM_START, GBA_OAM_SIZE, RD_SP_RW);
    rd_map_segment_n(ctx, "ROM", GBA_ROM_START, rd_reader_get_length(r), RD_SP_RX);
    // clang-format on

    rd_kb_load(ctx, "platform/gameboy/gba");

    rd_map_input_n(ctx, 0, GBA_ROM_START, rd_reader_get_length(r));
    rd_set_entry_point(ctx, GBA_ROM_START, NULL);
    rd_library_type(ctx, GBA_ROM_START + sizeof(u32), "GBA_CARTRIDGE", 0,
                    RD_TYPE_NONE);

    u32 target;
    usize rom_size = rd_reader_get_length(r);

    if(_gba_parse_address(gba, &target) && target >= GBA_ROM_START &&
       target - GBA_ROM_START < rom_size)
        rd_library_name(ctx, target, "init_vector");

    return true;
}

static const char* gba_get_processor(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "arm32_le";
}

static const char* gba_get_name(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "GameBoy Advance ROM";
}

const RDLoaderPlugin GBA = {
    .id = "gameboy_gba",
    .instance_size = sizeof(GBACartridge),
    .parse = gba_parse,
    .load = gba_load,
    .get_processor = gba_get_processor,
    .get_name = gba_get_name,
};
