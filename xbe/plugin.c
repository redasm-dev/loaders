#include "format.h"
#include <redasm/redasm.h>
#include <string.h>

#define XBE_GAME_REGION_NA 0x00000001
#define XBE_GAME_REGION_JAPAN 0x00000002
#define XBE_GAME_REGION_RESTOFWORLD 0x00000004
#define XBE_GAME_REGION_MANUFACTURING 0x80000000

#define XBE_REGION_BUFFER_SIZE 128

static void _xbe_read_kernel_thunks(const XBEFormat* xbe, RDContext* ctx) {
    u32 kt_va = xbe->kernel_thunk;
    if(!kt_va) return;

    u32 image_end = xbe->header.ImageBase + xbe->header.SizeOfImage;
    if(kt_va < xbe->header.ImageBase || kt_va >= image_end) return;

    rd_kb_load(ctx, "os/xbox/ordinals");

    RDAddress addr = (RDAddress)kt_va;

    while(addr + sizeof(u32) <= (RDAddress)image_end) {
        u32 slot;
        if(!rd_read_le32(ctx, addr, &slot) || !slot) break;

        u32 ordinal = slot & 0x7FFFFFFFU;
        rd_set_external_ord(ctx, addr, "XBoxKernel", ordinal, RD_EXT_IMPORTED);
        addr += sizeof(u32);
    }
}

static void _xbe_display_certificate(const XBECertificate* cert) {
    char* title = rd_alloc0(rd_count_of(cert->TitleName) + 1, sizeof(char));

    for(int i = 0; i < rd_count_of(cert->TitleName); i++) {
        if(!cert->TitleName[i]) break;
        title[i] = (char)(cert->TitleName[i] & 0xFF);
    }

    RD_LOG_INFO("Title: %s", title);

    char* regions = rd_alloc0(XBE_REGION_BUFFER_SIZE + 1, sizeof(char));

    if(cert->GameRegion & XBE_GAME_REGION_JAPAN)
        strncat(regions, "JAPAN", XBE_REGION_BUFFER_SIZE - strlen(regions) - 1);

    if(cert->GameRegion & XBE_GAME_REGION_NA) {
        if(*regions) {
            strncat(regions, ", ",
                    XBE_REGION_BUFFER_SIZE - strlen(regions) - 1);
        }

        strncat(regions, "NORTH AMERICA",
                XBE_REGION_BUFFER_SIZE - strlen(regions) - 1);
    }

    if(cert->GameRegion & XBE_GAME_REGION_RESTOFWORLD) {
        if(*regions) {
            strncat(regions, ", ",
                    XBE_REGION_BUFFER_SIZE - strlen(regions) - 1);
        }

        strncat(regions, "REST OF WORLD",
                XBE_REGION_BUFFER_SIZE - strlen(regions) - 1);
    }

    RD_LOG_INFO("Regions: %s", regions);

    rd_free(regions);
    rd_free(title);
}

static const char* xbe_get_name(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "XBox Executable";
}

static const char* xbe_get_processor(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "x86_32";
}

static bool xbe_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    XBEFormat* xbe = (XBEFormat*)ldr;

    u32 magic;
    if(!rd_reader_peek_le32(req->input, &magic) || magic != XBE_MAGIC)
        return false;

    if(!xbe_read_header(req->input, &xbe->header)) return false;

    if(!xbe->header.ImageBase) return false;
    if(!xbe->header.SizeOfImage || xbe->header.SizeOfImage > 0x10000000U)
        return false;

    if(!xbe->header.NumberOfSections) return false;
    if(xbe->header.NumberOfSections > XBE_MAX_SECTIONS) return false;

    if(xbe->header.CertificateAddress) {
        u32 off = xbe_va_to_off(xbe, xbe->header.CertificateAddress);

        if(off) {
            rd_reader_seek(req->input, off);
            xbe_read_certificate(req->input, &xbe->certificate);
        }
    }

    xbe->entry_point = xbe_decode_ep(xbe, &xbe->is_debug);
    xbe->kernel_thunk = xbe_decode_kt(xbe);
    return true;
}

static bool xbe_load(RDLoader* ldr, RDContext* ctx) {
    XBEFormat* xbe = (XBEFormat*)ldr;

    u32 sh_off = xbe_va_to_off(xbe, xbe->header.SectionHeadersAddress);
    if(!sh_off) return false;

    RDReader* r = rd_get_input_reader(ctx);
    rd_reader_seek(r, sh_off);

    for(u32 i = 0; i < xbe->header.NumberOfSections; i++) {
        XBESectionHeader shdr;
        if(!xbe_read_section(r, &shdr)) return false;

        if(!shdr.VirtualAddress || !shdr.VirtualSize) continue;

        rd_reader_save(r);
        const char* name = xbe_read_section_name(r, xbe, &shdr);
        rd_reader_restore(r);

        if(!rd_map_segment_n(ctx, name, shdr.VirtualAddress, shdr.VirtualSize,
                             xbe_segment_perm(&shdr)))
            continue;

        if(!shdr.RawOffset || !shdr.RawSize) continue;

        rd_map_input_n(ctx, shdr.RawOffset, shdr.VirtualAddress,
                       shdr.RawSize < shdr.VirtualSize ? shdr.RawSize
                                                       : shdr.VirtualSize);
    }

    _xbe_read_kernel_thunks(xbe, ctx);
    rd_set_entry_point(ctx, (RDAddress)xbe->entry_point, NULL);
    if(xbe->certificate.Size) _xbe_display_certificate(&xbe->certificate);
    return true;
}

static const RDLoaderPlugin XBE_LOADER = {
    .id = "xbox_xbe",
    .instance_size = sizeof(XBEFormat),
    .get_name = xbe_get_name,
    .get_processor = xbe_get_processor,
    .parse = xbe_parse,
    .load = xbe_load,
};

static void xbe_module_load(void) { rd_register_loader(&XBE_LOADER); }

RD_MODULE_EXPORT = {
    .api_version = RD_API_VERSION,
    .load = xbe_module_load,
};
