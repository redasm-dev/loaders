#include "dex.h"
#include "dex/classes.h"
#include "dex/format.h"
#include "dex/map.h"
#include "dex/sections.h"
#include "dex/strings.h"
#include <inttypes.h>

static void _dex_type_table(RDContext* ctx, u32 off, u32 size,
                            const char* tname) {
    if(!off && !size) return; // absent section, legal

    if(!off || !size) {
        RD_LOG_WARN("'%s' has off=%" PRIu32 " size=%" PRIu32
                    " (must be both zero or both non-zero)",
                    tname, off, size);
        return;
    }

    rd_library_type(ctx, off, tname, size, RD_TYPE_NONE);
}

static void _dex_name_strings(RDContext* ctx, RDReader* r, DEXFormat* dex) {
    const DEXHeader* hdr = &dex->header;

    for(u32 i = 0; i < hdr->string_ids_size; i++) {
        u32 dataoff;
        if(!dex_get_string_offset(r, hdr, i, &dataoff)) continue;

        RDAddress text;
        usize nbytes;
        if(!dex_string_extent(r, hdr, dataoff, &text, &nbytes)) continue;

        /*
         * char[n + 1] so the type covers the terminator, which the
         * renderer's char-array path expects.
         * The uleb128 prefix stays untyped: it is neither text nor code, and a
         * detector cannot find the boundary on its own because a prefix byte is
         * very often printable.
         */
        rd_library_type(ctx, text, "char", nbytes + 1, RD_TYPE_NONE);

        // the STR_IDS entry addresses the ITEM, not the text
        rd_add_xref(ctx, hdr->string_ids_off + ((u64)i * DEX_STRING_ID_SIZE),
                    dataoff, RD_DR_ADDRESS);
    }
}

static void _dex_name_protos(RDContext* ctx, RDReader* r, DEXFormat* dex) {
    for(u32 i = 0; i < dex->header.proto_ids_size; i++) {
        const char* n = dex_proto_name(r, dex, i);

        if(!n) {
            RD_LOG_WARN("cannot build a name for proto %" PRIu32, i);
            continue;
        }

        u64 addr = dex->header.proto_ids_off + ((u64)i * DEX_PROTO_ID_SIZE);
        rd_auto_name(ctx, addr, n);
    }
}

static void _dex_name_methods(RDContext* ctx, RDReader* r, DEXFormat* dex) {
    for(u32 i = 0; i < dex->header.method_ids_size; i++) {
        const char* n = dex_method_name(r, dex, i);

        if(!n) {
            RD_LOG_WARN("cannot build a name for method %" PRIu32, i);
            continue;
        }

        u64 addr = dex->header.method_ids_off + ((u64)i * DEX_METHOD_ID_SIZE);
        rd_auto_name(ctx, addr, n);
    }
}

static void _dex_name_types(RDContext* ctx, RDReader* r, DEXFormat* dex) {
    for(u32 i = 0; i < dex->header.type_ids_size; i++) {
        const char* n = dex_type_name(r, dex, i);

        if(!n) {
            RD_LOG_WARN("cannot build a name for type %" PRIu32, i);
            continue;
        }

        u64 addr = dex->header.type_ids_off + ((u64)i * DEX_TYPE_ID_SIZE);
        rd_auto_name(ctx, addr, n);
    }
}

static void _dex_name_fields(RDContext* ctx, RDReader* r, DEXFormat* dex) {
    for(u32 i = 0; i < dex->header.field_ids_size; i++) {
        const char* n = dex_field_name(r, dex, i);
        if(!n) continue;

        u64 addr = dex->header.field_ids_off + ((u64)i * DEX_FIELD_ID_SIZE);
        rd_auto_name(ctx, addr, n);
    }
}

static bool dex_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    DEXFormat* dex = (DEXFormat*)ldr;

    dex->version = dex_read_header(req->input, &dex->header);
    if(!dex->version) return false;

    return dex_validate_header(req->input, dex);
}

static bool dex_load(RDLoader* ldr, RDContext* ctx) {
    DEXFormat* dex = (DEXFormat*)ldr;
    RDReader* r = rd_get_input_reader(ctx);

    if(!dex_validate_checksum(r, dex))
        RD_LOG_WARN("DEX checksum mismatch (file may be packed or patched)");

    if(!dex_validate_signature(r, dex))
        RD_LOG_WARN("DEX signature mismatch (file may be packed or patched)");

    DEXMap map;
    if(!dex_read_map(r, &dex->header, &map)) return false;
    if(!dex_map_segments(ctx, dex, &map)) return false;

    r = rd_get_reader(ctx);
    dex_type_sections(ctx, r, dex, &map);

    rd_kb_load(ctx, "platform/android/dex");
    rd_library_type(ctx, 0, "DEX_HEADER", 0, RD_TYPE_NONE);

    // clang-format off
    _dex_type_table(ctx, dex->header.string_ids_off, dex->header.string_ids_size, "DEX_STRING_ID");
    _dex_type_table(ctx, dex->header.type_ids_off, dex->header.type_ids_size, "DEX_TYPE_ID");
    _dex_type_table(ctx, dex->header.proto_ids_off, dex->header.proto_ids_size, "DEX_PROTO_ID");
    _dex_type_table(ctx, dex->header.field_ids_off, dex->header.field_ids_size, "DEX_FIELD_ID");
    _dex_type_table(ctx, dex->header.method_ids_off, dex->header.method_ids_size, "DEX_METHOD_ID");
    _dex_type_table(ctx, dex->header.class_defs_off, dex->header.class_defs_size, "DEX_CLASS_DEF");
    // clang-format on

    dex_walk_classes(ctx, r, dex);
    _dex_name_strings(ctx, r, dex);
    _dex_name_protos(ctx, r, dex);
    _dex_name_methods(ctx, r, dex);
    _dex_name_types(ctx, r, dex);
    _dex_name_fields(ctx, r, dex);

    return true;
}

static RDLoader* dex_create(const RDLoaderPlugin* plugin) {
    RD_UNUSED(plugin);

    DEXFormat* dex = (DEXFormat*)rd_alloc0(1, sizeof(DEXFormat));
    dex->raw_buf = rd_scratch_create();
    dex->string_buf = rd_scratch_create();
    dex->name_buf = rd_scratch_create();
    return (RDLoader*)dex;
}

static void dex_destroy(RDLoader* ldr) {
    DEXFormat* dex = (DEXFormat*)ldr;
    rd_scratch_destroy(dex->name_buf);
    rd_scratch_destroy(dex->string_buf);
    rd_scratch_destroy(dex->raw_buf);
    rd_free(dex);
}

static const char* dex_get_name(const RDLoader* ldr) {
    const DEXFormat* dex = (const DEXFormat*)ldr;
    return rd_format("Android DEX Version %03" PRIu32, dex->version);
}

static const char* dex_get_processor(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "android_dalvik";
}

static void dex_get_options(RDLoader* ldr, RDLoaderOptionBuilder* b) {
    RD_UNUSED(ldr);

    // clang-format off
    rd_loader_options_set_group(b, "Framework");
    rd_loader_options_add_bool(b, "skip_android", "Skip android.*", NULL, true);
    rd_loader_options_add_bool(b, "skip_java", "Skip java.* and javax.*", NULL, true);
 
    rd_loader_options_set_group(b, "Bundled libraries");
    rd_loader_options_add_bool(b, "skip_support", "Skip android.support.*", NULL, true);
    rd_loader_options_add_bool(b, "skip_androidx", "Skip androidx.*", NULL, true);
    rd_loader_options_add_bool(b, "skip_google", "Skip com.google.*", NULL, true);
    rd_loader_options_add_bool(b, "skip_kotlin", "Skip kotlin.*", NULL, true);
    // clang-format on
}

const RDLoaderPlugin DEX = {
    .id = "android_dex",
    .create = dex_create,
    .destroy = dex_destroy,
    .parse = dex_parse,
    .load = dex_load,
    .get_name = dex_get_name,
    .get_processor = dex_get_processor,
    .get_options = dex_get_options,
};
