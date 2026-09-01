#include "le.h"
#include "common/common.h"
#include "le/exports.h"
#include "le/format.h"
#include "le/objects.h"
#include "le/vxd.h"

static bool le_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    LEFormat* le = (LEFormat*)ldr;
    if(!mz_read_dos_header(req->input, &le->dosheader)) return false;

    bool is_le =
        mz_match_signature(req->input, &le->dosheader, MZ_LE_SIGNATURE);
    bool is_lx =
        mz_match_signature(req->input, &le->dosheader, MZ_LX_SIGNATURE);

    if(!is_le && !is_lx) return false;

    le->is_lx = is_lx;
    le->base = le->dosheader.e_lfanew;
    return le_read_header(req->input, &le->header);
}

static bool le_load(RDLoader* ldr, RDContext* ctx) {
    LEFormat* le = (LEFormat*)ldr;
    le->objects = le_objectslice_create(le);
    le->imports = le_importslice_create(le, ctx);

    le_report_module_type(le);
    le_report_cpu_type(le);
    le_report_os_type(le);

    if(!le_segments_load(le, ctx)) return false;

    le_exports_read(le, ctx);

    if(le->header.eip_obj) {
        RDAddress eip = le_seg_address(le, le->header.eip_obj, le->header.eip);
        rd_set_entry_point(ctx, eip, NULL);
    }

    if(le->header.esp_obj) {
        RDAddress esp = le_seg_address(le, le->header.esp_obj, le->header.esp);
        rd_auto_sregval(ctx, 0, "esp", esp);
    }

    if(le_is_vxd(le)) le_load_vxd(le, ctx);

    return true;
}

static void le_destroy(RDLoader* ldr) {
    LEFormat* le = (LEFormat*)ldr;
    le_objectslice_destroy(&le->objects);
    le_importslice_destroy(&le->imports);
    rd_free(le);
}

static const char* le_get_name(const RDLoader* ldr) {
    const LEFormat* le = (const LEFormat*)ldr;
    if(le_is_vxd(le)) return "Virtual Device Driver (VxD)";

    const char* t = le->is_lx ? "LX" : "LE";

    if((le->header.flags & LE_MOD_VDD) == LE_MOD_VDD)
        return rd_format("%s Virtual Device Driver", t);

    if((le->header.flags & LE_MOD_PDD) == LE_MOD_PDD)
        return rd_format("%s Physical Device Driver", t);

    if(le->header.flags & LE_MOD_DLL)
        return rd_format("%s Dynamic module (DLL)", t);

    return rd_format("%s Executable", t);
}

static const char* le_get_processor(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "x86_32";
}

const RDLoaderPlugin LE_LOADER = {
    .id = "mz_linear",
    .instance_size = sizeof(LEFormat),
    .get_name = le_get_name,
    .get_processor = le_get_processor,
    .destroy = le_destroy,
    .parse = le_parse,
    .load = le_load,
};
