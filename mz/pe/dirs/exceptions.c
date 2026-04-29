#include "exceptions.h"
#include <inttypes.h>

void pe_register_exceptions_types(RDContext* ctx) {
    // clang-format off
    RDTypeDef* rfe = rd_typedef_create_struct("IMAGE_RUNTIME_FUNCTION_ENTRY", ctx);
    rd_typedef_add_member(rfe, "u32", "BeginAddress",    0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(rfe, "u32", "EndAddress",      0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(rfe, "u32", "UnwindInfoAddress", 0, RD_TYPE_NONE, ctx);
    rd_typedef_register(rfe, ctx);
    // clang-format on
}

bool pe_read_exceptions(RDContext* ctx, PEFormat* pe) {
    // exception directory is only present on x64 (and Itanium)
    if(pe_get_bits(pe) != 64) return false;

    ImageDataDirectory d = pe->datadir[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if(!d.VirtualAddress || !d.Size) return false;

    RDAddress va;
    if(!pe_from_rva(pe, d.VirtualAddress, &va)) return false;

    usize entrysize = rd_size_of(ctx, "IMAGE_RUNTIME_FUNCTION_ENTRY", 0);
    usize n = d.Size / entrysize;
    RDReader* r = rd_get_reader(ctx);

    for(usize i = 0; i < n; i++) {
        RDAddress entry_va = va + (i * entrysize);
        rd_reader_seek(r, entry_va);

        ImageRuntimeFunctionEntry entry;
        rd_reader_read_le32(r, &entry.BeginAddress);
        rd_reader_read_le32(r, &entry.EndAddress);
        rd_reader_read_le32(r, &entry.UnwindInfoAddress);

        if(rd_reader_has_error(r)) break;
        if(!entry.BeginAddress) continue;

        rd_user_type(ctx, entry_va, "IMAGE_RUNTIME_FUNCTION_ENTRY", 0,
                     RD_TYPE_NONE);

        RDAddress func_va;
        if(!pe_from_rva(pe, entry.BeginAddress, &func_va)) continue;

        rd_library_function(ctx, func_va, rd_format("exc_%" PRIx64, func_va));
    }

    return true;
}
