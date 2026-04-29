#include "exports.h"

void pe_register_exports_types(RDContext* ctx) {
    // clang-format off
    RDTypeDef* exportdir = rd_typedef_create_struct("IMAGE_EXPORT_DIRECTORY", ctx);
    rd_typedef_add_member(exportdir, "u32", "Characteristics", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(exportdir, "u32", "TimeDateStamp", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(exportdir, "u16", "MajorVersion", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(exportdir, "u16", "MinorVersion", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(exportdir, "u32", "Name", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(exportdir, "u32", "Base", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(exportdir, "u32", "NumberOfFunctions", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(exportdir, "u32", "NumberOfNames", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(exportdir, "u32", "AddressOfFunctions", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(exportdir, "u32", "AddressOfNames", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(exportdir, "u32", "AddressOfNameOrdinals", 0, RD_TYPE_NONE, ctx);
    rd_typedef_register(exportdir, ctx);
    // clang-format on
}

bool pe_read_exports(RDContext* ctx, PEFormat* pe) {
    ImageDataDirectory d = pe->datadir[IMAGE_DIRECTORY_ENTRY_EXPORT];

    RDAddress va;
    if(!pe_from_rva(pe, d.VirtualAddress, &va)) return false;

    RDReader* r = rd_get_reader(ctx);

    ImageExportDirectory exportdir;
    rd_reader_seek(r, va);
    rd_reader_read_le32(r, &exportdir.Characteristics);
    rd_reader_read_le32(r, &exportdir.TimeDateStamp);
    rd_reader_read_le16(r, &exportdir.MajorVersion);
    rd_reader_read_le16(r, &exportdir.MinorVersion);
    rd_reader_read_le32(r, &exportdir.Name);
    rd_reader_read_le32(r, &exportdir.Base);
    rd_reader_read_le32(r, &exportdir.NumberOfFunctions);
    rd_reader_read_le32(r, &exportdir.NumberOfNames);
    rd_reader_read_le32(r, &exportdir.AddressOfFunctions);
    rd_reader_read_le32(r, &exportdir.AddressOfNames);
    rd_reader_read_le32(r, &exportdir.AddressOfNameOrdinals);
    if(rd_reader_has_error(r)) return false;

    rd_user_type(ctx, va, "IMAGE_EXPORT_DIRECTORY", 0, RD_TYPE_NONE);

    RDAddress name_va;
    if(pe_from_rva(pe, exportdir.Name, &name_va)) {
        rd_reader_seek(r, name_va);
        usize n;
        if(rd_reader_peek_str(r, &n))
            rd_user_type(ctx, name_va, "char", n + 1, RD_TYPE_NONE);
    }

    for(u32 i = 0; i < exportdir.NumberOfNames; i++) {
        RDAddress names_va, ordinals_va;

        if(!pe_from_rva(pe, exportdir.AddressOfNames + (i * sizeof(u32)),
                        &names_va))
            continue;
        if(!pe_from_rva(pe, exportdir.AddressOfNameOrdinals + (i * sizeof(u16)),
                        &ordinals_va))
            continue;

        u32 name_rva;
        u16 ordinal_idx;

        rd_reader_seek(r, names_va);
        rd_reader_read_le32(r, &name_rva);
        if(rd_reader_has_error(r)) continue;

        rd_reader_seek(r, ordinals_va);
        rd_reader_read_le16(r, &ordinal_idx);
        if(rd_reader_has_error(r)) continue;

        RDAddress func_va, funcptrs_va;
        if(!pe_from_rva(
               pe, exportdir.AddressOfFunctions + (ordinal_idx * sizeof(u32)),
               &funcptrs_va))
            continue;

        u32 funcrva;
        rd_reader_seek(r, funcptrs_va);
        rd_reader_read_le32(r, &funcrva);
        if(rd_reader_has_error(r)) continue;

        bool is_fwd =
            funcrva >= d.VirtualAddress && funcrva < d.VirtualAddress + d.Size;
        if(is_fwd) continue; // don't handle export forwarding (yet)

        if(!pe_from_rva(pe, funcrva, &func_va)) continue;

        RDAddress exportname_va;
        if(!pe_from_rva(pe, name_rva, &exportname_va)) continue;

        rd_reader_seek(r, exportname_va);

        usize n;
        const char* name = rd_reader_peek_str(r, &n);
        if(!name) continue;

        rd_user_type(ctx, exportname_va, "char", n + 1, RD_TYPE_NONE);
        rd_set_exported(ctx, func_va, name);
        rd_library_function(ctx, func_va, name);
    }

    return true;
}
