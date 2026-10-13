#include "exports.h"
#include <string.h>

/*
 * GNU ld emits these linker-defined boundary symbols in the export table
 * when targeting PE (MinGW). They are address markers, not functions.
 * - _etext marks the end of .text
 * - _edata the end of initialized data
 * - _end the end of BSS
 *
 * The executable permission check alone is not enough because
 * _etext lands inside the executable segment it is bounding.
 */
static bool _pe_is_linker_boundary(const char* name) {
    if(!name) return false;

    static const char* const BOUNDARIES[] = {
        "_etext",
        "_edata",
        "_end",
        "__bss_start",
        "__fini_array_start",
        "__fini_array_end",
        "__init_array_start",
        "__init_array_end",
        "__preinit_array_start",
        "__preinit_array_end",
        NULL,
    };

    for(int i = 0; BOUNDARIES[i]; i++) {
        if(strcmp(name, BOUNDARIES[i]) == 0) return true;
    }

    return false;
}

bool pe_read_exports_dir(RDContext* ctx, PEFormat* pe) {
    PEDataDirectory d = pe->data_dirs[PE_DIRECTORY_ENTRY_EXPORT];

    RDAddress va;
    if(!pe_from_rva(pe, d.VirtualAddress, &va)) return false;

    RDReader* r = rd_get_reader(ctx);

    PEExportDirectory exportdir;
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

    rd_library_type(ctx, va, "PE_EXPORT_DIRECTORY", 0, RD_TYPE_NONE);

    RDAddress name_va;
    if(pe_from_rva(pe, exportdir.Name, &name_va)) {
        rd_reader_seek(r, name_va);
        usize n;
        if(rd_reader_peek_str(r, &n)) {
            rd_library_type(ctx, name_va, "char", n + 1, RD_TYPE_NONE);
            rd_auto_name(ctx, name_va, "__rd_pe_module_name");
        }
    }

    RDAddress functions_va, names_va, ordinals_va;
    if(!pe_from_rva(pe, exportdir.AddressOfFunctions, &functions_va) ||
       !pe_from_rva(pe, exportdir.AddressOfNames, &names_va) ||
       !pe_from_rva(pe, exportdir.AddressOfNameOrdinals, &ordinals_va))
        return false;

    rd_reader_seek(r, functions_va);

    for(u32 i = 0; i < exportdir.NumberOfFunctions; i++) {
        u32 entry_rva;
        if(!rd_reader_read_le32(r, &entry_rva)) return false;
        if(!entry_rva) continue;

        u32 ord = exportdir.Base + i;
        const char* export_name = NULL;

        rd_reader_save(r);
        rd_reader_seek(r, ordinals_va);

        for(u32 j = 0; j < exportdir.NumberOfNames; j++) {
            u16 name_ord;

            if(!rd_reader_read_le16(r, &name_ord)) {
                rd_reader_restore(r);
                return false;
            }

            if((u32)name_ord == i) {
                u32 exportname_rva;
                rd_reader_seek(r, names_va + (j * sizeof(u32)));
                rd_reader_read_le32(r, &exportname_rva);

                RDAddress exportname_va;
                if(pe_from_rva(pe, exportname_rva, &exportname_va)) {
                    rd_reader_seek(r, exportname_va);
                    usize export_len = 0;
                    export_name = rd_reader_peek_str(r, &export_len);

                    if(export_name && export_len) {
                        rd_library_type(ctx, exportname_va, "char",
                                        export_len + 1, RD_TYPE_NONE);
                    }
                }

                break;
            }
        }

        RDAddress entry_va;
        if(pe_from_rva(pe, entry_rva, &entry_va)) {
            rd_set_external_ord(ctx, entry_va, NULL, ord, RD_EXT_EXPORTED);
            if(export_name) rd_library_name(ctx, entry_va, export_name);
        }

        const RDSegment* seg = rd_segment_find(ctx, entry_va);
        bool is_func = seg && rd_segment_has_perm(seg, RD_SP_X) &&
                       !_pe_is_linker_boundary(export_name);

        bool is_fwd = entry_rva >= d.VirtualAddress &&
                      entry_rva < d.VirtualAddress + d.Size;

        if(is_fwd) {
            // forwarded exports points to a null-terminated string
            // like DLLNAME.FunctionNName
            usize fwd_len;
            rd_reader_seek(r, entry_va);
            const char* fwd_name = rd_reader_peek_str(r, &fwd_len);

            if(fwd_name) {
                rd_library_type(ctx, entry_va, "char", fwd_len + 1,
                                RD_TYPE_NONE);
            }
        }
        else if(is_func)
            rd_set_function(ctx, entry_va);

        rd_reader_restore(r);
    }

    return true;
}
