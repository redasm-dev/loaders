#include "sm83.h"
#include "sm83/tables.h"
#include <string.h>

#define SM83_REGISTERS(X)                                                      \
    X(SM83_OP_AF, "af", false)                                                 \
    X(SM83_OP_BC, "bc", false)                                                 \
    X(SM83_OP_DE, "de", false)                                                 \
    X(SM83_OP_HL, "hl", false)                                                 \
    X(SM83_OP_SP, "sp", false)                                                 \
    X(SM83_OP_A, "a", false)                                                   \
    X(SM83_OP_B, "b", false)                                                   \
    X(SM83_OP_C, "c", false)                                                   \
    X(SM83_OP_D, "d", false)                                                   \
    X(SM83_OP_E, "e", false)                                                   \
    X(SM83_OP_L, "l", false)                                                   \
    X(SM83_OP_H, "h", false)

#define SM83_REG_NAME2ID_X(id, reg_name, indirect)                             \
    if(!strcmp(name, reg_name)) return id;

#define SM83_REG_ID2NAME_X(id, reg_name, indirect)                             \
    case id: return reg_name;

#define SM83_REG_CASE_X(id, reg_name, indirect)                                \
    case id:                                                                   \
        op->kind = (indirect) ? SM83_OP_KIND_IND_REG : RD_OP_REG;              \
        op->reg = id;                                                          \
        break;

typedef enum {
    SM83_OP_KIND_COND = RD_OP_USERBASE,
    SM83_OP_KIND_IND_REG, // [reg]
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

static void _sm83_emulate_default(RDContext* ctx, const RDInstruction* instr) {
    rd_foreach_operand(i, op, instr) {
        if(op->kind != RD_OP_ADDR) continue;

        if(rd_instr_is_jump(instr))
            rd_add_xref(ctx, instr->address, op->addr, RD_CR_JUMP);
        else if(rd_instr_is_call(instr))
            rd_add_xref(ctx, instr->address, op->addr, RD_CR_CALL);
        else
            rd_add_xref(ctx, instr->address, op->addr, RD_DR_ADDRESS);
    }
}

static void _sm83_ind_reg(RDOperand* op, SM83OperandType reg, i32 step) {
    op->kind = SM83_OP_KIND_IND_REG;
    op->reg = reg;
    op->userdata1 = (u32)step;
}

static bool _sm83_decode_op(RDContext* ctx, RDInstruction* instr,
                            const SM83Instruction* sm83, int idx,
                            usize* cursor) {
    if(sm83->operands[idx] == SM83_OP_NONE) return true;

    RDOperand* op = &instr->operands[idx];
    op->kind = RD_OP_STUB; // stub and override later

    switch(sm83->operands[idx]) {
        SM83_REGISTERS(SM83_REG_CASE_X)

        case SM83_OP_IND_BC: _sm83_ind_reg(op, SM83_OP_BC, 0); break;
        case SM83_OP_IND_DE: _sm83_ind_reg(op, SM83_OP_DE, 0); break;
        case SM83_OP_IND_C: _sm83_ind_reg(op, SM83_OP_C, 0); break;
        case SM83_OP_IND_HL: _sm83_ind_reg(op, SM83_OP_HL, 0); break;
        case SM83_OP_IND_HLI: _sm83_ind_reg(op, SM83_OP_HL, +1); break;
        case SM83_OP_IND_HLD: _sm83_ind_reg(op, SM83_OP_HL, -1); break;

        case SM83_OP_COND_Z:
        case SM83_OP_COND_NZ:
        case SM83_OP_COND_C:
        case SM83_OP_COND_NC:
            op->kind = SM83_OP_KIND_COND;
            op->reg = sm83->operands[idx];
            break;

        case SM83_OP_N8: {
            u8 b;
            if(!rd_read_byte(ctx, _sm83_pc(instr, cursor), &b)) return false;

            (*cursor)++;

            op->kind = RD_OP_CNST;
            op->cnst = b;
            break;
        }

        case SM83_OP_N16: {
            u16 imm;
            if(!rd_read_le16(ctx, _sm83_pc(instr, cursor), &imm)) return false;

            *cursor += sizeof(u16);

            // are we loading an address?
            if(sm83->id == SM83_INSTR_LD && idx == 1) {
                op->kind = RD_OP_ADDR;
                op->addr = (RDAddress)imm;
            }
            else {
                op->kind = RD_OP_CNST;
                op->cnst = imm;
            }
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

            RDAddress addr = 0xFF00 | b;

            if(sm83->id == SM83_INSTR_LDH && idx == 0) {
                op->kind = RD_OP_MEM;
                op->mem = addr;
            }
            else {
                op->kind = RD_OP_ADDR;
                op->addr = addr;
            }

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

        default: break;
    }
}

// 8-bit halves live inside their 16-bit pair, so a write to 'h' or a read of
// 'bc' stay coherent in the regval map.
// 'sp' and the pairs are roots.
static bool _sm83_reg_layout(SM83OperandType id, SM83OperandType* root,
                             RDRegMask* mask) {
    static const RDRegMask FULL = {.mask = RD_REGMASK_FULL, .shift = 0};
    static const RDRegMask HIGH = {.mask = 0xFF00, .shift = 8};
    static const RDRegMask LOW = {.mask = 0x00FF, .shift = 0};

    *root = id;
    *mask = FULL;

    // clang-format off
    switch(id) {
        case SM83_OP_AF:
        case SM83_OP_BC:
        case SM83_OP_DE:
        case SM83_OP_HL:
        case SM83_OP_SP: return true;

        case SM83_OP_A: *root = SM83_OP_AF; *mask = HIGH; return true;
        case SM83_OP_B: *root = SM83_OP_BC; *mask = HIGH; return true;
        case SM83_OP_C: *root = SM83_OP_BC; *mask = LOW; return true;
        case SM83_OP_D: *root = SM83_OP_DE; *mask = HIGH; return true;
        case SM83_OP_E: *root = SM83_OP_DE; *mask = LOW; return true;
        case SM83_OP_H: *root = SM83_OP_HL; *mask = HIGH; return true;
        case SM83_OP_L: *root = SM83_OP_HL; *mask = LOW; return true;
        default: break; // not a register ([hl] as an operand)
    }
    // clang-format on

    return false;
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

    if(!(q->want & (RD_QUERY_REG_WANT_MASK | RD_QUERY_REG_WANT_ROOT)))
        return true;

    SM83OperandType root;
    RDRegMask mask;
    if(!_sm83_reg_layout((SM83OperandType)q->id, &root, &mask)) return false;

    if(q->want & RD_QUERY_REG_WANT_MASK) q->mask = mask;

    if(q->want & RD_QUERY_REG_WANT_ROOT)
        q->root_name = _sm83_get_reg_name(root);

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

static void _sm83_forget(RDContext* ctx, const RDOperand* op) {
    if(op->kind == RD_OP_REG) rd_del_regval_id(ctx, op->reg);
}

static void _sm83_forget_all(RDContext* ctx) {
    // after a call the callee may have changed anything
    rd_del_regval_id(ctx, SM83_OP_AF);
    rd_del_regval_id(ctx, SM83_OP_BC);
    rd_del_regval_id(ctx, SM83_OP_DE);
    rd_del_regval_id(ctx, SM83_OP_HL);
}

// [hli] / [hld] move hl after the access
static void _sm83_step_hl(RDContext* ctx, const RDOperand* op) {
    if(op->kind != SM83_OP_KIND_IND_REG || op->reg != SM83_OP_HL) return;

    i32 step = op->userdata1;
    if(!step) return;

    RDRegValue hl;
    if(!rd_get_regval_id(ctx, SM83_OP_HL, &hl)) return;

    rd_set_regval_id(ctx, SM83_OP_HL, (hl + (RDRegValue)(i64)step) & 0xFFFF);
}

static void _sm83_track_regs(RDContext* ctx, const RDInstruction* instr) {
    const RDOperand* dst = &instr->operands[0];
    const RDOperand* src = &instr->operands[1];

    switch(instr->id) {
        case SM83_INSTR_LD: {
            _sm83_step_hl(ctx, dst);
            _sm83_step_hl(ctx, src);

            if((dst->kind == SM83_OP_KIND_IND_REG &&
                dst->reg == SM83_OP_IND_HL) ||
               (src->kind == SM83_OP_KIND_IND_REG &&
                src->reg == SM83_OP_IND_HL))
                rd_del_regval_id(ctx, SM83_OP_HL);

            if(dst->kind != RD_OP_REG) break;

            if(src->kind == RD_OP_CNST) {
                rd_set_regval_id(ctx, dst->reg, src->cnst);
            }
            else if(src->kind == RD_OP_ADDR) {
                rd_set_regval_id(ctx, dst->reg, src->addr);
            }
            else if(src->kind == RD_OP_REG) {
                RDRegValue v;
                if(rd_get_regval_id(ctx, src->reg, &v))
                    rd_set_regval_id(ctx, dst->reg, v);
                else
                    rd_del_regval_id(ctx, dst->reg);
            }
            else {
                rd_del_regval_id(ctx, dst->reg);
            }

            break;
        }

        case SM83_INSTR_LDH: _sm83_forget(ctx, dst); break;

        case SM83_INSTR_XOR:
            if(dst->kind == RD_OP_REG && src->kind == RD_OP_REG &&
               dst->reg == src->reg) {
                rd_set_regval_id(ctx, dst->reg, 0);
                break;
            }

            _sm83_forget(ctx, dst);
            break;

        case SM83_INSTR_INC:
        case SM83_INSTR_DEC:
        case SM83_INSTR_ADD:
        case SM83_INSTR_ADC:
        case SM83_INSTR_SUB:
        case SM83_INSTR_SBC:
        case SM83_INSTR_AND:
        case SM83_INSTR_OR:
        case SM83_INSTR_SWAP:
        case SM83_INSTR_RL:
        case SM83_INSTR_RLC:
        case SM83_INSTR_RR:
        case SM83_INSTR_RRC:
        case SM83_INSTR_SLA:
        case SM83_INSTR_SRA:
        case SM83_INSTR_SRL:
        case SM83_INSTR_SET:
        case SM83_INSTR_RES:
        case SM83_INSTR_POP: _sm83_forget(ctx, dst); break;

        // implicit 'a'
        case SM83_INSTR_RLCA:
        case SM83_INSTR_RLA:
        case SM83_INSTR_RRCA:
        case SM83_INSTR_RRA:
        case SM83_INSTR_CPL:
        case SM83_INSTR_DAA: rd_del_regval_id(ctx, SM83_OP_AF); break;

        case SM83_INSTR_CALL:
        case SM83_INSTR_RST: _sm83_forget_all(ctx); break;

        default: break;
    }
}

// Reports stores whose address is a plain bus address.
// The value is attached when the source register is known, otherwise the write
// is still reported.
static void _sm83_report_writes(RDContext* ctx, const RDInstruction* instr) {
    if(instr->id != SM83_INSTR_LD && instr->id != SM83_INSTR_LDH) return;

    const RDOperand* dst = &instr->operands[0];
    const RDOperand* src = &instr->operands[1];
    if(src->kind != RD_OP_REG) return;

    RDAddress addr;

    if(dst->kind == RD_OP_MEM) {
        addr = dst->mem;
    }
    else if(instr->id == SM83_INSTR_LDH && dst->kind == SM83_OP_KIND_IND_REG &&
            dst->reg == SM83_OP_IND_C) {
        RDRegValue c;
        if(!rd_get_regval_id(ctx, SM83_OP_C, &c)) return; // address unknown
        addr = 0xFF00 | (c & 0xFF);
    }
    else {
        return; // [hl], [bc], [de]: address not tracked
    }

    u8 size = src->reg == SM83_OP_SP ? 2 : 1;
    RDRegValue v;

    if(rd_get_regval_id(ctx, src->reg, &v))
        rd_bus_write(ctx, addr, size, v);
    else
        rd_bus_write_unknown(ctx, addr, size);
}

static void sm83_emulate(RDContext* ctx, const RDInstruction* instr,
                         RDProcessor* p) {
    RD_UNUSED(p);

    if(instr->id == SM83_INSTR_LD && instr->operands[1].kind == RD_OP_ADDR)
        rd_add_xref(ctx, instr->address, instr->operands[1].addr, RD_DR_READ);
    else if(instr->id == SM83_INSTR_LDH && instr->operands[0].kind == RD_OP_MEM)
        rd_add_xref(ctx, instr->address, instr->operands[0].mem, RD_DR_WRITE);
    else
        _sm83_emulate_default(ctx, instr);

    _sm83_report_writes(ctx, instr); // before tracking: sees the old values
    _sm83_track_regs(ctx, instr);

    if(rd_instr_can_flow(instr)) rd_flow(ctx, instr->address + instr->length);
}

static bool sm83_render_operand(RDRenderer* r, const RDInstruction* instr,
                                int idx, RDProcessor* p) {
    RD_UNUSED(p);

    const RDOperand* op = &instr->operands[idx];

    if(op->kind == SM83_OP_KIND_COND) {
        // clang-format off
        switch(op->reg) {
            case SM83_OP_COND_Z: rd_renderer_text(r, "z", RD_THEME_FAIL, RD_THEME_DEFAULT); return true;
            case SM83_OP_COND_NZ: rd_renderer_text(r, "nz", RD_THEME_FAIL, RD_THEME_DEFAULT); return true;
            case SM83_OP_COND_C: rd_renderer_text(r, "c", RD_THEME_FAIL, RD_THEME_DEFAULT); return true;
            case SM83_OP_COND_NC: rd_renderer_text(r, "nc", RD_THEME_FAIL, RD_THEME_DEFAULT); return true;
            default: break;
        }
        // clang-format on

        return false;
    }

    if(op->kind == SM83_OP_KIND_SYS) {
        rd_renderer_num(r, (i64)op->cnst, 16, 2, RD_NUM_NOADDR);
        rd_renderer_text(r, "h", RD_THEME_NUMBER, RD_THEME_DEFAULT);
        return true;
    }

    if(op->kind == SM83_OP_KIND_IND_REG) {
        rd_renderer_norm(r, "[");
        rd_renderer_reg(r, op->reg);

        // use RGBDS spelling: hli / hld
        i32 step = op->userdata1;
        if(step > 0)
            rd_renderer_text(r, "i", RD_THEME_REG, RD_THEME_DEFAULT);
        else if(step < 0)
            rd_renderer_text(r, "d", RD_THEME_REG, RD_THEME_DEFAULT);

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
