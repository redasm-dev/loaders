#include "debug.h"

#define PE_DEBUG_TYPE_UNKNOWN 0
#define PE_DEBUG_TYPE_COFF 1
#define PE_DEBUG_TYPE_CODEVIEW 2
#define PE_DEBUG_TYPE_FPO 3
#define PE_DEBUG_TYPE_MISC 4
#define PE_DEBUG_TYPE_EXCEPTION 5
#define PE_DEBUG_TYPE_FIXUP 6
#define PE_DEBUG_TYPE_OMAP_TO_SRC 7
#define PE_DEBUG_TYPE_OMAP_FROM_SRC 8
#define PE_DEBUG_TYPE_BORLAND 9
#define PE_DEBUG_TYPE_RESERVED10 10
#define PE_DEBUG_TYPE_CLSID 11
#define PE_DEBUG_TYPE_VC_FEATURE 12
#define PE_DEBUG_TYPE_POGO 13
#define PE_DEBUG_TYPE_ILTCG 14
#define PE_DEBUG_TYPE_MPX 15
#define PE_DEBUG_TYPE_REPRO 16

// little-endian u32 of ASCII magic
#define PE_CVINFO_PDB20_SIGNATURE 0x3031424E // 'NB10'
#define PE_CVINFO_PDB70_SIGNATURE 0x53445352 // 'RSDS'

static bool _pe_read_cv_info_pdb20(RDReader* r, CvInfoPdb20* pdb) {
    rd_reader_read_le32(r, &pdb->CvSignature);
    rd_reader_read_le32(r, &pdb->Offset);
    rd_reader_read_le32(r, &pdb->TimeStamp);
    rd_reader_read_le32(r, &pdb->Age);

    return !rd_reader_has_error(r);
}

static bool _pe_read_cv_info_pdb70(RDReader* r, CvInfoPdb70* pdb) {
    rd_reader_read_le32(r, &pdb->CvSignature);
    rd_reader_read_le32(r, &pdb->GuidData1);
    rd_reader_read_le16(r, &pdb->GuidData2);
    rd_reader_read_le16(r, &pdb->GuidData3);
    rd_reader_read_exact(r, pdb->GuidData4, sizeof(pdb->GuidData4));
    rd_reader_read_le32(r, &pdb->Age);

    return !rd_reader_has_error(r);
}

static bool _pe_read_debug_directory(RDReader* r, PEDebugDirectory* dbgdir) {
    rd_reader_read_le32(r, &dbgdir->Characteristics);
    rd_reader_read_le32(r, &dbgdir->TimeDateStamp);
    rd_reader_read_le16(r, &dbgdir->MajorVersion);
    rd_reader_read_le16(r, &dbgdir->MinorVersion);
    rd_reader_read_le32(r, &dbgdir->Type);
    rd_reader_read_le32(r, &dbgdir->SizeOfData);
    rd_reader_read_le32(r, &dbgdir->AddressOfRawData);
    rd_reader_read_le32(r, &dbgdir->PointerToRawData);

    return !rd_reader_has_error(r);
}

static void _pe_read_codeview(RDContext* ctx, PEFormat* pe, RDReader* r,
                              const PEDebugDirectory* dbgdir) {
    RDAddress dbg_va;
    if(!pe_from_rva(pe, dbgdir->AddressOfRawData, &dbg_va)) return;

    u32 sig;
    rd_reader_seek(r, dbg_va);
    if(!rd_reader_peek_le32(r, &sig)) return;

    if(sig == PE_CVINFO_PDB20_SIGNATURE) {
        CvInfoPdb20 pdb;
        if(!_pe_read_cv_info_pdb20(r, &pdb)) return;

        rd_library_type(ctx, dbg_va, "CV_INFO_PDB20", 0, RD_TYPE_NONE);

        RDAddress pdbfilename_va = (RDAddress)rd_reader_tell(r);
        usize n;
        const char* pdbname = rd_reader_peek_str(r, &n);

        if(pdbname) {
            rd_library_type(ctx, pdbfilename_va, "char", n + 1, RD_TYPE_NONE);
            rd_auto_name(ctx, pdbfilename_va, "__pdb_filename");
            RD_LOG_INFO("PDB 2.0: %s (Server Key: %08X%X)", pdbname,
                        pdb.TimeStamp, pdb.Age);
        }
    }
    else if(sig == PE_CVINFO_PDB70_SIGNATURE) {
        CvInfoPdb70 pdb;
        if(!_pe_read_cv_info_pdb70(r, &pdb)) return;

        rd_library_type(ctx, dbg_va, "CV_INFO_PDB70", 0, RD_TYPE_NONE);

        usize n;
        const char* pdb_filepath = rd_reader_peek_str(r, &n);

        if(pdb_filepath) {
            RDAddress pdbfilename_va = (RDAddress)rd_reader_tell(r);
            rd_library_type(ctx, pdbfilename_va, "char", n + 1, RD_TYPE_NONE);
            rd_library_name(ctx, pdbfilename_va, "__pdb_filename");

            const char* pdb_guid =
                rd_format("%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X",
                          pdb.GuidData1, pdb.GuidData2, pdb.GuidData3,
                          pdb.GuidData4[0], pdb.GuidData4[1], pdb.GuidData4[2],
                          pdb.GuidData4[3], pdb.GuidData4[4], pdb.GuidData4[5],
                          pdb.GuidData4[6], pdb.GuidData4[7]);

            RD_LOG_INFO("PDB 7.0: %s (Server Key: %s%X)", pdb_filepath,
                        pdb_guid, pdb.Age);

            rd_analyzer_enable(ctx, "pdb_debuginfo");
        }
    }
}

bool pe_read_debug_dir(RDContext* ctx, PEFormat* pe) {
    PEDataDirectory d = pe->data_dirs[PE_DIRECTORY_ENTRY_DEBUG];
    if(!d.VirtualAddress || !d.Size) return false;

    RDAddress va;
    if(!pe_from_rva(pe, d.VirtualAddress, &va)) return false;

    RDReader* r = rd_get_reader(ctx);
    rd_reader_seek(r, va);

    while(rd_reader_tell(r) < va + d.Size) {
        RDAddress entry_va = rd_reader_tell(r);

        PEDebugDirectory dbgdir;
        if(!_pe_read_debug_directory(r, &dbgdir)) break;

        rd_reader_save(r);
        rd_library_type(ctx, entry_va, "PE_DEBUG_DIRECTORY", 0, RD_TYPE_NONE);

        if(dbgdir.Type == PE_DEBUG_TYPE_CODEVIEW)
            _pe_read_codeview(ctx, pe, r, &dbgdir);

        rd_reader_restore(r);
    }

    return true;
}
