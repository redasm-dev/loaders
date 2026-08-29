#include "exceptions.h"
#include <inttypes.h>

#define PE_UNW_FLAG_EHANDLER 0x1
#define PE_UNW_FLAG_UHANDLER 0x2
#define PE_UNW_FLAG_CHAININFO 0x4

static bool _pe_read_runtime_function_entry(RDReader* r,
                                            PERuntimeFunctionEntry* entry) {
    rd_reader_read_le32(r, &entry->BeginAddress);
    rd_reader_read_le32(r, &entry->EndAddress);
    rd_reader_read_le32(r, &entry->UnwindInfoAddress);
    return !rd_reader_has_error(r);
}

static bool _pe_read_unwindinfo_header(RDReader* r, PEUnwindInfoHeader* v) {
    rd_reader_read(r, &v->VersionFlags, 1);
    rd_reader_read(r, &v->SizeOfProlog, 1);
    rd_reader_read(r, &v->CountOfCodes, 1);
    rd_reader_read(r, &v->FrameRegisterOffset, 1);
    return !rd_reader_has_error(r);
}

static void _pe_read_unwindinfo_funcinfo(RDContext* ctx, PEFormat* pe,
                                         RDReader* r, RDAddress unwind_va) {
    rd_reader_save(r);
    rd_reader_seek(r, unwind_va);

    PEUnwindInfoHeader uh;

    if(_pe_read_unwindinfo_header(r, &uh)) {
        u8 flags = (u8)((uh.VersionFlags >> 3) & 0x1F);

        if(flags & (PE_UNW_FLAG_EHANDLER | PE_UNW_FLAG_UHANDLER)) {
            // Skip the variable-length unwind code array (each code is a
            // 2-byte USHORT; an odd count gets a 2-byte alignment pad
            // before what follows).
            usize codebytes = (usize)uh.CountOfCodes * 2;
            if(uh.CountOfCodes & 1) codebytes += 2;

            rd_reader_seek(r, (RDAddress)rd_reader_tell(r) + codebytes);

            // ExceptionHandler: RVA of the language-specific handler
            // (__CxxFrameHandler3/4 itself on x64, no per-function
            // trampoline needed here, unlike x86).
            u32 handler_rva = 0;
            bool have_handler = rd_reader_read_le32(r, &handler_rva);
            RD_UNUSED(handler_rva);

            if(have_handler) {
                // ExceptionData[0]: for the C++ EH handler.
                // This is the FuncInfo RVA.
                u32 funcinfo_rva = 0;

                if(rd_reader_read_le32(r, &funcinfo_rva) && funcinfo_rva) {
                    RDAddress funcinfo_va;

                    if(pe_from_rva(pe, funcinfo_rva, &funcinfo_va)) {
                        rd_library_type(ctx, funcinfo_va,
                                        "PE_EH_FUNCINFO_CANDIDATE", 0,
                                        RD_TYPE_NONE);
                    }
                }
            }
        }
    }

    rd_reader_restore(r);
}

bool pe_read_exceptions_dir(RDContext* ctx, PEFormat* pe) {
    // exception directory is only present on x64 (and Itanium)
    if(pe->bits != 64) return false;

    PEDataDirectory d = pe->data_dirs[PE_DIRECTORY_ENTRY_EXCEPTION];
    if(!d.VirtualAddress || !d.Size) return false;

    RDAddress va;
    if(!pe_from_rva(pe, d.VirtualAddress, &va)) return false;

    RDReader* r = rd_get_reader(ctx);
    rd_reader_seek(r, va);

    while(rd_reader_tell(r) < va + d.Size) {
        RDAddress entry_va = rd_reader_tell(r);

        PERuntimeFunctionEntry entry;
        if(!_pe_read_runtime_function_entry(r, &entry)) break;
        if(!entry.BeginAddress) continue;

        rd_library_type(ctx, entry_va, "PE_RUNTIME_FUNCTION_ENTRY", 0,
                        RD_TYPE_NONE);

        RDAddress func_va;
        if(!pe_from_rva(pe, entry.BeginAddress, &func_va)) continue;

        func_va = pe_norm(ctx, pe, func_va);

        rd_set_function(ctx, func_va);
        rd_placeholder_name(ctx, func_va, rd_format("exc_%" PRIX64, func_va));

        if(entry.UnwindInfoAddress) {
            RDAddress unwind_va;

            if(pe_from_rva(pe, entry.UnwindInfoAddress, &unwind_va))
                _pe_read_unwindinfo_funcinfo(ctx, pe, r, unwind_va);
        }
    }

    return true;
}
