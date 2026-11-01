#pragma once

#include <redasm/redasm.h>

#define GB_BASE 0x100

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

typedef enum { GB_MAPPERS(GB_MAPPER_ENUM_X) } GBMappers;

bool gb_read_cartridge(RDReader* r, GBCartridge* v);
