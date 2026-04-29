#include "debug.h"

#define IMAGE_DEBUG_TYPE_UNKNOWN 0
#define IMAGE_DEBUG_TYPE_COFF 1
#define IMAGE_DEBUG_TYPE_CODEVIEW 2
#define IMAGE_DEBUG_TYPE_FPO 3
#define IMAGE_DEBUG_TYPE_MISC 4
#define IMAGE_DEBUG_TYPE_EXCEPTION 5
#define IMAGE_DEBUG_TYPE_FIXUP 6
#define IMAGE_DEBUG_TYPE_OMAP_TO_SRC 7
#define IMAGE_DEBUG_TYPE_OMAP_FROM_SRC 8
#define IMAGE_DEBUG_TYPE_BORLAND 9
#define IMAGE_DEBUG_TYPE_RESERVED10 10
#define IMAGE_DEBUG_TYPE_CLSID 11
#define IMAGE_DEBUG_TYPE_VC_FEATURE 12
#define IMAGE_DEBUG_TYPE_POGO 13
#define IMAGE_DEBUG_TYPE_ILTCG 14
#define IMAGE_DEBUG_TYPE_MPX 15
#define IMAGE_DEBUG_TYPE_REPRO 16

// little-endian u32 of ASCII magic
#define PE_CVINFO_PDB20_SIGNATURE 0x3031424E // 'NB10'
#define PE_CVINFO_PDB70_SIGNATURE 0x53445352 // 'RSDS'

void pe_register_debug_types(RDContext* ctx) {
    // clang-format off
    RDTypeDef* dbgdir = rd_typedef_create_struct("IMAGE_DEBUG_DIRECTORY", ctx);
    rd_typedef_add_member(dbgdir, "u32", "Characteristics",   0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(dbgdir, "u32", "TimeDateStamp",     0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(dbgdir, "u16", "MajorVersion",      0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(dbgdir, "u16", "MinorVersion",      0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(dbgdir, "u32", "Type",              0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(dbgdir, "u32", "SizeOfData",        0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(dbgdir, "u32", "AddressOfRawData",  0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(dbgdir, "u32", "PointerToRawData",  0, RD_TYPE_NONE, ctx);
    rd_typedef_register(dbgdir, ctx);
 
    RDTypeDef* pdb20 = rd_typedef_create_struct("CV_INFO_PDB20", ctx);
    rd_typedef_add_member(pdb20, "u32", "CvSignature", 0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(pdb20, "u32", "Offset",      0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(pdb20, "u32", "Signature",   0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(pdb20, "u32", "Age",         0, RD_TYPE_NONE, ctx);
    rd_typedef_register(pdb20, ctx);
 
    RDTypeDef* pdb70 = rd_typedef_create_struct("CV_INFO_PDB70", ctx);
    rd_typedef_add_member(pdb70, "u32", "CvSignature",  0, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(pdb70, "u8",  "Signature", 16, RD_TYPE_NONE, ctx);
    rd_typedef_add_member(pdb70, "u32", "Age",          0, RD_TYPE_NONE, ctx);
    rd_typedef_register(pdb70, ctx);
    // clang-format on
}

static void _pe_read_codeview(RDContext* ctx, PEFormat* pe, RDReader* r,
                              const ImageDebugDirectory* dbgdir) {
    RDAddress dbgva;
    if(!pe_from_rva(pe, dbgdir->AddressOfRawData, &dbgva)) return;

    u32 sig;
    rd_reader_seek(r, dbgva);
    if(!rd_reader_read_le32(r, &sig)) return;

    if(sig == PE_CVINFO_PDB20_SIGNATURE) {
        rd_user_type(ctx, dbgva, "CV_INFO_PDB20", 0, RD_TYPE_NONE);

        RDAddress pdbname_va = dbgva + rd_size_of(ctx, "CV_INFO_PDB20", 0);
        usize n;
        rd_reader_seek(r, pdbname_va);
        const char* pdbname = rd_reader_peek_str(r, &n);
        if(pdbname) rd_user_type(ctx, pdbname_va, "char", n + 1, RD_TYPE_NONE);
    }
    else if(sig == PE_CVINFO_PDB70_SIGNATURE) {
        rd_user_type(ctx, dbgva, "CV_INFO_PDB70", 0, RD_TYPE_NONE);

        RDAddress pdbname_va = dbgva + rd_size_of(ctx, "CV_INFO_PDB70", 0);
        usize n;
        rd_reader_seek(r, pdbname_va);
        const char* pdbname = rd_reader_peek_str(r, &n);
        if(pdbname) rd_user_type(ctx, pdbname_va, "char", n + 1, RD_TYPE_NONE);
    }
}

bool pe_read_debug(RDContext* ctx, PEFormat* pe) {
    ImageDataDirectory d = pe->datadir[IMAGE_DIRECTORY_ENTRY_DEBUG];
    if(!d.VirtualAddress || !d.Size) return false;

    RDAddress va;
    if(!pe_from_rva(pe, d.VirtualAddress, &va)) return false;

    usize entrysize = rd_size_of(ctx, "IMAGE_DEBUG_DIRECTORY", 0);
    usize n = d.Size / entrysize;
    RDReader* r = rd_get_reader(ctx);

    for(usize i = 0; i < n; i++) {
        RDAddress entry_va = va + (i * entrysize);
        rd_reader_seek(r, entry_va);

        ImageDebugDirectory dbgdir;
        rd_reader_read_le32(r, &dbgdir.Characteristics);
        rd_reader_read_le32(r, &dbgdir.TimeDateStamp);
        rd_reader_read_le16(r, &dbgdir.MajorVersion);
        rd_reader_read_le16(r, &dbgdir.MinorVersion);
        rd_reader_read_le32(r, &dbgdir.Type);
        rd_reader_read_le32(r, &dbgdir.SizeOfData);
        rd_reader_read_le32(r, &dbgdir.AddressOfRawData);
        rd_reader_read_le32(r, &dbgdir.PointerToRawData);
        if(rd_reader_has_error(r)) break;

        rd_user_type(ctx, entry_va, "IMAGE_DEBUG_DIRECTORY", 0, RD_TYPE_NONE);

        if(dbgdir.Type == IMAGE_DEBUG_TYPE_CODEVIEW)
            _pe_read_codeview(ctx, pe, r, &dbgdir);
    }

    return true;
}
