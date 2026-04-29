#pragma once

#include "constants.h"
#include <redasm/redasm.h>

#define IMAGE_FIRST_SECTION(nt)                                                \
    (ImageSectionHeader*)((char*)(nt) + nt->FileHeader.SizeOfOptionalHeader +  \
                          0x18)

typedef struct ImageFileHeader {
    u16 Machine, NumberOfSections;
    u32 TimeDateStamp, PointerToSymbolTable, NumberOfSymbols;
    u16 SizeOfOptionalHeader, Characteristics;
} ImageFileHeader;

typedef struct ImageDataDirectory {
    u32 VirtualAddress, Size;
} ImageDataDirectory;

typedef struct ImageOptionalHeader32 {
    u16 Magic;
    u8 MajorLinkerVersion, MinorLinkerVersion;
    u32 SizeOfCode, SizeOfInitializedData, SizeOfUninitializedData;
    u32 AddressOfEntryPoint, BaseOfCode, BaseOfData, ImageBase;
    u32 SectionAlignment, FileAlignment;
    u16 MajorOperatingSystemVersion, MinorOperatingSystemVersion;
    u16 MajorImageVersion, MinorImageVersion;
    u16 MajorSubsystemVersion, MinorSubsystemVersion;
    u32 Win32VersionValue, SizeOfImage, SizeOfHeaders, CheckSum;
    u16 Subsystem, DllCharacteristics;
    u32 SizeOfStackReserve, SizeOfStackCommit;
    u32 SizeOfHeapReserve, SizeOfHeapCommit;
    u32 LoaderFlags, NumberOfRvaAndSizes;
    // ImageDataDirectory DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} ImageOptionalHeader32;

typedef struct ImageOptionalHeader64 {
    u16 Magic;
    u8 MajorLinkerVersion, MinorLinkerVersion;
    u32 SizeOfCode, SizeOfInitializedData, SizeOfUninitializedData;
    u32 AddressOfEntryPoint, BaseOfCode;
    u64 ImageBase;
    u32 SectionAlignment, FileAlignment;
    u16 MajorOperatingSystemVersion, MinorOperatingSystemVersion;
    u16 MajorImageVersion, MinorImageVersion;
    u16 MajorSubsystemVersion, MinorSubsystemVersion;
    u32 Win32VersionValue, SizeOfImage, SizeOfHeaders, CheckSum;
    u16 Subsystem, DllCharacteristics;
    u64 SizeOfStackReserve, SizeOfStackCommit;
    u64 SizeOfHeapReserve, SizeOfHeapCommit;
    u32 LoaderFlags, NumberOfRvaAndSizes;
    // ImageDataDirectory DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} ImageOptionalHeader64;

// typedef struct ImageNtHeaders {
//     u32 Signature;
//     ImageFileHeader FileHeader;
// } ImageNtHeaders;

typedef struct ImageSectionHeader {
    char Name[IMAGE_SIZEOF_SHORT_NAME];
    u32 VirtualSize, VirtualAddress;
    u32 SizeOfRawData, PointerToRawData;
    u32 PointerToRelocations, PointerToLinenumbers;
    u16 NumberOfRelocations, NumberOfLinenumbers;
    u32 Characteristics;
} ImageSectionHeader;
