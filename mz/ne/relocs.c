#include "relocs.h"
#include "common/win16_ordinals.h"
#include <redasm/redasm.h>

// Relocation source types
#define NE_RELOC_SRC_LOBYTE 0x00
#define NE_RELOC_SRC_SELECTOR 0x02
#define NE_RELOC_SRC_FARPTR 0x03
#define NE_RELOC_SRC_OFFSET 0x05

// Relocation target types (low 3 bits of FlagsAndTarget)
#define NE_RELOC_TGT_INTERNALREF 0x00
#define NE_RELOC_TGT_IMPORTORDINAL 0x01
#define NE_RELOC_TGT_IMPORTNAME 0x02
#define NE_RELOC_TGT_OSFIXUP 0x03

// Relocation flags (upper bits of FlagsAndTarget)
#define NE_RELOC_FLAG_ADDITIVE 0x04

// Patch the relocation source location in the flags buffer.
// src_type determines what kind of value lives at from_addr.
// tgt_seg and tgt_offset are in NE segment-index space (1-based).
static void _ne_patch_reloc(RDContext* ctx, u8 src_type, RDAddress from_addr,
                            u16 tgt_seg, u16 tgt_offset) {
    switch(src_type) {
        case NE_RELOC_SRC_OFFSET:
            // 16-bit offset within segment: write the flat offset word.
            // The segment base is implicit (CS/DS at runtime); we write the
            // offset portion of our synthetic flat address so the disassembler
            // sees the correct intra-segment target.
            rd_write_le16(ctx, from_addr, tgt_offset);
            break;

        case NE_RELOC_SRC_FARPTR:
            // 16:16 far pointer: offset word followed by segment word.
            // Write the real-mode paragraph of our synthetic slot as the
            // selector so far-call targets resolve in our flat space.
            rd_write_le16(ctx, from_addr, tgt_offset);
            rd_write_le16(ctx, from_addr + sizeof(u16), tgt_seg);
            break;

        case NE_RELOC_SRC_SELECTOR:
            // Segment selector only: no offset word at this location.
            rd_write_le16(ctx, from_addr, tgt_seg);
            break;

        case NE_RELOC_SRC_LOBYTE:
            // Low byte of offset: rare, used by some data references.
            rd_write_byte(ctx, from_addr, (u8)tgt_offset);
            break;

        default: break;
    }
}

void ne_load_relocs(NEFormat* ne, RDContext* ctx, u32 file_off, u16 seg_idx,
                    u16 seg_bytes, NEModuleSlice* modules) {
    RDReader* r = rd_get_input_reader(ctx);

    // Relocation table immediately follows segment data
    u32 reloc_off = file_off + seg_bytes;
    rd_reader_seek(r, reloc_off);

    u16 reloc_count;
    rd_reader_read_le16(r, &reloc_count);
    if(rd_reader_has_error(r)) return;

    RDAddress seg_base = ne_seg_address(seg_idx, 0);

    char* proc_name = NULL;
    usize proc_name_len = 0;

    for(u16 i = 0; i < reloc_count; i++) {
        // Each relocation record is exactly 8 bytes:
        // u8  src_type
        // u8  flags_and_target (low 2 bits = target type, bit 2 = additive)
        // u16 src_chain_offset (offset within this segment of the fixup site)
        // u8  tb[4]            (target, interpretation depends on target type)

        u8 src_type, flags_and_target;
        u16 src_chain_offset;
        rd_reader_read_byte(r, &src_type);
        rd_reader_read_byte(r, &flags_and_target);
        rd_reader_read_le16(r, &src_chain_offset);
        if(rd_reader_has_error(r)) return;

        u8 target_type = flags_and_target & 0x03;
        bool additive = !!(flags_and_target & NE_RELOC_FLAG_ADDITIVE);

        u8 tb[4];
        rd_reader_read_exact(r, tb, 4);
        if(rd_reader_has_error(r)) return;

        RDAddress from_addr = seg_base + src_chain_offset;

        switch(target_type) {
            case NE_RELOC_TGT_INTERNALREF: {
                // tb[0] = target segment index (1-based); 0xFF = movable
                // tb[1] = reserved (0)
                // tb[2..3] = offset within target segment
                //            (or entry table ordinal if tb[0] == 0xFF)

                u8 tgt_seg = tb[0];
                u16 tgt_offset = (u16)tb[2] | ((u16)tb[3] << 8);

                if(tgt_seg == 0xFF) {
                    // Movable segment: target is an entry table ordinal.
                    // We'd need the ordinal map here to resolve it; skip for
                    // now since movable segments are rare in practice.
                    break;
                }

                if(tgt_seg == 0 || tgt_seg > ne->header.SegCount) break;

                RDAddress to_addr = ne_seg_address(tgt_seg, tgt_offset);

                // Additive: the value at from_addr is added to the target,
                // not replaced. Read the current value and add.
                // Rare in NE.
                if(additive) {
                    u16 current = 0;
                    rd_read_le16(ctx, from_addr, &current);
                    tgt_offset += current;
                    to_addr = ne_seg_address(tgt_seg, tgt_offset);
                }

                _ne_patch_reloc(ctx, src_type, from_addr, tgt_seg, tgt_offset);
                rd_auto_xref_add(ctx, from_addr, to_addr, RD_DR_ADDRESS);

                // Seek back: IMPORTNAME case may have displaced the reader
                rd_reader_seek(r, reloc_off + 2 + ((i + 1) * 8U));
                break;
            }

            case NE_RELOC_TGT_IMPORTORDINAL: {
                // tb[0..1] = module reference table index (1-based)
                // tb[2..3] = import ordinal
                u16 mod_idx = (u16)tb[0] | ((u16)tb[1] << 8);
                u16 ordinal = (u16)tb[2] | ((u16)tb[3] << 8);
                if(!mod_idx || mod_idx > ne->header.ModRefs) break;

                const char* mod =
                    modules->length ? modules->names[mod_idx - 1] : NULL;
                if(!mod) break;

                const char* resolved_ord =
                    mz_win16_ordinal_lookup(mod, ordinal);
                const char* import_name = NULL;

                if(resolved_ord)
                    import_name = rd_format("%s.%s", mod, resolved_ord);
                else
                    import_name = rd_format("%s.%u", mod, ordinal);

                RDAddress import_addr = ne_moduleslice_resolve_import(
                    modules, ctx, mod_idx - 1, import_name);

                if(!import_addr) break;

                // import slot is in segment (SegCount + mod_idx), offset
                // within that slot is import_addr & 0xFFFF
                u16 imp_seg = ne->header.SegCount + mod_idx;
                u16 imp_off = (u16)(import_addr & 0xFFFF);
                _ne_patch_reloc(ctx, src_type, from_addr, imp_seg, imp_off);
                rd_auto_xref_add(ctx, from_addr, import_addr, RD_DR_ADDRESS);
                break;
            }

            case NE_RELOC_TGT_IMPORTNAME: {
                // tb[0..1] = module reference table index (1-based)
                // tb[2..3] = offset into imported names table (byte offset)
                u16 mod_idx = (u16)tb[0] | ((u16)tb[1] << 8);
                u16 name_off = (u16)tb[2] | ((u16)tb[3] << 8);

                if(!mod_idx || mod_idx > ne->header.ModRefs) break;
                const char* mod =
                    modules->length ? modules->names[mod_idx - 1] : NULL;
                if(!mod) break;

                u32 int_base = ne->base + ne->header.ImportNamesTableOffset;
                rd_reader_seek(r, int_base + name_off);

                u8 len;
                rd_reader_read_byte(r, &len);
                if(rd_reader_has_error(r) || !len) break;

                if(len + 1 > proc_name_len) {
                    rd_free(proc_name);
                    proc_name_len = len + 1;
                    proc_name = rd_alloc(proc_name_len);
                }

                rd_reader_read_exact(r, proc_name, len);
                proc_name[len] = 0;
                if(rd_reader_has_error(r)) break;

                const char* import_name = rd_format("%s.%s", mod, proc_name);

                RDAddress import_addr = ne_moduleslice_resolve_import(
                    modules, ctx, mod_idx - 1, import_name);
                if(!import_addr) break;

                u16 imp_seg = ne->header.SegCount + mod_idx;
                u16 imp_off = (u16)(import_addr & 0xFFFF);
                _ne_patch_reloc(ctx, src_type, from_addr, imp_seg, imp_off);
                rd_auto_xref_add(ctx, from_addr, import_addr, RD_DR_ADDRESS);

                // Restore reader position to the next reloc record
                rd_reader_seek(r, reloc_off + 2 + ((i + 1) * 8U));
                break;
            }

            // OS-specific fixups (floating point emulation etc.), ignore
            case NE_RELOC_TGT_OSFIXUP:
            default: break;
        }
    }

    rd_free(proc_name);
}
