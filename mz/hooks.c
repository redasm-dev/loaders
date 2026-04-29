#include "hooks.h"

static void _x86_dos_int_hook(RDContext* ctx, RDInstruction* instr) {
    RD_UNUSED(ctx);

    if(instr->operands[0].kind != RD_OP_IMM) return;

    switch(instr->operands[0].imm) {
        case 0x20: // Terminate program
        case 0x27: // Terminate and stay resident
            instr->flow = RD_IF_STOP;
            break;

        default: break;
    }
}

void mz_register_dos_hooks(RDContext* ctx) {
    rd_register_instruction_hook(ctx, "x86.int", _x86_dos_int_hook);
}
