#include "sna_format.h"
#include "common.h"

bool sna_read_header(RDReader* r, SNAHeader* v) {
    rd_reader_read_byte(r, &v->i);
    rd_reader_read_le16(r, &v->hl_);
    rd_reader_read_le16(r, &v->de_);
    rd_reader_read_le16(r, &v->bc_);
    rd_reader_read_le16(r, &v->af_);
    rd_reader_read_le16(r, &v->hl);
    rd_reader_read_le16(r, &v->de);
    rd_reader_read_le16(r, &v->bc);
    rd_reader_read_le16(r, &v->ix);
    rd_reader_read_le16(r, &v->iy);
    rd_reader_read_byte(r, &v->int_flag);
    rd_reader_read_byte(r, &v->r);
    rd_reader_read_le16(r, &v->af);
    rd_reader_read_le16(r, &v->sp);
    rd_reader_read_byte(r, &v->int_mode);
    rd_reader_read_byte(r, &v->border_color);
    return !rd_reader_has_error(r);
}

void sna_init_registers(RDContext* ctx, const SNAFormat* sna) {
    rd_set_regval(ctx, "i", sna->header.i);
    rd_set_regval(ctx, "hl'", sna->header.hl_);
    rd_set_regval(ctx, "de'", sna->header.de_);
    rd_set_regval(ctx, "bc'", sna->header.bc_);
    rd_set_regval(ctx, "af'", sna->header.af_);
    rd_set_regval(ctx, "hl", sna->header.hl);
    rd_set_regval(ctx, "de", sna->header.de);
    rd_set_regval(ctx, "bc", sna->header.bc);
    rd_set_regval(ctx, "iy", sna->header.iy);
    rd_set_regval(ctx, "ix", sna->header.ix);
    rd_set_regval(ctx, "r", sna->header.r);
    rd_set_regval(ctx, "af", sna->header.af);
    rd_set_regval(ctx, "sp", sna->header.sp);
}

bool sna_load_48k(RDContext* ctx, const SNAFormat* sna) {
    rd_map_segment(ctx, "ROM", 0x0000, 0x4000, RD_SP_R);
    rd_map_segment(ctx, "RAM", 0x4000, 0x10000, RD_SP_RWX);
    rd_map_input_n(ctx, sna->ram_start, 0x4000, sna->ram_length);
    rd_library_name(ctx, sna->header.sp, "stack");

    u16 ep;
    if(rd_read_le16(ctx, sna->header.sp, &ep)) zx_set_entry_point(ctx, ep);

    rd_kb_load(ctx, "platform/zxspectrum/rom48k");
    return true;
}
