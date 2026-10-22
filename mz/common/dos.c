#include "dos.h"

// https://www.stanislavs.org/helppc/int_21.html

static void _x86_read_com_string(RDContext* ctx, const RDInstruction* instr,
                                 RDAddress addr, u8 term) {
    usize count = 0;
    u8 b;

    while(rd_read_byte(ctx, addr + count, &b) && b != term)
        count++;

    count++; // include terminator

    if(count > 1) {
        rd_add_xref(ctx, instr->address, addr, RD_DR_READ);
        rd_library_type(ctx, addr, "char", count, RD_TYPE_NONE);
    }
}

static void _x86_decode_dos_21h(RDContext* ctx, RDInstruction* instr) {
    RDRegValue ah;
    if(!rd_get_regval(ctx, "ah", &ah)) return;

    switch(ah) {
        case 0x4c: // exit
            instr->flow = RD_IF_STOP;
            break;

        default: break;
    }
}

static void _x86_emulate_dos_21h(RDContext* ctx, const RDInstruction* instr) {
    RDRegValue ah;
    if(!rd_get_regval(ctx, "ah", &ah)) return;

    switch(ah) {
        case 0x09: { // print
            RDRegValue dx;
            if(!rd_get_regval(ctx, "dx", &dx)) return;
            _x86_read_com_string(ctx, instr, (RDAddress)dx, '$');
            break;
        }

        case 0x3d: { // file open
            RDRegValue dx;
            if(!rd_get_regval(ctx, "dx", &dx)) return;
            _x86_read_com_string(ctx, instr, (RDAddress)dx, '\0');
            break;
        }

        case 0x40: { // file write (bx=1 stdout, bx=2 stderr)
            RDRegValue bx, cx, dx;
            if(!rd_get_regval(ctx, "bx", &bx)) return;
            if(bx != 1 && bx != 2)
                break; // only stdout/stderr are likely strings

            if(!rd_get_regval(ctx, "cx", &cx)) return;
            if(!rd_get_regval(ctx, "dx", &dx)) return;

            if(cx > 0) {
                rd_add_xref(ctx, instr->address, (RDAddress)dx, RD_DR_READ);
                rd_library_type(ctx, (RDAddress)dx, "char", (usize)cx,
                                RD_TYPE_NONE);
            }

            break;
        }

        default: break;
    }
}

static void _x86_dos_int_hook(RDContext* ctx, const RDHookEvent* e,
                              void* userdata) {
    RD_UNUSED(userdata);

    if(e->kind == RD_HOOK_DECODE) {
        if(e->decode.instr->operands[0].kind != RD_OP_CNST) return;

        switch(e->decode.instr->operands[0].cnst) {
            case 0x21: _x86_decode_dos_21h(ctx, e->decode.instr); break;
            case 0x20: // Terminate program
            case 0x27: // Terminate and stay resident
                e->decode.instr->flow = RD_IF_STOP;
                break;

            default: break;
        }
    }
    else if(e->kind == RD_HOOK_EMULATE) {
        if(e->emulate.instr->operands[0].kind != RD_OP_CNST) return;

        switch(e->emulate.instr->operands[0].cnst) {
            case 0x21: _x86_emulate_dos_21h(ctx, e->emulate.instr); break;
            default: break;
        }
    }
}

static void dos_setup(RDContext* ctx, RDPlatform* p) {
    RD_UNUSED(p);

    rd_register_hook(ctx, RD_HOOK_DECODE, "x86.int", _x86_dos_int_hook, NULL);
    rd_register_hook(ctx, RD_HOOK_EMULATE, "x86.int", _x86_dos_int_hook, NULL);
}

const RDPlatformPlugin DOS = {
    .id = "ms_dos",
    .name = "MS-DOS",
    .setup = dos_setup,
};
