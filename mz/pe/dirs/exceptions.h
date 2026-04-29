#pragma once

#include "pe/format.h"
#include <redasm/redasm.h>

typedef struct ImageRuntimeFunctionEntry {
    u32 BeginAddress;
    u32 EndAddress;
    u32 UnwindInfoAddress; // or UnwindData
} ImageRuntimeFunctionEntry;

void pe_register_exceptions_types(RDContext* ctx);
bool pe_read_exceptions(RDContext* ctx, PEFormat* pe);
