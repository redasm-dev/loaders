#include "pe.h"
#include "format.h"
#include "pe/dirs/config.h"
#include "pe/dirs/debug.h"
#include "pe/dirs/dotnet.h"
#include "pe/dirs/exceptions.h"
#include "pe/dirs/exports.h"
#include "pe/dirs/imports.h"
#include "pe/dirs/resources.h"
#include "pe/dirs/tls.h"
#include <inttypes.h>
#include <string.h>

static bool pe_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    PEFormat* pe = (PEFormat*)ldr;
    if(!mz_read_dos_header(req->input, &pe->dosheader)) return false;

    if(!mz_match_signature(req->input, &pe->dosheader, MZ_NT_SIGNATURE))
        return false;

    rd_reader_read_le16(req->input, &pe->fileheader.Machine);
    rd_reader_read_le16(req->input, &pe->fileheader.NumberOfSections);
    rd_reader_read_le32(req->input, &pe->fileheader.TimeDateStamp);
    rd_reader_read_le32(req->input, &pe->fileheader.PointerToSymbolTable);
    rd_reader_read_le32(req->input, &pe->fileheader.NumberOfSymbols);
    rd_reader_read_le16(req->input, &pe->fileheader.SizeOfOptionalHeader);
    rd_reader_read_le16(req->input, &pe->fileheader.Characteristics);
    if(rd_reader_has_error(req->input)) return false;

    rd_reader_read_le16(req->input, &pe->opt32.Magic);
    rd_reader_read_byte(req->input, &pe->opt32.MajorLinkerVersion);
    rd_reader_read_byte(req->input, &pe->opt32.MinorLinkerVersion);
    rd_reader_read_le32(req->input, &pe->opt32.SizeOfCode);
    rd_reader_read_le32(req->input, &pe->opt32.SizeOfInitializedData);
    rd_reader_read_le32(req->input, &pe->opt32.SizeOfUninitializedData);
    rd_reader_read_le32(req->input, &pe->opt32.AddressOfEntryPoint);
    rd_reader_read_le32(req->input, &pe->opt32.BaseOfCode);

    if(pe->opt32.Magic == PE_NT_OPTIONAL_HDR32_MAGIC) {
        rd_reader_read_le32(req->input, &pe->opt32.BaseOfData);
        rd_reader_read_le32(req->input, &pe->opt32.ImageBase);
        pe->imagebase = pe->opt32.ImageBase;
        pe->entrypoint = pe->opt32.AddressOfEntryPoint;
    }
    else if(pe->opt32.Magic == PE_NT_OPTIONAL_HDR64_MAGIC) {
        rd_reader_read_le64(req->input, &pe->opt64.ImageBase);
        pe->imagebase = pe->opt64.ImageBase;
        pe->entrypoint = pe->opt64.AddressOfEntryPoint;
    }
    else
        return false;

    rd_reader_read_le32(req->input, &pe->opt32.SectionAlignment);
    rd_reader_read_le32(req->input, &pe->opt32.FileAlignment);
    rd_reader_read_le16(req->input, &pe->opt32.MajorOperatingSystemVersion);
    rd_reader_read_le16(req->input, &pe->opt32.MinorOperatingSystemVersion);
    rd_reader_read_le16(req->input, &pe->opt32.MajorImageVersion);
    rd_reader_read_le16(req->input, &pe->opt32.MinorImageVersion);
    rd_reader_read_le16(req->input, &pe->opt32.MajorSubsystemVersion);
    rd_reader_read_le16(req->input, &pe->opt32.MinorSubsystemVersion);
    rd_reader_read_le32(req->input, &pe->opt32.Win32VersionValue);
    rd_reader_read_le32(req->input, &pe->opt32.SizeOfImage);
    rd_reader_read_le32(req->input, &pe->opt32.SizeOfHeaders);
    rd_reader_read_le32(req->input, &pe->opt32.CheckSum);
    rd_reader_read_le16(req->input, &pe->opt32.Subsystem);
    rd_reader_read_le16(req->input, &pe->opt32.DllCharacteristics);

    if(pe->opt32.Magic == PE_NT_OPTIONAL_HDR32_MAGIC) {
        rd_reader_read_le32(req->input, &pe->opt32.SizeOfStackReserve);
        rd_reader_read_le32(req->input, &pe->opt32.SizeOfStackCommit);
        rd_reader_read_le32(req->input, &pe->opt32.SizeOfHeapReserve);
        rd_reader_read_le32(req->input, &pe->opt32.SizeOfHeapCommit);
        rd_reader_read_le32(req->input, &pe->opt32.LoaderFlags);
        rd_reader_read_le32(req->input, &pe->opt32.NumberOfRvaAndSizes);
        pe->section_align = pe->opt32.SectionAlignment;
    }
    else if(pe->opt32.Magic == PE_NT_OPTIONAL_HDR64_MAGIC) {
        rd_reader_read_le64(req->input, &pe->opt64.SizeOfStackReserve);
        rd_reader_read_le64(req->input, &pe->opt64.SizeOfStackCommit);
        rd_reader_read_le64(req->input, &pe->opt64.SizeOfHeapReserve);
        rd_reader_read_le64(req->input, &pe->opt64.SizeOfHeapCommit);
        rd_reader_read_le32(req->input, &pe->opt32.LoaderFlags);
        rd_reader_read_le32(req->input, &pe->opt32.NumberOfRvaAndSizes);
        pe->section_align = pe->opt64.SectionAlignment;
    }

    for(int i = 0; i < PE_NUMBER_OF_DIRECTORY_ENTRIES; i++) {
        rd_reader_read_le32(req->input, &pe->data_dirs[i].VirtualAddress);
        rd_reader_read_le32(req->input, &pe->data_dirs[i].Size);
    }

    if(pe->fileheader.NumberOfSections) {
        pe->sections =
            rd_alloc0(pe->fileheader.NumberOfSections, sizeof(PESectionHeader));

        const u32 FIRST_SECTION =
            pe->dosheader.e_lfanew + pe->fileheader.SizeOfOptionalHeader + 0x18;

        rd_reader_seek(req->input, FIRST_SECTION);

        for(u16 i = 0; i < pe->fileheader.NumberOfSections; i++) {
            if(!pe_read_section_header(req->input, &pe->sections[i]))
                return false;
        }
    }

    pe->dotnet_version = pe_dotnet_get_major(req->input, pe);
    return !rd_reader_has_error(req->input);
}

static bool pe_load(RDLoader* ldr, RDContext* ctx) {
    rd_kb_load(ctx, "pe/types");

    PEFormat* pe = (PEFormat*)ldr;
    pe_set_bits(pe);
    pe_parse_richheader(ctx, pe);

    for(u16 i = 0; i < pe->fileheader.NumberOfSections; i++) {
        PESectionHeader* s = &pe->sections[i];

        u32 perm = 0;

        if(s->Characteristics & PE_SCN_MEM_EXECUTE ||
           (pe->entrypoint >= s->VirtualAddress &&
            pe->entrypoint < s->VirtualAddress + s->VirtualSize)) {
            perm |= RD_SP_X;
        }

        if(s->Characteristics & PE_SCN_MEM_READ) perm |= RD_SP_R;
        if(s->Characteristics & PE_SCN_MEM_WRITE) perm |= RD_SP_W;

        // if section name is exactly PE_SIZEOF_SHORT_NAME long
        // it needs a null terminator (not included in PE Header)
        char section_name[PE_SIZE_OF_SHORT_NAME + 1] = {0};
        memcpy(section_name, s->Name, PE_SIZE_OF_SHORT_NAME);

        RDAddress addr = pe->imagebase + s->VirtualAddress;

        u32 vsize = s->VirtualSize;
        if(!vsize) vsize = s->SizeOfRawData;

        if(pe->section_align) {
            u32 diff = vsize % pe->section_align;
            if(diff) vsize += pe->section_align - diff;
        }

        rd_map_segment_n(ctx, section_name, addr, vsize, perm);

        if(s->PointerToRawData) {
            rd_map_input_n(ctx, s->PointerToRawData, addr,
                           s->VirtualSize < s->SizeOfRawData
                               ? s->VirtualSize
                               : s->SizeOfRawData);
        }
    }

    if(pe->dotnet_version > 0) {
        RD_LOG_FAIL(".NET is not supported");
        return false;
    }

    pe_read_exports_dir(ctx, pe);
    pe_read_imports_dir(ctx, pe);
    pe_read_resources_dir(ctx, pe);
    pe_read_exceptions_dir(ctx, pe);
    pe_read_debug_dir(ctx, pe);
    pe_read_tls_dir(ctx, pe);
    pe_read_config_dir(ctx, pe);

    if(pe->fileheader.PointerToSymbolTable && pe->fileheader.NumberOfSymbols) {
        const RDCommandValue COFF_ARGS[] = {
            {RD_CMDARG_OFFSET, .off = pe->fileheader.PointerToSymbolTable},
            {RD_CMDARG_UINT, .u = pe->fileheader.NumberOfSymbols},
            {RD_CMDARG_VOID},
        };

        rd_command_run(ctx, "coff_parse", COFF_ARGS);
    }

    RDAddress ep;
    if(pe_from_rva(pe, pe->entrypoint, &ep))
        rd_set_entry_point(ctx, pe_norm(ctx, pe, ep), NULL);

    RD_LOG_INFO("Image Base: %" PRIX64, pe->imagebase);
    pe->classification = pe_classify(pe, ctx);

    switch(pe->classification.kind) {
        case PE_CLASS_VISUAL_BASIC_5:
        case PE_CLASS_VISUAL_BASIC_6:
            rd_analyzer_enable(ctx, "compiler_vb");
            break;

        case PE_CLASS_VISUAL_STUDIO:
            rd_analyzer_enable(ctx, "compiler_msvc_rtti");
            rd_analyzer_enable(ctx, "compiler_msvc_eh");
            break;

        default: break;
    }

    if(pe->classification.is_unicode) rd_set_scan_char16(ctx, true);

    pe_classify_print(&pe->classification);

    switch(pe->rich_header.status) {
        case PE_RICH_OK: {
            RD_LOG_INFO("Rich Header: valid, %zu records",
                        pe->rich_header.length);
            break;
        }

        case PE_RICH_ABSENT: RD_LOG_INFO("Rich Header: absent"); break;

        case PE_RICH_CORRUPTED: {
            RD_LOG_INFO(
                "Rich Header: present but checksum mismatch (modified?)");
            break;
        }
    }

    return true;
}

static void pe_destroy(RDLoader* ldr) {
    PEFormat* pe = (PEFormat*)ldr;

    rd_free(pe->rich_header.data);
    pe->rich_header.length = 0;

    rd_free(pe->sections);
    rd_free(ldr);
}

static const char* pe_get_name(const RDLoader* ldr) {
    const PEFormat* pe = (const PEFormat*)ldr;

    const char* pe_kind =
        pe->opt32.Magic == PE_NT_OPTIONAL_HDR64_MAGIC ? "PE32+" : "PE32";

    if(pe->dotnet_version > 0) {
        return rd_format("Microsoft .NET %d.x Executable (%s)",
                         pe->dotnet_version, pe_kind);
    }

    return rd_format("Portable Executable (%s)", pe_kind);
}

const RDLoaderPlugin PE_LOADER = {
    .id = "win_pe",
    .instance_size = sizeof(PEFormat),
    .get_name = pe_get_name,
    .get_processor = pe_get_processor,
    .destroy = pe_destroy,
    .parse = pe_parse,
    .load = pe_load,
};
