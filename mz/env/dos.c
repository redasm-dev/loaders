#include "dos.h"

static void x86_dos_decode(const RDContext* ctx, RDInstruction* instr,
                           RDProcessor* p) {
    RD_UNUSED(p);
    rd_processor_parent_decode(ctx, instr);

    if(rd_instruction_equals(ctx, instr, "int")) {
        switch(instr->operands[0].imm) {
            case 0x20: // Terminate program
            case 0x27: // Terminate and stay resident
                instr->flow = RD_IF_STOP;
                break;

            default: break;
        }
    }
}

const RDProcessorPlugin X86_16_DOS = {
    .level = RD_API_LEVEL,
    .id = "x86_16_dos",
    .name = "X86_16 (DOS)",
    .parent = "x86_16_real",
    .decode = x86_dos_decode,
};
