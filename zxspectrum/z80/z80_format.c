#include "z80_format.h"
#include "common.h"

#define Z80_FLAGS_COMPRESSED (1 << 5)

static const u8 Z80_48K_REQUIRED_PAGES[] = {4, 5, 8};

static bool _z80_48k_page_address(u8 page, RDAddress* addr) {
    switch(page) {
        case 0: *addr = 0x0000; break; // real ROM content, if present
        case 4: *addr = 0x8000; break;
        case 5: *addr = 0xC000; break;
        case 8: *addr = 0x4000; break;
        default: return false;
    }

    return true;
}

static bool _z80_try_expand_run(RDReader* r, RDScratchBuffer* sb, u8 b0,
                                bool* out_ok) {
    if(b0 != 0xED) return false;

    u64 checkpoint = rd_reader_tell(r);
    u8 b1;

    if(rd_reader_read_byte(r, &b1) && b1 == 0xED) {
        u8 count, value;
        if(!rd_reader_read_byte(r, &count) || !rd_reader_read_byte(r, &value)) {
            *out_ok = false;
            return true;
        }

        for(u8 i = 0; i < count; i++)
            rd_scratch_push(sb, value);

        *out_ok = true;
        return true;
    }

    rd_reader_seek(r, checkpoint); // not a run: put b1 (if read) back
    return false;
}

static RDScratchBuffer* _z80_decompress_v1(RDReader* r, usize expected_len) {
    RDScratchBuffer* sb = rd_scratch_create();
    rd_scratch_reserve(sb, expected_len);

    while(rd_scratch_length(sb) < expected_len) {
        u8 b0;
        if(!rd_reader_read_byte(r, &b0)) goto fail;

        if(b0 == 0x00) {
            u64 checkpoint = rd_reader_tell(r);
            u8 b1, b2, b3;

            if(rd_reader_read_byte(r, &b1) && b1 == 0xED &&
               rd_reader_read_byte(r, &b2) && b2 == 0xED &&
               rd_reader_read_byte(r, &b3) && b3 == 0x00) {
                break;
            }

            rd_reader_seek(r, checkpoint);
            rd_scratch_push(sb, (char)b0);
            continue;
        }

        bool ok;
        if(_z80_try_expand_run(r, sb, b0, &ok)) {
            if(!ok) goto fail;
            continue;
        }

        rd_scratch_push(sb, (char)b0);
    }

    if(rd_scratch_length(sb) != expected_len) goto fail;
    return sb;

fail:
    rd_scratch_destroy(sb);
    return NULL;
}

static RDScratchBuffer* _z80_decompress_block(RDReader* r, usize compressed_len,
                                              usize expected_out_len) {
    u64 start = rd_reader_tell(r);
    RDScratchBuffer* sb = rd_scratch_create();
    rd_scratch_reserve(sb, expected_out_len);

    while(rd_reader_tell(r) - start < compressed_len) {
        u8 b0;
        if(!rd_reader_read_byte(r, &b0)) goto fail;

        bool ok;
        if(_z80_try_expand_run(r, sb, b0, &ok)) {
            if(!ok) goto fail;
            continue;
        }

        rd_scratch_push(sb, (char)b0);
    }

    if(rd_scratch_length(sb) != expected_out_len) goto fail;
    return sb;

fail:
    rd_scratch_destroy(sb);
    return NULL;
}

void z80_init_registers(RDContext* ctx, const Z80Format* z80) {
    rd_set_regval(ctx, "a", z80->hdr_base.a);
    rd_set_regval(ctx, "f", z80->hdr_base.f);
    rd_set_regval(ctx, "bc", z80->hdr_base.bc);
    rd_set_regval(ctx, "hl", z80->hdr_base.hl);
    rd_set_regval(ctx, "sp", z80->hdr_base.sp);
    rd_set_regval(ctx, "i", z80->hdr_base.i);

    rd_set_regval(ctx, "r",
                  (z80->hdr_base.r & 0x7F) | ((z80->hdr_base.flags & 1) << 7));

    rd_set_regval(ctx, "de", z80->hdr_base.de);
    rd_set_regval(ctx, "bc'", z80->hdr_base.bc_);
    rd_set_regval(ctx, "de'", z80->hdr_base.de_);
    rd_set_regval(ctx, "hl'", z80->hdr_base.hl_);
    rd_set_regval(ctx, "a'", z80->hdr_base.a_);
    rd_set_regval(ctx, "f'", z80->hdr_base.f_);
    rd_set_regval(ctx, "iy", z80->hdr_base.iy);
    rd_set_regval(ctx, "ix", z80->hdr_base.ix);
}

bool z80_read_header_base(RDReader* r, Z80HeaderBase* v) {
    rd_reader_read_byte(r, &v->a);
    rd_reader_read_byte(r, &v->f);
    rd_reader_read_le16(r, &v->bc);
    rd_reader_read_le16(r, &v->hl);
    rd_reader_read_le16(r, &v->pc);
    rd_reader_read_le16(r, &v->sp);
    rd_reader_read_byte(r, &v->i);
    rd_reader_read_byte(r, &v->r);

    if(rd_reader_read_byte(r, &v->flags) && v->flags == 255) v->flags = 1;

    rd_reader_read_le16(r, &v->de);
    rd_reader_read_le16(r, &v->bc_);
    rd_reader_read_le16(r, &v->de_);
    rd_reader_read_le16(r, &v->hl_);
    rd_reader_read_byte(r, &v->a_);
    rd_reader_read_byte(r, &v->f_);
    rd_reader_read_le16(r, &v->iy);
    rd_reader_read_le16(r, &v->ix);
    rd_reader_read_byte(r, &v->iff1);
    rd_reader_read_byte(r, &v->iff2);
    rd_reader_read_byte(r, &v->im_flags);

    return !rd_reader_has_error(r);
}

bool z80_read_header_ext(RDReader* r, Z80HeaderExt* v) {
    rd_reader_read_le16(r, &v->length);
    rd_reader_read_le16(r, &v->pc);
    rd_reader_read_byte(r, &v->hw_mode);
    rd_reader_read_byte(r, &v->state);

    rd_reader_read_byte(r, &v->rom_paged);
    rd_reader_read_byte(r, &v->flags);
    rd_reader_read_byte(r, &v->last_out_fffd);
    rd_reader_read_exact(r, &v->audio_regs, sizeof(v->audio_regs));

    if(v->length == Z80_HEADER_LEN_V3) {
        rd_reader_read_le16(r, &v->v3.t_state_l);
        rd_reader_read_byte(r, &v->v3.t_state_h);
        rd_reader_read_byte(r, &v->v3.flag_byte_spec);
        rd_reader_read_byte(r, &v->v3.mgt_rom_paged);
        rd_reader_read_byte(r, &v->v3.multface_rom_paged);
        rd_reader_read_byte(r, &v->v3.is_rom_l);
        rd_reader_read_byte(r, &v->v3.is_rom_h);
        rd_reader_read_exact(r, &v->v3.joy_mapping, sizeof(v->v3.joy_mapping));
        rd_reader_read_exact(r, &v->v3.kbd_mapping, sizeof(v->v3.kbd_mapping));
        rd_reader_read_byte(r, &v->v3.mgt_type);
        rd_reader_read_byte(r, &v->v3.disciple_button_state);
        rd_reader_read_byte(r, &v->v3.disciple_flags);
    }

    if(v->length == Z80_HEADER_LEN_V3_EXT)
        rd_reader_read_byte(r, &v->v3_ext.out_1ffd);

    return !rd_reader_has_error(r);
}

bool z80_load_v1(RDContext* ctx, RDReader* r, Z80Format* z80) {
    rd_reader_seek(r, z80->body_start);
    RDScratchBuffer* buf = _z80_decompress_v1(r, 48ULL * 1024);
    if(!buf) return false;

    rd_map_segment(ctx, "ROM", 0x0000, 0x4000, RD_SP_R);
    rd_map_segment(ctx, "RAM", 0x4000, 0x10000, RD_SP_RWX);
    rd_write(ctx, 0x4000, rd_scratch_data(buf), rd_scratch_length(buf));

    zx_set_entry_point(ctx, z80->hdr_base.pc);
    rd_kb_load(ctx, "platform/zxspectrum/rom48k");

    rd_scratch_destroy(buf);
    return true;
}

bool z80_load_v2_v3(RDContext* ctx, RDReader* r, Z80Format* z80) {
    rd_map_segment(ctx, "ROM", 0x0000, 0x4000, RD_SP_R);
    rd_map_segment(ctx, "RAM", 0x4000, 0x10000, RD_SP_RWX);

    rd_reader_seek(r, z80->body_start);

    bool page_seen[9] = {0};

    while(!rd_reader_at_end(r)) {
        u16 length;
        if(!rd_reader_read_le16(r, &length)) break;

        u8 page;
        if(!rd_reader_read_byte(r, &page)) {
            RD_LOG_WARN("truncated block header at end of file");
            break;
        }

        RDAddress addr;
        if(!_z80_48k_page_address(page, &addr)) {
            RD_LOG_WARN("unexpected page number %d (not valid for 48k mode), "
                        "skipping block",
                        page);

            usize skip = (length == 0xFFFF) ? 16384 : length;
            rd_reader_seek(r, rd_reader_tell(r) + skip);
            continue;
        }

        if(page <= 8) page_seen[page] = true;

        if(length == 0xFFFF) {
            RDScratchBuffer* raw = rd_scratch_create();
            rd_scratch_reserve(raw, 16384);

            for(usize i = 0; i < 16384; i++) {
                u8 b;

                if(!rd_reader_read_byte(r, &b)) {
                    rd_scratch_destroy(raw);
                    return false;
                }

                rd_scratch_push(raw, b);
            }

            rd_write(ctx, addr, rd_scratch_data(raw), rd_scratch_length(raw));
            rd_scratch_destroy(raw);
        }
        else {
            RDScratchBuffer* buf = _z80_decompress_block(r, length, 16384);
            if(!buf) return false;

            rd_write(ctx, addr, rd_scratch_data(buf), rd_scratch_length(buf));
            rd_scratch_destroy(buf);
        }
    }

    for(usize i = 0; i < rd_count_of(Z80_48K_REQUIRED_PAGES); i++) {
        u8 p = Z80_48K_REQUIRED_PAGES[i];
        if(!page_seen[p])
            RD_LOG_WARN("required page %d never appeared -- file may be "
                        "incomplete",
                        p);
    }

    zx_set_entry_point(ctx, z80->hdr_ext.pc);
    rd_kb_load(ctx, "platform/zxspectrum/rom48k");

    return true;
}
