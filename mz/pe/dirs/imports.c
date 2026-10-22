#include "imports.h"
#include <ctype.h>
#include <string.h>

#define PE_ORDINAL_FLAG64 0x8000000000000000ULL
#define PE_ORDINAL_FLAG32 0x80000000

typedef struct PEThunk {
    union {
        u64 rva;
        u16 ordinal;
    };

    bool is_ord;
} PEThunk;

static const char* const RD_WIN_MODULES[] = {
    "kernel32",   "user32",     "gdi32",    "advapi32", "shell32",
    "ole32",      "oleaut32",   "comctl32", "comdlg32", "ntdll",
    "api-ms-win", "ext-ms-win", NULL,
};

static bool _pe_is_win_module(const char* module) {
    const char* const* mod = RD_WIN_MODULES;

    while(*mod) {
        if(rd_stristr(*mod, module) == module) return true;
        mod++;
    }

    return false;
}

static bool _pe_read_thunk(RDAddress va, RDReader* r, const PEFormat* pe,
                           PEThunk* thunk) {
    rd_reader_seek(r, va);

    if(pe->bits == 64) {
        if(rd_reader_read_le64(r, &thunk->rva)) {
            thunk->is_ord = !!(thunk->rva & PE_ORDINAL_FLAG64);
            if(thunk->is_ord) thunk->ordinal = thunk->rva & 0xFFFF;
        }
    }
    else {
        u32 t;
        if(rd_reader_read_le32(r, &t)) {
            thunk->is_ord = !!(t & PE_ORDINAL_FLAG32);
            thunk->rva = (u64)t;
            if(thunk->is_ord) thunk->ordinal = thunk->rva & 0xFFFF;
        }
    }

    return !rd_reader_has_error(r);
}

static void _pe_read_thunks(RDContext* ctx, const PEFormat* pe, RDReader* r,
                            const char* module, RDAddress oft_va,
                            RDAddress ft_va) {
    rd_reader_seek(r, ft_va);

    while(true) {
        PEThunk oft_thunk, ft_thunk;
        if(!_pe_read_thunk(oft_va, r, pe, &oft_thunk)) break;
        if(!_pe_read_thunk(ft_va, r, pe, &ft_thunk)) break;

        if(!oft_thunk.rva) { // null terminator: type both slots
            rd_library_type(ctx, oft_va, pe->thunk_type, 0, RD_TYPE_NONE);
            rd_library_type(ctx, ft_va, pe->thunk_type, 0, RD_TYPE_NONE);
            break;
        }

        rd_library_type(ctx, oft_va, pe->thunk_type, 0, RD_TYPE_PTR);
        rd_library_type(ctx, ft_va, "function", 0, RD_TYPE_NONE);

        if(!oft_thunk.is_ord) {
            RDAddress thunkva;
            pe_from_rva(pe, oft_thunk.rva, &thunkva);

            rd_reader_seek(r, thunkva);
            rd_reader_read_le16(r, NULL); // skip hint

            usize n;
            const char* name = rd_reader_read_str(r, &n);

            if(name && n && (name[n - 1] == 'W') && _pe_is_win_module(module))
                rd_set_scan_char16(ctx, true);

            rd_library_type(ctx, thunkva, "u16", 0, RD_TYPE_NONE);
            rd_library_type(ctx, thunkva + sizeof(u16), "char", n + 1,
                            RD_TYPE_NONE);

            rd_auto_name(ctx, thunkva, rd_format("%s_%s_hint", module, name));

            rd_auto_name(ctx, thunkva + sizeof(u16),
                         rd_format("%s_%s_name", module, name));

            rd_set_external(ctx, ft_va, module, RD_EXT_IMPORTED);
            rd_library_name(ctx, ft_va, name);

            if(oft_va != ft_va)
                rd_library_name(ctx, oft_va,
                                rd_format("oft_%s_%s", module, name));
        }
        else
            rd_set_external_ord(ctx, ft_va, module, oft_thunk.ordinal,
                                RD_EXT_IMPORTED);

        ft_va += pe->thunk_size;
        oft_va += pe->thunk_size;
    }
}

static char* _pe_get_import_name_stem(const char* mod) {
    char* stem = rd_alloc0(strlen(mod) + 1, sizeof(char));
    strcpy(stem, mod);

    char* dot = strrchr(stem, '.');
    if(dot && dot != stem) *dot = 0;

    char* p = stem;
    while(*p) {
        *p = tolower((int)*p);
        p++;
    }

    return stem;
}

bool pe_imports_read_descriptor(RDReader* r, PEImportDescriptor* desc) {
    rd_reader_read_le32(r, &desc->OriginalFirstThunk);
    rd_reader_read_le32(r, &desc->TimeDateStamp);
    rd_reader_read_le32(r, &desc->ForwarderChain);
    rd_reader_read_le32(r, &desc->Name);
    rd_reader_read_le32(r, &desc->FirstThunk);
    if(rd_reader_has_error(r)) return false;

    return desc->Name && (desc->OriginalFirstThunk || desc->FirstThunk);
}

bool pe_read_imports_dir(RDContext* ctx, const PEFormat* pe) {
    PEDataDirectory d = pe->data_dirs[PE_DIRECTORY_ENTRY_IMPORT];

    RDAddress va;
    if(!pe_from_rva(pe, d.VirtualAddress, &va)) return false;

    RDReader* r = rd_get_reader(ctx);
    rd_reader_seek(r, va);

    PEImportDescriptor desc;
    while(pe_imports_read_descriptor(r, &desc)) {
        rd_reader_save(r);
        rd_library_type(ctx, va, "PE_IMPORT_DESCRIPTOR", 0, RD_TYPE_NONE);

        char* mod = rd_strdup(pe_imports_get_descriptor_name(r, pe, &desc));

        if(mod) {
            char* import_stem = _pe_get_import_name_stem(mod);

            const char* kb_path =
                rd_format("platform/win32/functions/%s", import_stem);
            rd_kb_load(ctx, kb_path);

            kb_path = rd_format("platform/win32/ordinals/%s", import_stem);
            rd_kb_load(ctx, kb_path);

            rd_free(import_stem);

            RDAddress name_va;
            if(pe_from_rva(pe, desc.Name, &name_va)) {
                rd_library_type(ctx, name_va, "char", strlen(mod) + 1,
                                RD_TYPE_NONE);
            }

            RDAddress ft_va, oft_va;
            bool has_ft = pe_from_rva(pe, desc.FirstThunk, &ft_va);
            bool has_oft = pe_from_rva(pe, desc.OriginalFirstThunk, &oft_va);

            if(!has_oft) oft_va = ft_va;
            if(!has_ft) ft_va = oft_va;
            if(oft_va && ft_va) _pe_read_thunks(ctx, pe, r, mod, oft_va, ft_va);

            rd_free(mod);
        }

        va = rd_reader_restore(r);
    }

    return true;
}

const char* pe_imports_get_descriptor_name(RDReader* r, const PEFormat* pe,
                                           const PEImportDescriptor* desc) {
    RDAddress name_va;
    char* module = NULL;

    if(pe_from_rva(pe, desc->Name, &name_va)) {
        usize n;
        rd_reader_seek(r, name_va);
        return rd_reader_peek_str(r, &n);
    }

    return NULL;
}
