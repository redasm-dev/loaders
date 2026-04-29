#pragma once

#include "pe/format.h"

typedef struct ImageExportDirectory {
    u32 Characteristics;
    u32 TimeDateStamp;
    u16 MajorVersion;
    u16 MinorVersion;
    u32 Name;
    u32 Base;
    u32 NumberOfFunctions;
    u32 NumberOfNames;
    u32 AddressOfFunctions;
    u32 AddressOfNames;
    u32 AddressOfNameOrdinals;
} ImageExportDirectory;

void pe_register_exports_types(RDContext* ctx);
bool pe_read_exports(RDContext* ctx, PEFormat* pe);
