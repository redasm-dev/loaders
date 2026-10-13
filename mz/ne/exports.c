#include "exports.h"

void ne_load_exports(NEFormat* ne, RDContext* ctx,
                     const NEEntrySlice* entries) {
    if(!entries->length) return;

    RDReader* r = rd_get_input_reader(ctx);
    u32 rnt_off = ne->base + ne->header.ResidNamesTableOffset;
    bool first = true;

    rd_reader_seek(r, rnt_off);

    char* name = NULL;
    usize name_len = 0;

    while(true) {
        u8 len;
        rd_reader_read_byte(r, &len);
        if(rd_reader_has_error(r) || len == 0) break;

        if(len + 1 > name_len) {
            rd_free(name);
            name_len = len + 1;
            name = rd_alloc(name_len);
        }

        rd_reader_read_exact(r, name, len);
        name[len] = 0;

        u16 ordinal;
        rd_reader_read_le16(r, &ordinal);
        if(rd_reader_has_error(r)) break;

        if(first) { // first entry is the module name, not an export
            first = false;
            continue;
        }

        if(!ordinal || ordinal > entries->length) continue;

        RDAddress addr = entries->data[ordinal - 1];
        if(!addr) continue; // ordinal has no entry table address

        const RDSegment* seg = rd_segment_find(ctx, addr);
        if(!seg) continue;

        if(rd_segment_get_perm(seg) & RD_SP_X) rd_set_function(ctx, addr);

        rd_library_name(ctx, addr, name);
        rd_set_external(ctx, addr, NULL, RD_EXT_EXPORTED);
    }

    rd_free(name);
}
