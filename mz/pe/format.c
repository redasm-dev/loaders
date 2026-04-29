#include "format.h"

bool pe_from_rva(PEFormat* pe, RDAddress rva, RDAddress* va) {
    if(!rva) return false;
    *va = pe->imagebase + rva;
    return true;
}

int pe_get_bits(PEFormat* pe) {
    switch(pe->fileheader.Machine) {
        case IMAGE_FILE_MACHINE_AMD64: return 64;

        case IMAGE_FILE_MACHINE_ARM: {
            if(pe->opt32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) return 64;
            break;
        }

        default: break;
    }

    return 32;
}

bool pe_read_section_header(PEFormat* pe, RDReader* r, int idx,
                            ImageSectionHeader* s) {
    const u32 FIRST_SECTION =
        pe->dosheader.e_lfanew + pe->fileheader.SizeOfOptionalHeader + 0x18;

    rd_reader_seek(r, FIRST_SECTION + (idx * sizeof(ImageSectionHeader)));

    rd_reader_read(r, &s->Name, IMAGE_SIZEOF_SHORT_NAME);
    rd_reader_read_le32(r, &s->VirtualSize);
    rd_reader_read_le32(r, &s->VirtualAddress);
    rd_reader_read_le32(r, &s->SizeOfRawData);
    rd_reader_read_le32(r, &s->PointerToRawData);
    rd_reader_read_le32(r, &s->PointerToRelocations);
    rd_reader_read_le32(r, &s->PointerToLinenumbers);
    rd_reader_read_le16(r, &s->NumberOfRelocations);
    rd_reader_read_le16(r, &s->NumberOfLinenumbers);
    rd_reader_read_le32(r, &s->Characteristics);

    return !rd_reader_has_error(r);
}
