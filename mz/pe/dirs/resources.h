#pragma once

#include "pe/format.h"

#define IMAGE_RESOURCE_NAME_IS_STRING 0x80000000
#define IMAGE_RESOURCE_DATA_IS_DIRECTORY 0x80000000

typedef struct {
    u32 Characteristics;
    u32 TimeDateStamp;
    u16 MajorVersion;
    u16 MinorVersion;
    u16 NumberOfNamedEntries;
    u16 NumberOfIdEntries;
} ImageResourceDirectory;

typedef struct {
    u32 NameOffset;
    u32 OffsetToData;
} ImageResourceDirectoryEntry;

typedef struct {
    u32 OffsetToData;
    u32 Size;
    u32 CodePage;
    u32 Reserved;
} ImageResourceDataEntry;

void pe_register_resources_types(RDContext* ctx);
bool pe_read_resources(RDContext* ctx, PEFormat* pe);
