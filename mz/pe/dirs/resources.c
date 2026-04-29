#include "resources.h"

static void _pe_read_resource_dir(RDContext* ctx, PEFormat* pe, RDReader* r,
                                  RDAddress base, RDAddress va, int depth) {
    if(depth > 3) return; // max 3 levels deep in PE resources

    ImageResourceDirectory resdir;
    rd_reader_seek(r, va);
    rd_reader_read_le32(r, &resdir.Characteristics);
    rd_reader_read_le32(r, &resdir.TimeDateStamp);
    rd_reader_read_le16(r, &resdir.MajorVersion);
    rd_reader_read_le16(r, &resdir.MinorVersion);
    rd_reader_read_le16(r, &resdir.NumberOfNamedEntries);
    rd_reader_read_le16(r, &resdir.NumberOfIdEntries);
    if(rd_reader_has_error(r)) return;

    rd_user_type(ctx, va, "IMAGE_RESOURCE_DIRECTORY", 0, RD_TYPE_NONE);

    u16 total = resdir.NumberOfNamedEntries + resdir.NumberOfIdEntries;
    RDAddress entry_va = va + rd_size_of(ctx, "IMAGE_RESOURCE_DIRECTORY", 0);

    for(u16 i = 0; i < total; i++) {
        ImageResourceDirectoryEntry entry;
        rd_reader_seek(r, entry_va);
        rd_reader_read_le32(r, &entry.NameOffset);
        rd_reader_read_le32(r, &entry.OffsetToData);
        if(rd_reader_has_error(r)) break;

        rd_user_type(ctx, entry_va, "IMAGE_RESOURCE_DIRECTORY_ENTRY", 0,
                     RD_TYPE_NONE);

        if(entry.OffsetToData & IMAGE_RESOURCE_DATA_IS_DIRECTORY) {
            RDAddress subdir_va =
                base + (entry.OffsetToData & ~IMAGE_RESOURCE_DATA_IS_DIRECTORY);
            _pe_read_resource_dir(ctx, pe, r, base, subdir_va, depth + 1);
        }
        else {
            RDAddress dataentry_va = base + entry.OffsetToData;
            rd_user_type(ctx, dataentry_va, "IMAGE_RESOURCE_DATA_ENTRY", 0,
                         RD_TYPE_NONE);

            ImageResourceDataEntry dataentry;
            rd_reader_seek(r, dataentry_va);
            rd_reader_read_le32(r, &dataentry.OffsetToData);
            rd_reader_read_le32(r, &dataentry.Size);
            rd_reader_read_le32(r, &dataentry.CodePage);
            rd_reader_read_le32(r, &dataentry.Reserved);
            if(rd_reader_has_error(r)) break;

            RDAddress data_va;

            if(pe_from_rva(pe, dataentry.OffsetToData, &data_va) &&
               dataentry.Size) {
                rd_library_name(ctx, data_va,
                                rd_format("rsrc_data_%x", data_va));
            }
        }

        entry_va += rd_size_of(ctx, "IMAGE_RESOURCE_DIRECTORY_ENTRY", 0);
    }
}

void pe_register_resources_types(RDContext* ctx) {
    // clang-format off
    RDTypeDef* resdir = rd_typedef_create_struct("IMAGE_RESOURCE_DIRECTORY", ctx);
    rd_typedef_add_member(resdir, "u32", "Characteristics", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(resdir, "u32", "TimeDateStamp", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(resdir, "u16", "MajorVersion", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(resdir, "u16", "MinorVersion", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(resdir, "u16", "NumberOfNamedEntries", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(resdir, "u16", "NumberOfIdEntries", 0, RD_TYPE_NONE, ctx);
    rd_typedef_register(resdir, ctx);

    RDTypeDef* entry = rd_typedef_create_struct("IMAGE_RESOURCE_DIRECTORY_ENTRY", ctx);
    rd_typedef_add_member(entry, "u32", "NameOffset", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(entry, "u32", "OffsetToData", 0, RD_TYPE_NONE, ctx);
    rd_typedef_register(entry, ctx);

    RDTypeDef* dataentry = rd_typedef_create_struct("IMAGE_RESOURCE_DATA_ENTRY", ctx);
    rd_typedef_add_member(dataentry, "u32", "OffsetToData", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(dataentry, "u32", "Size", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(dataentry, "u32", "CodePage", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(dataentry, "u32", "Reserved", 0, RD_TYPE_NONE, ctx);
    rd_typedef_register(dataentry, ctx);
    // clang-format on
}

bool pe_read_resources(RDContext* ctx, PEFormat* pe) {
    ImageDataDirectory d = pe->datadir[IMAGE_DIRECTORY_ENTRY_RESOURCE];

    RDAddress va;
    if(!pe_from_rva(pe, d.VirtualAddress, &va)) return false;

    rd_user_type(ctx, va, "IMAGE_RESOURCE_DIRECTORY", 0, RD_TYPE_NONE);

    RDReader* r = rd_get_reader(ctx);
    _pe_read_resource_dir(ctx, pe, r, va, va, 0);
    return true;
}
