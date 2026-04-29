#include "pe.h"
#include "format.h"
#include "pe/dirs/debug.h"
#include "pe/dirs/exceptions.h"
#include "pe/dirs/exports.h"
#include "pe/dirs/imports.h"
#include "pe/dirs/resources.h"
#include <stdlib.h>

static bool pe_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    PEFormat* pe = (PEFormat*)ldr;
    if(!mz_read_dos_header(req->input, &pe->dosheader)) return false;

    rd_reader_seek(req->input, pe->dosheader.e_lfanew);
    if(!rd_reader_expect_le32(req->input, IMAGE_NT_SIGNATURE)) return false;

    rd_reader_read_le16(req->input, &pe->fileheader.Machine);
    rd_reader_read_le16(req->input, &pe->fileheader.NumberOfSections);
    rd_reader_read_le32(req->input, &pe->fileheader.TimeDateStamp);
    rd_reader_read_le32(req->input, &pe->fileheader.PointerToSymbolTable);
    rd_reader_read_le32(req->input, &pe->fileheader.NumberOfSymbols);
    rd_reader_read_le16(req->input, &pe->fileheader.SizeOfOptionalHeader);
    rd_reader_read_le16(req->input, &pe->fileheader.Characteristics);
    if(rd_reader_has_error(req->input)) return false;

    rd_reader_read_le16(req->input, &pe->opt32.Magic);
    rd_reader_read_u8(req->input, &pe->opt32.MajorLinkerVersion);
    rd_reader_read_u8(req->input, &pe->opt32.MinorLinkerVersion);
    rd_reader_read_le32(req->input, &pe->opt32.SizeOfCode);
    rd_reader_read_le32(req->input, &pe->opt32.SizeOfInitializedData);
    rd_reader_read_le32(req->input, &pe->opt32.SizeOfUninitializedData);
    rd_reader_read_le32(req->input, &pe->opt32.AddressOfEntryPoint);
    rd_reader_read_le32(req->input, &pe->opt32.BaseOfCode);

    if(pe->opt32.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        rd_reader_read_le32(req->input, &pe->opt32.BaseOfData);
        rd_reader_read_le32(req->input, &pe->opt32.ImageBase);
        pe->imagebase = pe->opt32.ImageBase;
        pe->entrypoint = pe->opt32.AddressOfEntryPoint;
    }
    else if(pe->opt32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
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

    if(pe->opt32.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        rd_reader_read_le32(req->input, &pe->opt32.SizeOfStackReserve);
        rd_reader_read_le32(req->input, &pe->opt32.SizeOfStackCommit);
        rd_reader_read_le32(req->input, &pe->opt32.SizeOfHeapReserve);
        rd_reader_read_le32(req->input, &pe->opt32.SizeOfHeapCommit);
        rd_reader_read_le32(req->input, &pe->opt32.LoaderFlags);
        rd_reader_read_le32(req->input, &pe->opt32.NumberOfRvaAndSizes);
    }
    else if(pe->opt32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        rd_reader_read_le64(req->input, &pe->opt64.SizeOfStackReserve);
        rd_reader_read_le64(req->input, &pe->opt64.SizeOfStackCommit);
        rd_reader_read_le64(req->input, &pe->opt64.SizeOfHeapReserve);
        rd_reader_read_le64(req->input, &pe->opt64.SizeOfHeapCommit);
        rd_reader_read_le32(req->input, &pe->opt32.LoaderFlags);
        rd_reader_read_le32(req->input, &pe->opt32.NumberOfRvaAndSizes);
    }

    for(int i = 0; i < IMAGE_NUMBEROF_DIRECTORY_ENTRIES; i++) {
        rd_reader_read_le32(req->input, &pe->datadir[i].VirtualAddress);
        rd_reader_read_le32(req->input, &pe->datadir[i].Size);
    }

    return !rd_reader_has_error(req->input);
}

static bool pe_load(RDLoader* ldr, RDContext* ctx) {
    PEFormat* pe = (PEFormat*)ldr;
    RDReader* r = rd_get_input_reader(ctx);

    pe_register_exports_types(ctx);
    pe_register_imports_types(ctx);
    pe_register_resources_types(ctx);
    pe_register_exceptions_types(ctx);
    pe_register_debug_types(ctx);

    for(usize i = 0; i < pe->fileheader.NumberOfSections; i++) {
        ImageSectionHeader s;
        if(!pe_read_section_header(pe, r, i, &s)) continue;

        u32 perm = 0;

        if(s.Characteristics & IMAGE_SCN_MEM_EXECUTE ||
           (pe->entrypoint >= s.VirtualAddress &&
            pe->entrypoint < s.VirtualAddress + s.VirtualSize)) {
            perm |= RD_SP_X;
        }

        if(s.Characteristics & IMAGE_SCN_MEM_READ) perm |= RD_SP_R;
        if(s.Characteristics & IMAGE_SCN_MEM_WRITE) perm |= RD_SP_W;

        RDAddress addr = pe->imagebase + s.VirtualAddress;
        rd_map_segment_n(ctx, s.Name, addr, s.VirtualSize, perm);

        rd_map_input_n(ctx, s.PointerToRawData, addr,
                       s.VirtualSize < s.SizeOfRawData ? s.VirtualSize
                                                       : s.SizeOfRawData);
    }

    pe_read_exports(ctx, pe);
    pe_read_imports(ctx, pe);
    pe_read_resources(ctx, pe);
    pe_read_exceptions(ctx, pe);
    pe_read_debug(ctx, pe);

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
        rd_set_entry_point(ctx, ep, "PE_EntryPoint");

    return true;
}

static RDLoader* pe_create(const RDLoaderPlugin* plugin) {
    RD_UNUSED(plugin);
    return calloc(1, sizeof(PEFormat));
}

static void pe_destroy(RDLoader* self) { free(self); }

static const char* pe_get_processor(RDLoader* ldr, const RDContext* ctx) {
    RD_UNUSED(ctx);

    PEFormat* pe = (PEFormat*)ldr;

    switch(pe->fileheader.Machine) {
        case IMAGE_FILE_MACHINE_ARM: {
            if(pe->opt32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
                return "arm64_le";
            return "arm32_le";
        }

        case IMAGE_FILE_MACHINE_AMD64: return "x86_64";
        case IMAGE_FILE_MACHINE_I386: return "x86_32";
        default: break;
    }

    return NULL;
}

const RDLoaderPlugin PE_LOADER = {
    .level = RD_API_LEVEL,
    .id = "mz_pe",
    .name = "Portable Executable",
    .create = pe_create,
    .destroy = pe_destroy,
    .parse = pe_parse,
    .load = pe_load,
    .get_processor = pe_get_processor,
};
