#pragma once

#include "pe/format.h"

typedef struct ImageDebugDirectory {
    u32 Characteristics;
    u32 TimeDateStamp;
    u16 MajorVersion;
    u16 MinorVersion;
    u32 Type;
    u32 SizeOfData;
    u32 AddressOfRawData;
    u32 PointerToRawData;
} ImageDebugDirectory;

typedef struct CvInfoPdb20 {
    u32 CvSignature;
    u32 Offset;
    u32 Signature;
    u32 Age;
} CvInfoPdb20;

typedef struct CvInfoPdb70 {
    u32 CvSignature;
    u8 Signature[16]; // GUID
    u32 Age;
} CvInfoPdb70;

void pe_register_debug_types(RDContext* ctx);
bool pe_read_debug(RDContext* ctx, PEFormat* pe);
