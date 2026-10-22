#include "vxd.h"
#include "le/exports.h"
#include <inttypes.h>
#include <string.h>

bool le_is_vxd(const LEFormat* le) {
    return !le->is_lx && le->header.os_type == LE_MOD_OS_WIN386 &&
           (le->header.flags & LE_MOD_VDD) == LE_MOD_VDD;
}

static bool _le_read_vxd_ddb(RDReader* r, VxDDescBlock* v) {
    rd_reader_read_le32(r, &v->DDB_Next);
    rd_reader_read_le16(r, &v->DDB_SDK_Version);
    rd_reader_read_le16(r, &v->DDB_Req_Device_Number);
    rd_reader_read_byte(r, &v->DDB_Dev_Major_Version);
    rd_reader_read_byte(r, &v->DDB_Dev_Minor_Version);
    rd_reader_read_le16(r, &v->DDB_Flags);
    rd_reader_read_exact(r, &v->DDB_Name, sizeof(v->DDB_Name));
    rd_reader_read_le32(r, &v->DDB_Init_Order);
    rd_reader_read_le32(r, &v->DDB_Control_Proc);
    rd_reader_read_le32(r, &v->DDB_V86_API_Proc);
    rd_reader_read_le32(r, &v->DDB_PM_API_Proc);
    rd_reader_read_le32(r, &v->DDB_V86_API_CSIP);
    rd_reader_read_le32(r, &v->DDB_PM_API_CSIP);
    rd_reader_read_le32(r, &v->DDB_Reference_Data);
    rd_reader_read_le32(r, &v->DDB_Service_Table_Ptr);
    rd_reader_read_le32(r, &v->DDB_Service_Table_Size);
    rd_reader_read_le32(r, &v->DDB_Win32_Service_Table);
    rd_reader_read_le32(r, &v->DDB_Prev);
    rd_reader_read_le32(r, &v->DDB_Size);
    rd_reader_read_le32(r, &v->DDB_Reserved1);
    rd_reader_read_le32(r, &v->DDB_Reserved2);
    rd_reader_read_le32(r, &v->DDB_Reserved3);

    return !rd_reader_has_error(r);
}

void le_load_vxd(const LEFormat* le, RDContext* ctx) {
    RDAddress ddb_addr = le_exports_entry(le, 1, ctx);
    if(!ddb_addr) return;

    rd_kb_load(ctx, "platform/win16/types");
    rd_library_type(ctx, ddb_addr, "VxDDescBlock", 0, RD_TYPE_NONE);

    RDReader* r = rd_get_reader(ctx);
    rd_reader_seek(r, ddb_addr);
    VxDDescBlock vxd;
    if(!_le_read_vxd_ddb(r, &vxd)) return;

    // get driver name
    char vxd_name[sizeof(vxd.DDB_Name) + 1] = {0};
    memcpy(vxd_name, vxd.DDB_Name, sizeof(vxd.DDB_Name));

    for(int i = sizeof(vxd.DDB_Name); i-- > 0;) {
        if(vxd_name[i] != ' ') break;
        vxd_name[i] = '\0';
    }

    if(vxd.DDB_Control_Proc) {
        rd_set_function(ctx, vxd.DDB_Control_Proc);
        rd_library_name(ctx, vxd.DDB_Control_Proc,
                        rd_format("%s_Control", vxd_name));

        rd_set_external(ctx, vxd.DDB_Control_Proc, NULL, RD_EXT_EXPORTED);
    }

    if(vxd.DDB_V86_API_Proc && vxd.DDB_V86_API_Proc == vxd.DDB_PM_API_Proc) {
        rd_set_function(ctx, vxd.DDB_V86_API_Proc);
        rd_library_name(ctx, vxd.DDB_V86_API_Proc,
                        rd_format("%s_V86_PM_API", vxd_name));
    }
    else {
        if(vxd.DDB_V86_API_Proc) {
            rd_set_function(ctx, vxd.DDB_V86_API_Proc);
            rd_library_name(ctx, vxd.DDB_V86_API_Proc,
                            rd_format("%s_V86_API", vxd_name));
        }
        if(vxd.DDB_PM_API_Proc) {
            rd_set_function(ctx, vxd.DDB_PM_API_Proc);
            rd_library_name(ctx, vxd.DDB_PM_API_Proc,
                            rd_format("%s_PM_API", vxd_name));
        }
    }

    if(vxd.DDB_Service_Table_Ptr && vxd.DDB_Service_Table_Size) {
        for(u32 i = 0; i < vxd.DDB_Service_Table_Size; i++) {
            RDAddress svc_ptr_addr =
                vxd.DDB_Service_Table_Ptr + (i * sizeof(u32));

            const char* svc_name =
                rd_format("%s_service_ptr_%" PRIu32, vxd_name, i);
            rd_library_name(ctx, svc_ptr_addr, svc_name);
            rd_library_type(ctx, svc_ptr_addr, "u32", 0, RD_TYPE_PTR);

            rd_reader_seek(r, svc_ptr_addr);

            u32 svc_addr;
            if(rd_reader_read_le32(r, &svc_addr)) {
                rd_set_function(ctx, (RDAddress)svc_addr);
                rd_auto_name(ctx, svc_addr,
                             rd_format("%s_service_%" PRIu32, vxd_name, i));
            }
        }
    }
}
