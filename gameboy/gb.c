#include "gb.h"
#include <string.h>

#define GB_BLOCK_SIZE 512
#define GB_HEADER_CHECKSUM_START 0x134
#define GB_HEADER_CHECKSUM_END 0x14d

#define GB_BASE 0x100
#define GB_MIN_ROM_SIZE 0x150
#define GB_ROM_BANK_SIZE 0x4000

#define GB_MAPPERS(X)                                                          \
    X(ROM_ONLY, 0x00, "ROM ONLY")                                              \
    X(MBC1, 0x01, "MBC1")                                                      \
    X(MBC1_RAM, 0x02, "MBC1+RAM")                                              \
    X(MBC1_RAM_BATT, 0x03, "MBC1+RAM+BATT")                                    \
    X(MBC2, 0x05, "MBC2")                                                      \
    X(MBC2_BATTERY, 0x06, "MBC2+BATTERY")                                      \
    X(ROM_RAM, 0x08, "ROM+RAM")                                                \
    X(ROM_RAM_BATTERY, 0x09, "ROM+RAM+BATTERY")                                \
    X(MMM01, 0x0B, "MMM01")                                                    \
    X(MMM01_SRAM, 0x0C, "MMM01+SRAM")                                          \
    X(MMM01_SRAM_BATT, 0x0D, "MMM01+SRAM+BATT")                                \
    X(MBC3_TIMER_BATT, 0x0F, "MBC3+TIMER+BATT")                                \
    X(MBC3_TIMER_RAM_BATT, 0x10, "MBC3+TIMER+RAM+BATT")                        \
    X(MBC3, 0x11, "MBC3")                                                      \
    X(MBC3_RAM, 0x12, "MBC3+RAM")                                              \
    X(MBC3_RAM_BATT, 0x13, "MBC3+RAM_BATT")                                    \
    X(MBC5, 0x19, "MBC5")                                                      \
    X(MBC5_RAM, 0x1A, "MBC5+RAM")                                              \
    X(MBC5_RAM_BATT, 0x1B, "MBC5+RAM+BATT")                                    \
    X(MBC5_RUMBLE, 0x1C, "MBC5+RUMBLE")                                        \
    X(MBC5_RUMBLE_SRAM, 0x1D, "MBC5+RUMBLE+SRAM")                              \
    X(MBC5_RUMBLE_SRAM_BATT, 0x1E, "MBC5_RUMBLE+SRAM+BATT")                    \
    X(POCKET_CAMERA, 0xFC, "Pocket Camera")                                    \
    X(BANDAI_TAMA5, 0xFD, "Bandai TAMA5")                                      \
    X(HUDSON_HUC_3, 0xFE, "Hudson HuC-3")                                      \
    X(HUDSON_HUC_1, 0xFF, "Hudson HuC-1")

#define GB_MAPPER_ENUM_X(id, val, name) GB_MAPPER_ID_##id = (val),

#define GB_MAPPER_CASE_X(id, val, name)                                        \
    case val: return #name;

typedef enum { GB_MAPPERS(GB_MAPPER_ENUM_X) } GBMappers;

typedef struct GBCartridge {
    u32 entry_point;
    u8 nintendo_logo[48];
    char title[11];
    u32 manufacturer_code;
    u8 cgb_flag;
    u16 licensee_code;
    u8 sgb_flag;
    u8 cartridge_type;
    u8 rom_size;
    u8 ram_size;
    u8 destination_code;
    u8 old_licensee_code;
    u8 rom_version;
    u8 header_checksum;
    u16 global_checksum;
} GBCartridge;

static bool _gb_read_header(RDReader* r, GBCartridge* v) {
    rd_reader_read_le32(r, &v->entry_point);
    rd_reader_read_exact(r, &v->nintendo_logo, sizeof(v->nintendo_logo));
    rd_reader_read_exact(r, &v->title, sizeof(v->title));
    rd_reader_read_le32(r, &v->manufacturer_code);
    rd_reader_read_byte(r, &v->cgb_flag);
    rd_reader_read_le16(r, &v->licensee_code);
    rd_reader_read_byte(r, &v->sgb_flag);
    rd_reader_read_byte(r, &v->cartridge_type);
    rd_reader_read_byte(r, &v->rom_size);
    rd_reader_read_byte(r, &v->ram_size);
    rd_reader_read_byte(r, &v->destination_code);
    rd_reader_read_byte(r, &v->old_licensee_code);
    rd_reader_read_byte(r, &v->rom_version);
    rd_reader_read_byte(r, &v->header_checksum);
    rd_reader_read_be16(r, &v->global_checksum);

    return !rd_reader_has_error(r);
}

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
    rd_reader_read_block(r, rd_reader_get_length(r), 1, // GB_BLOCK_SIZE,
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
    if(!rd_reader_read_exact(r, &chunk, sizeof(chunk))) return false;

    u8 checksum = 0;
    for(usize i = 0; i < sizeof(chunk); i++)
        checksum = checksum - chunk[i] - 1;

    rd_reader_restore(r);
    return checksum == gb->header_checksum;
}

static bool gb_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    if(rd_reader_get_length(req->input) < GB_MIN_ROM_SIZE) return false;

    GBCartridge* gb = (GBCartridge*)ldr;
    rd_reader_seek(req->input, GB_BASE);

    if(!_gb_read_header(req->input, gb)) return false;
    if(!_gb_validate_header_checksum(req->input, gb)) return false;
    if(!_gb_validate_global_checksum(req->input, gb)) return false;
    if(!_gb_get_mapper(gb)) return false;
    if(!_gb_get_rom_size(gb)) return false;

    return true;
}

static bool gb_load(RDLoader* ldr, RDContext* ctx) {
    RD_UNUSED(ldr);

    RDReader* r = rd_get_input_reader(ctx);
    usize size = (usize)rd_align_up(rd_reader_get_length(r), GB_ROM_BANK_SIZE);
    usize n_banks = size / GB_ROM_BANK_SIZE;

    for(usize i = 0; i < n_banks; i++) {
        const char* bank_id = rd_format("ROM%02X", (unsigned int)i);
        RDRelAddress bank_base = GB_ROM_BANK_SIZE * i;
        rd_map_segment_n(ctx, bank_id, bank_base, GB_ROM_BANK_SIZE, RD_SP_RX);
        rd_map_input_n(ctx, (RDOffset)bank_base, bank_base, GB_ROM_BANK_SIZE);
    }

    rd_kb_load(ctx, "os/gameboy/gb");
    rd_library_type(ctx, GB_BASE + sizeof(u32), "GB_CARTRIDGE", 0,
                    RD_TYPE_NONE);

    rd_set_entry_point(ctx, GB_BASE, NULL);
    return true;
}

static const char* gb_get_processor(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "sm83";
}

static const char* gb_get_name(const RDLoader* ldr) {
    const GBCartridge* gb = (const GBCartridge*)ldr;

    char title[sizeof(gb->title) + 1] = {0};
    memcpy(title, gb->title, sizeof(gb->title));

    return rd_format("Gameboy Image \"%s\" (Mapper: %s, ROM: %s)", title,
                     _gb_get_mapper(gb), _gb_get_rom_size(gb));
}

const RDLoaderPlugin GB = {
    .id = "gameboy_gb",
    .instance_size = sizeof(GBCartridge),
    .parse = gb_parse,
    .load = gb_load,
    .get_processor = gb_get_processor,
    .get_name = gb_get_name,
};
