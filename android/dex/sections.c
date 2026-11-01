#include "sections.h"
#include <inttypes.h>

static void _dex_type_u32_table(RDContext* ctx, const DEXMap* map, u16 type) {
    const DEXMapItem* it = dex_map_find(map, type);
    if(!it || !it->size) return;

    rd_library_type(ctx, it->offset, "u32", it->size, RD_TYPE_NONE);
}

static void _dex_type_struct_table(RDContext* ctx, const DEXMap* map, u16 type,
                                   const char* tname) {
    const DEXMapItem* it = dex_map_find(map, type);
    if(!it || !it->size) return;

    rd_library_type(ctx, it->offset, tname, it->size, RD_TYPE_NONE);
}

static void _dex_xref_call_sites(RDContext* ctx, RDReader* r,
                                 const DEXFormat* dex, const DEXMap* map) {
    const DEXMapItem* it = dex_map_find(map, DEX_TYPE_CALL_SITE_ID_ITEM);
    if(!it || !it->size) return;

    for(u32 i = 0; i < it->size; i++) {
        u64 addr = it->offset + ((u64)i * sizeof(u32));

        rd_reader_save(r);
        rd_reader_seek(r, addr);

        u32 dataoff;
        bool ok = rd_reader_read_le32(r, &dataoff);
        rd_reader_restore(r);

        if(!ok || dataoff >= dex->header.file_size) continue;
        rd_auto_xref_add(ctx, (RDAddress)addr, dataoff, RD_DR_ADDRESS);
    }
}

static void _dex_type_type_lists(RDContext* ctx, RDReader* r,
                                 const DEXFormat* dex, const DEXMap* map) {
    const DEXMapItem* it = dex_map_find(map, DEX_TYPE_TYPE_LIST);
    if(!it || !it->size) return;

    u64 p = it->offset;

    for(u32 i = 0; i < it->size; i++) {
        if(p + sizeof(u32) > dex->header.file_size) break;

        rd_reader_save(r);
        rd_reader_seek(r, p);

        u32 count;
        bool ok = rd_reader_read_le32(r, &count);
        rd_reader_restore(r);

        if(!ok) break;

        u64 end = p + sizeof(u32) + ((u64)count * sizeof(u16));

        if(!count || end > dex->header.file_size) {
            RD_LOG_WARN("DEX: malformed type_list @ %" PRIx64, p);
            break;
        }

        rd_library_type(ctx, p, "u32", 0, RD_TYPE_NONE); // the count
        rd_library_type(ctx, p + sizeof(u32), "u16", count, RD_TYPE_NONE);

        p = rd_align_up(end, sizeof(u32));
    }
}

static void _dex_type_offset_lists(RDContext* ctx, RDReader* r,
                                   const DEXFormat* dex, const DEXMap* map,
                                   u16 type) {
    const DEXMapItem* it = dex_map_find(map, type);
    if(!it || !it->size) return;

    u64 p = it->offset;

    for(u32 i = 0; i < it->size; i++) {
        if(p + sizeof(u32) > dex->header.file_size) break;

        rd_reader_save(r);
        rd_reader_seek(r, p);

        u32 count;
        bool ok = rd_reader_read_le32(r, &count);
        rd_reader_restore(r);

        if(!ok) break;

        u64 end = p + sizeof(u32) + ((u64)count * sizeof(u32));

        if(end > dex->header.file_size) {
            RD_LOG_WARN("DEX: malformed offset list @ %" PRIx64, p);
            break;
        }

        rd_library_type(ctx, p, "u32", 0, RD_TYPE_NONE); // the count

        // an empty set is legal: a count of zero and nothing after it
        if(count)
            rd_library_type(ctx, p + sizeof(u32), "u32", count, RD_TYPE_NONE);

        p = rd_align_up(end, sizeof(u32));
    }
}

void dex_type_sections(RDContext* ctx, RDReader* r, const DEXFormat* dex,
                       const DEXMap* map) {
    // fixed stride
    _dex_type_u32_table(ctx, map, DEX_TYPE_CALL_SITE_ID_ITEM);
    _dex_type_struct_table(ctx, map, DEX_TYPE_METHOD_HANDLE_ITEM,
                           "DEX_METHOD_HANDLE");
    _dex_xref_call_sites(ctx, r, dex, map);

    // length-prefixed
    _dex_type_type_lists(ctx, r, dex, map);
    _dex_type_offset_lists(ctx, r, dex, map, DEX_TYPE_ANNOTATION_SET_ITEM);
    _dex_type_offset_lists(ctx, r, dex, map, DEX_TYPE_ANNOTATION_SET_REF_LIST);
}
