#pragma once

#include "pe/format.h"

typedef struct ImageImportDescriptor {
    u32 OriginalFirstThunk;
    u32 TimeDateStamp;
    u32 ForwarderChain;
    u32 Name;
    u32 FirstThunk;
} ImageImportDescriptor;

typedef struct ImageImportByName {
    u16 Hint;
    u8 Name[1];
} ImageImportByName;

typedef u32 ImageThunkData32;
typedef u64 ImageThunkData64;

void pe_register_imports_types(RDContext* ctx);
bool pe_read_imports(RDContext* ctx, PEFormat* pe);
