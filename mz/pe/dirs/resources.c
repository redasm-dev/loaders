#include "resources.h"
#include <inttypes.h>

#define PE_RESOURCE_NAME_IS_STRING 0x80000000
#define PE_RESOURCE_DATA_IS_DIRECTORY 0x80000000

static bool _pe_resource_read_dir(RDReader* r, PEResourceDirectory* dir) {
    rd_reader_read_le32(r, &dir->Characteristics);
    rd_reader_read_le32(r, &dir->TimeDateStamp);
    rd_reader_read_le16(r, &dir->MajorVersion);
    rd_reader_read_le16(r, &dir->MinorVersion);
    rd_reader_read_le16(r, &dir->NumberOfNamedEntries);
    rd_reader_read_le16(r, &dir->NumberOfIdEntries);
    return !rd_reader_has_error(r);
}

static bool _pe_resource_read_dir_entry(RDReader* r,
                                        PEResourceDirectoryEntry* entry) {
    rd_reader_read_le32(r, &entry->NameOffset);
    rd_reader_read_le32(r, &entry->OffsetToData);
    return !rd_reader_has_error(r);
}

static bool _pe_resource_read_data_entry(RDReader* r,
                                         PEResourceDataEntry* dataentry) {
    rd_reader_read_le32(r, &dataentry->OffsetToData);
    rd_reader_read_le32(r, &dataentry->Size);
    rd_reader_read_le32(r, &dataentry->CodePage);
    rd_reader_read_le32(r, &dataentry->Reserved);
    return !rd_reader_has_error(r);
}

static void _pe_resource_read_dirs(RDContext* ctx, PEFormat* pe, RDReader* r,
                                   RDAddress base, RDAddress va, int depth) {
    if(depth > 3) return; // max 3 levels deep in PE resources

    PEResourceDirectory resdir;
    rd_reader_seek(r, va);
    if(!_pe_resource_read_dir(r, &resdir)) return;

    rd_library_type(ctx, va, "PE_RESOURCE_DIRECTORY", 0, RD_TYPE_NONE);

    u16 total = resdir.NumberOfNamedEntries + resdir.NumberOfIdEntries;
    RDAddress entry_va = rd_reader_tell(r);

    for(u16 i = 0; i < total; i++) {
        PEResourceDirectoryEntry entry;
        if(!_pe_resource_read_dir_entry(r, &entry)) break;

        rd_reader_save(r);
        rd_library_type(ctx, entry_va, "PE_RESOURCE_DIRECTORY_ENTRY", 0,
                        RD_TYPE_NONE);

        if(entry.OffsetToData & PE_RESOURCE_DATA_IS_DIRECTORY) {
            RDAddress subdir_va =
                base + (entry.OffsetToData & ~PE_RESOURCE_DATA_IS_DIRECTORY);
            _pe_resource_read_dirs(ctx, pe, r, base, subdir_va, depth + 1);
        }
        else {
            RDAddress dataentry_va = base + entry.OffsetToData;
            rd_library_type(ctx, dataentry_va, "PE_RESOURCE_DATA_ENTRY", 0,
                            RD_TYPE_NONE);

            rd_reader_seek(r, dataentry_va);
            PEResourceDataEntry dataentry;
            if(!_pe_resource_read_data_entry(r, &dataentry)) {
                rd_reader_restore(r);
                break;
            }

            RDAddress data_va;

            if(pe_from_rva(pe, dataentry.OffsetToData, &data_va) &&
               dataentry.Size) {
                rd_auto_name(ctx, data_va,
                             rd_format("rsrc_data_%" PRIX64, data_va));
            }
        }

        entry_va = rd_reader_restore(r);
    }
}

bool pe_read_resources_dir(RDContext* ctx, PEFormat* pe) {
    PEDataDirectory d = pe->data_dirs[PE_DIRECTORY_ENTRY_RESOURCE];

    RDAddress va;
    if(!pe_from_rva(pe, d.VirtualAddress, &va)) return false;

    RDReader* r = rd_get_reader(ctx);
    _pe_resource_read_dirs(ctx, pe, r, va, va, 0);
    return true;
}
