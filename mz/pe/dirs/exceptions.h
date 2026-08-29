#pragma once

#include "pe/format.h"
#include <redasm/redasm.h>

typedef struct PERuntimeFunctionEntry {
    u32 BeginAddress;
    u32 EndAddress;
    u32 UnwindInfoAddress; // or UnwindData
} PERuntimeFunctionEntry;

typedef struct PEUnwindInfoHeader {
    u8 VersionFlags;
    u8 SizeOfProlog;
    u8 CountOfCodes;
    u8 FrameRegisterOffset;
} PEUnwindInfoHeader;

bool pe_read_exceptions_dir(RDContext* ctx, PEFormat* pe);
