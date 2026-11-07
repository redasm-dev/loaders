#include "psx.h"

static const RDInstruction PSX_BIOS_STUB[] = {
    {
        .mnemonic = "addiu",
        .operands =
            {
                [0] = {.kind = RD_OP_REG},
                [1] = {.kind = RD_OP_REG},
                [2] = {.kind = RD_OP_IMM},
            },
    },
    {
        .mnemonic = "jr",
        .delay_slots = 1,
        .operands = {[0] = {.kind = RD_OP_REG}},
    },
    {
        .mnemonic = "addiu",
        .operands =
            {
                [0] = {.kind = RD_OP_REG},
                [1] = {.kind = RD_OP_REG},
                [2] = {.kind = RD_OP_IMM},
            },
    },
};

static const char* psx_bios_lookup(const RDDatum* kb, u8 sel, u8 fn) {
    const char* str = rd_format("bios.sel.%02x", sel);
    const char* sel_str = rd_datum_get_str(kb, str);
    if(!sel_str) return NULL;

    str = rd_format("%s.%02x", sel_str, fn);
    return rd_datum_get_str(kb, str);
}

static bool _psx_autorename_bioscalls(RDContext* ctx, const RDHookEvent* e,
                                      void* userdata) {
    RD_UNUSED(e);

    const RDDatum* kb = (const RDDatum*)userdata;
    RDAddressSlice functions = rd_get_all_functions(ctx);

    const RDAddress* addr;
    rd_slice_each(addr, functions) {
        RDInstruction instrs[3];
        if(!rd_decode_n(ctx, *addr, instrs, rd_count_of(instrs))) continue;

        if(!rd_instr_match_n(ctx, instrs, PSX_BIOS_STUB,
                             rd_count_of(PSX_BIOS_STUB)))
            continue;

        // jr must go through the register the first addiu
        if(instrs[0].operands[0].reg != instrs[1].operands[0].reg) continue;

        // second register operand must match
        if(instrs[0].operands[1].reg != instrs[2].operands[1].reg) continue;

        u64 sel = instrs[0].operands[2].imm, fn = instrs[2].operands[2].imm;
        if(sel > 0xFF || fn > 0xFF) continue;

        const char* name = psx_bios_lookup(kb, sel, fn);
        if(name) rd_library_name(ctx, *addr, name);
    }

    return true;
}

static bool _psx_render_cop2(RDContext* ctx, const RDHookEvent* e,
                             void* userdata) {
    RD_UNUSED(ctx);

    const RDDatum* kb = (const RDDatum*)userdata;
    RDRenderer* r = e->render_operand.renderer;
    u32 cmd = (u32)e->render_operand.instr->operands[e->render_operand.idx].imm;

    const char* name =
        rd_datum_get_str(kb, rd_format("gte.cmd.%02x", cmd & 0x3F));

    if(name) {
        rd_renderer_text(r, name, RD_THEME_LOCATION, RD_THEME_BACKGROUND);
        return true;
    }

    return false;
}

static void psx_setup(RDContext* ctx, RDPlatform* p) {
    RD_UNUSED(p);

    const RDDatum* bios_kb = rd_kb_load(ctx, "platform/psx/bios");

    if(bios_kb) {
        rd_register_hook(ctx, RD_HOOK_GENERAL, "redasm.symbols",
                         _psx_autorename_bioscalls, (void*)bios_kb);
    }
    else {
        RD_LOG_WARN("bios KB not found, BIOS calls won't be named");
    }

    const RDDatum* gte_kb = rd_kb_load(ctx, "platform/psx/gte");

    if(gte_kb) {
        rd_register_hook(ctx, RD_HOOK_RENDER_OPERAND, "mips.cop2",
                         _psx_render_cop2, (void*)gte_kb);
    }
    else {
        RD_LOG_WARN("gte KB not found, GTE commands won't be named");
    }
}

const RDPlatformPlugin PSX = {
    .id = "psx",
    .name = "PlayStation 1",
    .setup = psx_setup,
};
