#include "tls.h"
#include <inttypes.h>

// Sanity cap on the callback array walk: real binaries have a handful of
// callbacks at most.
// This only exists to keep a corrupted/malicious
// AddressOfCallBacks from spinning far past anything plausible before the
// reader's own EOF handling would eventually stop it.
#define PE_TLS_MAX_CALLBACKS 256

static bool _pe_read_tls_dir32(RDReader* r, PETlsDirectory32* v) {
    rd_reader_read_le32(r, &v->StartAddressOfRawData);
    rd_reader_read_le32(r, &v->EndAddressOfRawData);
    rd_reader_read_le32(r, &v->AddressOfIndex);
    rd_reader_read_le32(r, &v->AddressOfCallBacks);
    rd_reader_read_le32(r, &v->SizeOfZeroFill);
    rd_reader_read_le32(r, &v->Characteristics);
    return !rd_reader_has_error(r);
}

static bool _pe_read_tls_dir64(RDReader* r, PETlsDirectory64* v) {
    rd_reader_read_le64(r, &v->StartAddressOfRawData);
    rd_reader_read_le64(r, &v->EndAddressOfRawData);
    rd_reader_read_le64(r, &v->AddressOfIndex);
    rd_reader_read_le64(r, &v->AddressOfCallBacks);
    rd_reader_read_le32(r, &v->SizeOfZeroFill);
    rd_reader_read_le32(r, &v->Characteristics);
    return !rd_reader_has_error(r);
}

static void _pe_read_tls_callbacks(RDContext* ctx, PEFormat* pe, RDReader* r,
                                   RDAddress callbacks_va) {
    if(!callbacks_va) return; // directory present, no callbacks: not an error

    rd_reader_seek(r, callbacks_va);
    u32 index = 0;

    while(index < PE_TLS_MAX_CALLBACKS) {
        RDAddress slot_va = (RDAddress)rd_reader_tell(r);
        RDAddress cb_va;
        bool ok;

        // pe->thunk_size/thunk_type already encode this module's natural
        // pointer width (set in pe_set_bits, accounting for the ARM64
        // special case)
        if(pe->thunk_size == sizeof(u64)) {
            u64 v = 0;
            ok = rd_reader_read_le64(r, &v);
            cb_va = (RDAddress)v;
        }
        else {
            u32 v = 0;
            ok = rd_reader_read_le32(r, &v);
            cb_va = (RDAddress)v;
        }

        if(!ok || !cb_va)
            break; // read failure or null terminator: done either way

        rd_library_type(ctx, slot_va, pe->thunk_type, 0, RD_TYPE_PTR);

        // cb_va is already a raw VA (see the note in tls.h)
        // pe_norm only, never pe_from_rva.
        RDAddress fn_va = pe_norm(ctx, pe, cb_va);
        rd_set_function(ctx, fn_va);
        rd_placeholder_name(ctx, fn_va,
                            rd_format("tls_callback_%" PRIu32, index));

        index++;
    }
}

bool pe_read_tls_dir(RDContext* ctx, PEFormat* pe) {
    PEDataDirectory d = pe->data_dirs[PE_DIRECTORY_ENTRY_TLS];

    RDAddress va;
    if(!pe_from_rva(pe, d.VirtualAddress, &va)) return false;

    RDReader* r = rd_get_reader(ctx);
    rd_reader_seek(r, va);

    RDAddress callbacks_va;

    if(pe->bits == 32) {
        PETlsDirectory32 tlsdir;
        if(!_pe_read_tls_dir32(r, &tlsdir)) return false;

        rd_library_type(ctx, va, "PE_TLS_DIRECTORY32", 0, RD_TYPE_NONE);

        if(tlsdir.AddressOfIndex) {
            rd_library_type(ctx, tlsdir.AddressOfIndex, "u32", 0, RD_TYPE_NONE);
            rd_auto_name(ctx, tlsdir.AddressOfIndex, "_tls_index");
        }

        callbacks_va = tlsdir.AddressOfCallBacks;
    }
    else {
        PETlsDirectory64 tlsdir;
        if(!_pe_read_tls_dir64(r, &tlsdir)) return false;

        rd_library_type(ctx, va, "PE_TLS_DIRECTORY64", 0, RD_TYPE_NONE);

        if(tlsdir.AddressOfIndex) {
            // The TLS slot index itself is always a 32-bit value, even in
            // the 64-bit directory -- that's the OS's per-module TLS
            // array index, not a pointer-width quantity.
            rd_library_type(ctx, tlsdir.AddressOfIndex, "u32", 0, RD_TYPE_NONE);
            rd_auto_name(ctx, tlsdir.AddressOfIndex, "_tls_index");
        }

        callbacks_va = tlsdir.AddressOfCallBacks;
    }

    _pe_read_tls_callbacks(ctx, pe, r, callbacks_va);
    return true;
}
