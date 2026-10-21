#include "sm83.h"
#include "sm83/tables.h"
#include <string.h>

#define SM83_REGISTERS(X)                                                      \
    X(SM83_OP_DE, "de")                                                        \
    X(SM83_OP_BC, "bc")                                                        \
    X(SM83_OP_SP, "sp")                                                        \
    X(SM83_OP_HL, "hl")                                                        \
    X(SM83_OP_A, "a")                                                          \
    X(SM83_OP_B, "b")                                                          \
    X(SM83_OP_C, "c")                                                          \
    X(SM83_OP_D, "d")                                                          \
    X(SM83_OP_E, "e")                                                          \
    X(SM83_OP_L, "l")                                                          \
    X(SM83_OP_H, "h")

#define SM83_REG_NAME2ID_X(id, reg_name)                                       \
    if(!strcmp(name, reg_name)) return id;

#define SM83_REG_ID2NAME_X(id, reg_name)                                       \
    case id: return reg_name;

#define SM83_REG_CASE_X(id, reg_name)                                          \
    case id:                                                                   \
        op->kind = RD_OP_REG;                                                  \
        op->reg = id;                                                          \
        break;

typedef enum {
    SM83_OP_KIND_COND = RD_OP_USERBASE,
    SM83_OP_KIND_IND_REG,
    SM83_OP_KIND_SYS,
} SM83Operands;

static inline RDAddress _sm83_pc(const RDInstruction* instr,
                                 const usize* cursor) {
    return instr->address + *cursor;
}

static const char* _sm83_get_reg_name(SM83OperandType op) {
    switch(op) {
        SM83_REGISTERS(SM83_REG_ID2NAME_X)
        default: break;
    }

    return NULL;
}

static SM83OperandType _sm83_get_reg_id(const char* name) {
    if(!name) return SM83_OP_NONE;
    SM83_REGISTERS(SM83_REG_NAME2ID_X)
    return SM83_OP_NONE;
}

static bool _sm83_decode_op(RDContext* ctx, RDInstruction* instr,
                            const SM83Instruction* sm83, int idx,
                            usize* cursor) {
    if(sm83->operands[idx] == SM83_OP_NONE) return true;

    RDOperand* op = &instr->operands[idx];
    op->kind = RD_OP_STUB; // stub and override later

    switch(sm83->operands[idx]) {
        SM83_REGISTERS(SM83_REG_CASE_X)

        case SM83_OP_IND_BC:
        case SM83_OP_IND_C:
        case SM83_OP_IND_DE:
        case SM83_OP_IND_HL:
            op->kind = SM83_OP_KIND_IND_REG;
            op->reg = sm83->operands[idx];
            break;

        case SM83_OP_COND_Z:
        case SM83_OP_COND_NZ:
        case SM83_OP_COND_C:
        case SM83_OP_COND_NC:
            op->kind = SM83_OP_KIND_COND;
            op->reg = sm83->operands[idx];
            break;

        case SM83_OP_N16: {
            u16 imm;
            if(!rd_read_le16(ctx, _sm83_pc(instr, cursor), &imm)) return false;

            *cursor += sizeof(u16);

            op->kind = RD_OP_CNST;
            op->cnst = imm;
            break;
        }

            // clang-format off
        case SM83_OP_0: op->kind = RD_OP_CNST; op->cnst = 0; break;
        case SM83_OP_1: op->kind = RD_OP_CNST; op->cnst = 1; break;
        case SM83_OP_2: op->kind = RD_OP_CNST; op->cnst = 2; break;
        case SM83_OP_3: op->kind = RD_OP_CNST; op->cnst = 3; break;
        case SM83_OP_4: op->kind = RD_OP_CNST; op->cnst = 4; break;
        case SM83_OP_5: op->kind = RD_OP_CNST; op->cnst = 5; break;
        case SM83_OP_6: op->kind = RD_OP_CNST; op->cnst = 6; break;
        case SM83_OP_7: op->kind = RD_OP_CNST; op->cnst = 7; break;

        case SM83_OP_00H: op->kind = SM83_OP_KIND_SYS; op->cnst = 0x00; break;
        case SM83_OP_08H: op->kind = SM83_OP_KIND_SYS; op->cnst = 0x08; break;
        case SM83_OP_10H: op->kind = SM83_OP_KIND_SYS; op->cnst = 0x10; break;
        case SM83_OP_18H: op->kind = SM83_OP_KIND_SYS; op->cnst = 0x18; break;
        case SM83_OP_20H: op->kind = SM83_OP_KIND_SYS; op->cnst = 0x20; break;
        case SM83_OP_28H: op->kind = SM83_OP_KIND_SYS; op->cnst = 0x28; break;
        case SM83_OP_30H: op->kind = SM83_OP_KIND_SYS; op->cnst = 0x30; break;
        case SM83_OP_38H: op->kind = SM83_OP_KIND_SYS; op->cnst = 0x38; break;
            // clang-format on

        case SM83_OP_E8: {
            u8 b;
            if(!rd_read_byte(ctx, _sm83_pc(instr, cursor), &b)) return false;

            (*cursor)++;

            op->kind = RD_OP_ADDR;
            op->addr = _sm83_pc(instr, cursor) + (i8)b;
            break;
        }

        case SM83_OP_IND_A16: {
            u16 addr;
            if(!rd_read_le16(ctx, _sm83_pc(instr, cursor), &addr)) return false;

            *cursor += sizeof(u16);

            op->kind = RD_OP_MEM;
            op->mem = (RDAddress)addr;
            break;
        }

        case SM83_OP_IND_A8: {
            u8 b;
            if(!rd_read_byte(ctx, _sm83_pc(instr, cursor), &b)) return false;

            (*cursor)++;

            op->kind = RD_OP_ADDR;
            op->addr = 0xFF00 | b;
            break;
        }

        case SM83_OP_A16: {
            u16 addr;
            if(!rd_read_le16(ctx, _sm83_pc(instr, cursor), &addr)) return false;

            *cursor += sizeof(u16);

            op->kind = RD_OP_ADDR;
            op->addr = (RDAddress)addr;
            break;
        }

        case SM83_OP_N8: {
            u8 b;
            if(!rd_read_byte(ctx, _sm83_pc(instr, cursor), &b)) return false;

            (*cursor)++;

            op->kind = RD_OP_CNST;
            op->cnst = b;
            break;
        }

        case SM83_OP_AF: break;

        case SM83_OP_NONE: break;
        default: return false;
    }

    return true;
}

static void _sm83_categorize(RDInstruction* instr,
                             const SM83Instruction* sm83_instr) {
    switch(instr->id) {
        case SM83_INSTR_NOP: instr->flow = RD_IF_NOP; break;
        case SM83_INSTR_JP: instr->flow = RD_IF_JUMP; break;
        case SM83_INSTR_RETI: instr->flow = RD_IF_STOP; break;

        case SM83_INSTR_CALL: {
            instr->flow = sm83_instr->operands[1] != SM83_OP_NONE
                              ? RD_IF_CALL_COND
                              : RD_IF_CALL;
            break;
        }

        case SM83_INSTR_JR: {
            instr->flow = sm83_instr->operands[1] != SM83_OP_NONE
                              ? RD_IF_JUMP_COND
                              : RD_IF_JUMP;
            break;
        }

        case SM83_INSTR_RET: {
            if(sm83_instr->operands[0] == SM83_OP_NONE)
                instr->flow = RD_IF_STOP;
            break;
        }

        case SM83_INSTR_HALT:
            instr->no_ret = true;
            instr->flow = RD_IF_STOP;
            break;

        default: break;
    }
}

static const char* sm83_get_mnemonic(const RDInstruction* instr,
                                     RDProcessor* p) {
    RD_UNUSED(p);
    return instr->mnemonic;
}

static bool sm83_query_reg(RDQueryReg* q, RDProcessor* p) {
    RD_UNUSED(p);

    if(q->kind == RD_QUERY_REG_BY_ID) {
        q->name = _sm83_get_reg_name(q->id);
        if(!q->name) return NULL;
    }

    else if(q->kind == RD_QUERY_REG_BY_NAME) {
        q->id = _sm83_get_reg_id(q->name);
        if(q->id == SM83_OP_NONE) return false;
    }
    else {
        return false;
    }

    if(q->want & RD_QUERY_REG_WANT_MASK) {
    }

    if(q->want & RD_QUERY_REG_WANT_CANONICAL) {
    }

    return true;
}

static void sm83_decode(RDContext* ctx, RDInstruction* instr, RDProcessor* p) {
    RD_UNUSED(p);

    u8 op;
    if(!rd_read_byte(ctx, instr->address, &op)) return;

    usize cursor = 1;
    u8 prefix = op; // might be a prefix
    const SM83Instruction* sel_table = NULL;

    switch(op) {
        case 0xCB: {
            if(!rd_read_byte(ctx, instr->address + cursor, &op)) return;
            sel_table = SM83_TABLE_CB;
            cursor++;
            break;
        }

        default: {
            sel_table = SM83_TABLE_NONE;
            break;
        }
    }

    if(!sel_table) return;

    const SM83Instruction* sm83_instr = &sel_table[op];
    if(sm83_instr->id == SM83_INSTR_INVALID) return;

    if(!_sm83_decode_op(ctx, instr, sm83_instr, 0, &cursor)) return;
    if(!_sm83_decode_op(ctx, instr, sm83_instr, 1, &cursor)) return;
    if(!_sm83_decode_op(ctx, instr, sm83_instr, 2, &cursor)) return;

    instr->id = sm83_instr->id;
    instr->mnemonic = sm83_instr->mnemonic;
    instr->length = sm83_instr->bytes;
    _sm83_categorize(instr, sm83_instr);
}

static void sm83_emulate(RDContext* ctx, const RDInstruction* instr,
                         RDProcessor* p) {
    RD_UNUSED(p);

    rd_foreach_operand(i, op, instr) {
        if(op->kind != RD_OP_ADDR) continue;

        if(rd_instr_is_jump(instr))
            rd_add_xref(ctx, instr->address, op->addr, RD_CR_JUMP);
        else if(rd_instr_is_call(instr))
            rd_add_xref(ctx, instr->address, op->addr, RD_CR_CALL);
        else
            rd_add_xref(ctx, instr->address, op->addr, RD_DR_ADDRESS);
    }

    if(rd_instr_can_flow(instr)) rd_flow(ctx, instr->address + instr->length);
}

static bool sm83_render_operand(RDRenderer* r, const RDInstruction* instr,
                                int idx, RDProcessor* p) {
    RD_UNUSED(p);

    const RDOperand* op = &instr->operands[idx];

    if(op->kind == SM83_OP_KIND_COND) {
        switch(op->reg) {
            case SM83_OP_COND_Z: {
                rd_renderer_text(r, "z", RD_THEME_FAIL, RD_THEME_DEFAULT);
                return true;
            }

            case SM83_OP_COND_NZ: {
                rd_renderer_text(r, "nz", RD_THEME_FAIL, RD_THEME_DEFAULT);
                return true;
            }

            case SM83_OP_COND_C: {
                rd_renderer_text(r, "c", RD_THEME_FAIL, RD_THEME_DEFAULT);
                return true;
            }

            case SM83_OP_COND_NC: {
                rd_renderer_text(r, "nc", RD_THEME_FAIL, RD_THEME_DEFAULT);
                return true;
            }

            default: break;
        }

        return false;
    }

    if(op->kind == SM83_OP_KIND_SYS) {
        rd_renderer_num(r, (i64)op->cnst, 16, 2, RD_NUM_NOADDR);
        rd_renderer_text(r, "h", RD_THEME_NUMBER, RD_THEME_DEFAULT);
        return true;
    }

    if(op->kind == SM83_OP_KIND_IND_REG) {
        rd_renderer_norm(r, "[");

        switch(op->reg) {
            case SM83_OP_IND_BC:
                rd_renderer_text(r, "bc", RD_THEME_REG, RD_THEME_DEFAULT);
                break;

            case SM83_OP_IND_C:
                rd_renderer_text(r, "c", RD_THEME_REG, RD_THEME_DEFAULT);
                break;

            case SM83_OP_IND_DE:
                rd_renderer_text(r, "de", RD_THEME_REG, RD_THEME_DEFAULT);
                break;

            case SM83_OP_IND_HL:
                rd_renderer_text(r, "hl", RD_THEME_REG, RD_THEME_DEFAULT);
                break;

            default: break;
        }

        rd_renderer_norm(r, "]");
        return true;
    }

    return false;
}

const RDProcessorPlugin SM83 = {
    .id = "sm83",
    .name = "Sharp SM83",
    .ptr_size = sizeof(u16),
    .get_mnemonic = sm83_get_mnemonic,
    .query_reg = sm83_query_reg,
    .decode = sm83_decode,
    .emulate = sm83_emulate,
    .render_operand = sm83_render_operand,
};
