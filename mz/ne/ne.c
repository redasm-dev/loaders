#include "ne.h"
#include "hooks.h"
#include "ne/entries.h"
#include "ne/exports.h"
#include "ne/format.h"
#include "ne/modules.h"
#include "ne/relocs.h"

static void _ne_set_entry_point(NEFormat* ne, RDContext* ctx) {
    const NEHeader* hdr = &ne->header;

    u16 cs_idx = (u16)(hdr->EntryPoint >> 16);
    u16 ip = (u16)(hdr->EntryPoint & 0xFFFF);

    if(!cs_idx || cs_idx > hdr->SegCount) return;

    RDAddress entry = ne_seg_address(cs_idx, ip);

    bool is_dll = !!(hdr->AppFlags & NE_APPFLAG_DLL);
    rd_set_entry_point(ctx, entry, is_dll ? "DllInit" : NULL);

    // Seed SS:SP for stack analysis
    u16 ss_idx = (u16)(hdr->InitStack >> 16);
    u16 sp = (u16)(hdr->InitStack & 0xFFFF);

    if(ss_idx && ss_idx <= hdr->SegCount) {
        RDAddress ss_base = ne_seg_address(ss_idx, 0);
        // SS holds the segment selector, which in our flat scheme is the
        // paragraph number of the slot base (base >> 4).
        rd_library_sregval(ctx, entry, "ss", ss_base >> 4);
        rd_set_regval(ctx, "sp", sp);
    }

    if(hdr->AutoDataSegIndex && hdr->AutoDataSegIndex <= hdr->SegCount) {
        RDAddress ds_base = ne_seg_address(hdr->AutoDataSegIndex, 0);
        rd_library_sregval(ctx, entry, "ds", ds_base >> 4);
        rd_library_sregval(ctx, entry, "es",
                           ds_base >> 4); // ES = DS at startup
    }

    // Seed CS so the processor knows which segment we start in
    RDAddress cs_base = ne_seg_address(cs_idx, 0);
    rd_library_sregval(ctx, entry, "cs", cs_base >> 4);
}

static bool ne_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    NEFormat* ne = (NEFormat*)ldr;
    if(!mz_read_dos_header(req->input, &ne->dosheader)) return false;

    if(!mz_match_signature(req->input, &ne->dosheader, MZ_NE_SIGNATURE))
        return false;

    ne->base = ne->dosheader.e_lfanew;
    return ne_read_header(req->input, &ne->header);
}

static bool ne_load(RDLoader* ldr, RDContext* ctx) {
    mz_register_dos_hooks(ctx);

    NEFormat* ne = (NEFormat*)ldr;
    if(!ne->header.SegCount) return false;

    // 1. Map all segments into the synthetic flat address space
    if(!ne_load_segments(ne, ctx)) return false;

    // 2. Build ordinal => address map from the entry table
    NEEntrySlice entries = ne_entryslice_create(ne, ctx);

    // 3. Register exports from the resident names table
    ne_load_exports(ne, ctx, &entries);

    // 4. Build module name table and allocate synthetic import segments
    NEModuleSlice modules = ne_moduleslice_create(ne, ctx);
    ne_moduleslice_build_imports(&modules, ne, ctx);

    // 5. Process per-segment relocations (xrefs + import markers)
    RDReader* r = rd_get_input_reader(ctx);
    u32 segtab_off = ne->base + ne->header.SegTableOffset;
    u32 sector_size = 1U << ne->header.FileAlnSzShftCnt;

    for(u16 i = 0; i < ne->header.SegCount; i++) {
        rd_reader_seek(r, segtab_off + (i * sizeof(u16) * 4));

        u16 sector_base, seg_bytes, seg_flags, min_alloc;
        rd_reader_read_le16(r, &sector_base);
        rd_reader_read_le16(r, &seg_bytes);
        rd_reader_read_le16(r, &seg_flags);
        rd_reader_read_le16(r, &min_alloc);
        if(rd_reader_has_error(r)) break;

        if(!sector_base) continue; // no file data
        if(!(seg_flags & NE_SEGFLAG_HAS_RELOCS)) continue;

        // seg_bytes == 0 means 64KB on disk.
        // if a 64KB segment has relocs they follow at
        // sector_base * sector_size + 0x10000.
        u16 file_seg_bytes = seg_bytes ? seg_bytes : 0;
        u32 file_off = (u32)sector_base * sector_size;
        ne_load_relocs(ne, ctx, file_off, i + 1, file_seg_bytes, &modules);
    }

    // 6. Set entry point and seed segment registers
    _ne_set_entry_point(ne, ctx);

    ne_moduleslice_destroy(&modules);
    ne_entryslice_destroy(&entries);
    return true;
}

static const char* ne_get_name(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "New Executable";
}

static const char* ne_get_processor(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "x86_16";
}

const RDLoaderPlugin NE_LOADER = {
    .id = "dos_ne",
    .instance_size = sizeof(NEFormat),
    .get_name = ne_get_name,
    .get_processor = ne_get_processor,
    .parse = ne_parse,
    .load = ne_load,
};
