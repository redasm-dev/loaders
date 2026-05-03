#include <redasm/redasm.h>
#include <stdlib.h>
#include <string.h>

#define PSXEXE_SIGNATURE_SIZE 8
#define PSXEXE_SIGNATURE "PS-X EXE"
#define PSXEXE_TEXT_OFFSET 0x00000800
#define PSX_USER_RAM_START 0x80000000
#define PSX_USER_RAM_END 0x80200000

typedef struct PsxExeHeader {
    char id[PSXEXE_SIGNATURE_SIZE];
    u32 text, data;
    u32 pc0, gp0;
    u32 t_addr, t_size;
    u32 d_addr, d_size;
    u32 b_addr, b_size;
    u32 s_addr, s_size;
    u32 SavedSP, SavedFP, SavedGP, SavedRA, SavedS0;
} PsxExeHeader;

static RDLoader* psx_create(const RDLoaderPlugin* plugin) {
    RD_UNUSED(plugin);
    return calloc(1, sizeof(PsxExeHeader));
}

static void psx_destroy(RDLoader* ldr) { free(ldr); }

static bool psx_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    PsxExeHeader* h = (PsxExeHeader*)ldr;

    rd_reader_read(req->input, h->id, sizeof(h->id));
    rd_reader_read_le32(req->input, &h->text);
    rd_reader_read_le32(req->input, &h->data);
    rd_reader_read_le32(req->input, &h->pc0);
    rd_reader_read_le32(req->input, &h->gp0);
    rd_reader_read_le32(req->input, &h->t_addr);
    rd_reader_read_le32(req->input, &h->t_size);
    rd_reader_read_le32(req->input, &h->d_addr);
    rd_reader_read_le32(req->input, &h->d_size);
    rd_reader_read_le32(req->input, &h->b_addr);
    rd_reader_read_le32(req->input, &h->b_size);
    rd_reader_read_le32(req->input, &h->s_addr);
    rd_reader_read_le32(req->input, &h->s_size);
    rd_reader_read_le32(req->input, &h->SavedSP);
    rd_reader_read_le32(req->input, &h->SavedFP);
    rd_reader_read_le32(req->input, &h->SavedGP);
    rd_reader_read_le32(req->input, &h->SavedRA);
    rd_reader_read_le32(req->input, &h->SavedS0);

    return !rd_reader_has_error(req->input) &&
           !strncmp(h->id, PSXEXE_SIGNATURE, sizeof(h->id));
}

static bool psx_load(RDLoader* ldr, RDContext* ctx) {
    PsxExeHeader* h = (PsxExeHeader*)ldr;

    if(!rd_map_segment(ctx, "RAM", PSX_USER_RAM_START, PSX_USER_RAM_END,
                       RD_SP_RWX)) {
        return false;
    }

    if(!rd_map_input_n(ctx, PSXEXE_TEXT_OFFSET, h->t_addr, h->t_size))
        return false;

    if(h->gp0) {
        rd_library_name(ctx, h->gp0, "PSX_gp0");
        rd_library_regval(ctx, h->pc0, "$gp", h->gp0);
    }

    if(h->s_addr) {
        rd_library_name(ctx, h->s_addr, "PSX_sp0");
        rd_library_regval(ctx, h->pc0, "$sp", h->s_addr);
    }

    return rd_set_entry_point(ctx, h->pc0, "PSX_EntryPoint");
}

static const char* psx_get_processor(RDLoader* ldr, const RDContext* ctx) {
    RD_UNUSED(ldr);
    RD_UNUSED(ctx);
    return "mips32_le";
}

static const RDLoaderPlugin PSX_EXE_LOADER = {
    .level = RD_API_LEVEL,
    .id = "psx_exe",
    .name = "PS-X EXE Loader",
    .create = psx_create,
    .destroy = psx_destroy,
    .parse = psx_parse,
    .load = psx_load,
    .get_processor = psx_get_processor,
};

void rd_plugin_create(void) { rd_register_loader(&PSX_EXE_LOADER); }
