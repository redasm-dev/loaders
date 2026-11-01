#include "cartridge.h"

bool gb_read_cartridge(RDReader* r, GBCartridge* v) {
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
