#include "biosfunc.h"

static const RDInstruction PSX_BIOS_STUB[] = {
    {
        .mnemonic = "li",
        .operands = {[0] = {.kind = RD_OP_REG}, [1] = {.kind = RD_OP_IMM}},
    },
    {
        .mnemonic = "jr",
        .delay_slots = 1,
        .operands = {[0] = {.kind = RD_OP_REG}},
    },
    {
        .mnemonic = "li",
        .operands = {[0] = {.kind = RD_OP_REG}, [1] = {.kind = RD_OP_IMM}},
    },
};

static const char* psx_bios_lookup(RDContext* ctx, u8 sel, u8 fn) {
    const RDDatum* kb = rd_kb_load(ctx, "psx/bios");

    const char* str = rd_format("bios.sel.%02x", sel);
    const char* sel_str = rd_datum_get_str(kb, str);
    if(!sel_str) return NULL;

    str = rd_format("%s.%02x", sel_str, fn);
    return rd_datum_get_str(kb, str);
}

void psx_bios_autorename_hook(RDContext* ctx, const RDHookEvent* e,
                              void* userdata) {
    RD_UNUSED(e);
    RD_UNUSED(userdata);

    RDAddressSlice functions = rd_get_all_functions(ctx);

    const RDAddress* addr;
    rd_slice_each(addr, functions) {
        RDInstruction instrs[3];
        if(!rd_decode_n(ctx, *addr, instrs, rd_count_of(instrs))) continue;

        if(!rd_instr_match_n(ctx, instrs, PSX_BIOS_STUB,
                             rd_count_of(PSX_BIOS_STUB)))
            continue;

        // jr must go through the register the first li loaded
        if(instrs[0].operands[0].reg != instrs[1].operands[0].reg) continue;

        const char* name = psx_bios_lookup(ctx, instrs[0].operands[1].imm,
                                           instrs[2].operands[1].imm);

        if(name) rd_library_name(ctx, *addr, name);
    }
}
