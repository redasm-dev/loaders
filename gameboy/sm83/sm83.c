#include "sm83.h"
#include "sm83/tables.h"

// id, name, root, bits, shift
#define SM83_REGISTERS(X)                                                      \
    X(SM83_OP_AF, "af", RD_REGID_INVALID, 16, 0)                               \
    X(SM83_OP_BC, "bc", RD_REGID_INVALID, 16, 0)                               \
    X(SM83_OP_DE, "de", RD_REGID_INVALID, 16, 0)                               \
    X(SM83_OP_HL, "hl", RD_REGID_INVALID, 16, 0)                               \
    X(SM83_OP_SP, "sp", RD_REGID_INVALID, 16, 0)                               \
    X(SM83_OP_A, "a", SM83_OP_AF, 8, 8)                                        \
    X(SM83_OP_B, "b", SM83_OP_BC, 8, 8)                                        \
    X(SM83_OP_C, "c", SM83_OP_BC, 8, 0)                                        \
    X(SM83_OP_D, "d", SM83_OP_DE, 8, 8)                                        \
    X(SM83_OP_E, "e", SM83_OP_DE, 8, 0)                                        \
    X(SM83_OP_H, "h", SM83_OP_HL, 8, 8)                                        \
    X(SM83_OP_L, "l", SM83_OP_HL, 8, 0)

#define SM83_REG_DEF_X(id, reg_name, reg_root, reg_bits, reg_shift)            \
    {id, reg_name, reg_root, reg_bits, reg_shift},

#define SM83_REG_CASE_X(id, reg_name, reg_root, reg_bits, reg_shift)           \
    case id:                                                                   \
        op->kind = RD_OP_REG;                                                  \
        op->reg = id;                                                          \
        break;

// clang-format off
static const RDRegisterDef SM83_REGS[] = {
    SM83_REGISTERS(SM83_REG_DEF_X)
    {0},
};
// clang-format on

typedef enum {
    SM83_OP_KIND_COND = RD_OP_USERBASE,
    SM83_OP_KIND_SYS,
} SM83Operands;

static inline RDAddress _sm83_pc(const RDInstruction* instr,
                                 const usize* cursor) {
    return instr->address + *cursor;
}

static void _sm83_ind_reg(RDOperand* op, SM83OperandType reg, i32 step) {
    op->kind = RD_OP_DISPL;
    op->displ.base = reg;
    op->displ.index = RD_REGID_INVALID;
    op->displ.offset = 0;
    op->displ.scale = 0;

    // post-step of 'base' ([hli] / [hld]): sm83 private, like arm's post_index
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

            // 'ld rr, nn' can load a pointer or a number: it stays an
            // immediate until an access proves it is an address
            if(sm83->id == SM83_INSTR_LD && idx == 1) {
                op->kind = RD_OP_IMM;
                op->imm = imm;
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

            op->kind = RD_OP_MEM;
            op->mem = 0xFF00 | b;
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
static const char* sm83_get_mnemonic(const RDInstruction* instr,
                                     RDProcessor* p) {
    RD_UNUSED(p);
    return instr->mnemonic;
}

static bool sm83_query_reg(RDQueryReg* q, RDProcessor* p) {
    RD_UNUSED(p);
    return rd_query_reg_table(q, SM83_REGS);
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

static bool _sm83_is_pair(RDReg reg) {
    return reg == SM83_OP_BC || reg == SM83_OP_DE || reg == SM83_OP_HL;
}

static RDReg _sm83_pair_of(RDReg reg) {
    switch(reg) {
        case SM83_OP_B:
        case SM83_OP_C: return SM83_OP_BC;
        case SM83_OP_D:
        case SM83_OP_E: return SM83_OP_DE;
        case SM83_OP_H:
        case SM83_OP_L: return SM83_OP_HL;
        default: break;
    }

    return reg;
}

// Address used by a [reg] operand, when the register's value is known
static bool _sm83_ind_address(RDContext* ctx, const RDOperand* op,
                              RDAddress* addr) {
    if(op->kind != RD_OP_DISPL) return false;

    RDRegValue v;

    if(op->displ.base == SM83_OP_C) { // ldh [c]: $FF00 + c
        if(!rd_get_regval_id(ctx, SM83_OP_C, &v)) return false;
        *addr = 0xFF00 | (v & 0xFF);
        return true;
    }

    if(!_sm83_is_pair(op->displ.base)) return false;
    if(!rd_get_regval_id(ctx, op->displ.base, &v)) return false;

    *addr = (RDAddress)(v & 0xFFFF);
    return true;
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
    if(op->kind != RD_OP_DISPL || op->displ.base != SM83_OP_HL) return;

    i32 step = (i32)op->userdata1;
    if(!step) return;

    RDRegValue hl;
    if(!rd_get_regval_id(ctx, SM83_OP_HL, &hl)) return;

    rd_set_regval_id(ctx, SM83_OP_HL, (hl + (RDRegValue)(i64)step) & 0xFFFF);
}

// The value a store writes, when it is known
static bool _sm83_stored_value(RDContext* ctx, const RDOperand* src,
                               RDRegValue* value, u8* size) {
    *size = 1;

    if(src->kind == RD_OP_CNST) {
        *value = src->cnst;
        return true;
    }

    if(src->kind != RD_OP_REG) return false;

    if(src->reg == SM83_OP_SP) *size = 2;
    return rd_get_regval_id(ctx, src->reg, value);
}

// Every memory access the instruction performs, and what it implies: the
// data xrefs, the pointer proof, and the bus write for a store. Registers
// are read as they were before the instruction, so this runs before any
// value is updated.
static void _sm83_access(RDContext* ctx, const RDInstruction* instr, bool store,
                         bool rmw) {
    rd_foreach_operand(i, op, instr) {
        RDAddress addr;
        bool has_addr = false;

        if(op->kind == RD_OP_MEM) { // ld [nn], a / ldh [n], a
            addr = op->mem;
            has_addr = true;

            rd_add_xref(ctx, instr->address, addr,
                        i == 0 ? RD_DR_WRITE : RD_DR_READ);
        }
        else if(_sm83_ind_address(ctx, op, &addr)) {
            has_addr = true;

            // an access through a register is only reported as a reference
            // when the register holds something addressable
            if(rd_is_address(ctx, addr)) {
                if(i == 0 && (store || rmw))
                    rd_add_xref(ctx, instr->address, addr, RD_DR_WRITE);

                if(i > 0 || rmw)
                    rd_add_xref(ctx, instr->address, addr, RD_DR_READ);

                if(_sm83_is_pair(op->displ.base))
                    rd_mark_regval_address(ctx, op->displ.base);
            }
        }

        // the bus sees every store, addressable or not ($FFFF is not memory)
        if(i != 0 || !has_addr || (!store && !rmw)) continue;

        RDRegValue value;
        u8 size = 1;

        // a read-modify-write does not compute the new value, and a store
        // from an unknown register still reports the write
        if(!rmw && _sm83_stored_value(ctx, &instr->operands[1], &value, &size))
            rd_bus_write(ctx, addr, size, value);
        else
            rd_bus_write_unknown(ctx, addr, size);
    }
}

static void _sm83_emu_ld(RDContext* ctx, const RDInstruction* instr) {
    const RDOperand* dst = &instr->operands[0];
    const RDOperand* src = &instr->operands[1];

    _sm83_access(ctx, instr, true, false);

    _sm83_step_hl(ctx, dst);
    _sm83_step_hl(ctx, src);

    if(dst->kind != RD_OP_REG) return;

    switch(src->kind) {
        case RD_OP_CNST: rd_set_regval_id(ctx, dst->reg, src->cnst); break;
        case RD_OP_IMM: rd_set_regval_id(ctx, dst->reg, src->imm); break;

        case RD_OP_REG: {
            RDRegValue v;

            if(rd_get_regval_id(ctx, src->reg, &v))
                rd_set_regval_id(ctx, dst->reg, v);
            else
                rd_del_regval_id(ctx, dst->reg);

            break;
        }

        // memory-sourced (ld a, [hl] / ldh a, [n]): unknown statically
        default: rd_del_regval_id(ctx, dst->reg); break;
    }
}

static void _sm83_emu_branch(RDContext* ctx, const RDInstruction* instr) {
    RDXRefType type = rd_instr_is_call(instr) ? RD_CR_CALL : RD_CR_JUMP;

    rd_foreach_operand(i, op, instr) {
        if(op->kind == RD_OP_ADDR) // jp nn / jr e8 / call nn
            rd_add_xref(ctx, instr->address, op->addr, type);
    }

    // jp hl: the target is wherever hl points
    if(instr->operands[0].kind == RD_OP_REG &&
       instr->operands[0].reg == SM83_OP_HL) {
        RDRegValue hl;

        if(rd_get_regval_id(ctx, SM83_OP_HL, &hl) &&
           rd_is_address(ctx, (RDAddress)hl)) {
            rd_add_xref(ctx, instr->address, (RDAddress)hl, type);
            rd_mark_regval_address(ctx, SM83_OP_HL);
        }
    }

    if(rd_instr_is_call(instr)) _sm83_forget_all(ctx);
}

// 'add a, l' with hl loaded by 'ld hl, nn': hl is the base of a table
static void _sm83_base_index(RDContext* ctx, const RDInstruction* instr) {
    const RDOperand* dst = &instr->operands[0];
    const RDOperand* src = &instr->operands[1];
    if(dst->kind != RD_OP_REG || dst->reg != SM83_OP_A) return;
    if(src->kind != RD_OP_REG) return;

    RDReg pair = _sm83_pair_of(src->reg);
    if(pair == src->reg || !_sm83_is_pair(pair)) return; // not a half

    rd_mark_regval_address(ctx, pair);
}

typedef enum {
    SM83_ALU_WRITE, // dst gets a value we don't compute (add, or, pop, ...)
    SM83_ALU_RMW,   // dst is read and written in place (inc, rl, set, ...)
    SM83_ALU_READ,  // dst is only read (cp, bit)
} SM83AluKind;

static void _sm83_emu_alu(RDContext* ctx, const RDInstruction* instr,
                          SM83AluKind kind) {
    const RDOperand* dst = &instr->operands[0];
    const RDOperand* src = &instr->operands[1];

    _sm83_access(ctx, instr, false, kind == SM83_ALU_RMW);

    if(instr->id == SM83_INSTR_ADD) _sm83_base_index(ctx, instr);

    if(kind == SM83_ALU_READ) return;

    // 'xor a, a' is zero whatever a was
    if(instr->id == SM83_INSTR_XOR && dst->kind == RD_OP_REG &&
       src->kind == RD_OP_REG && dst->reg == src->reg) {
        rd_set_regval_id(ctx, dst->reg, 0);
        return;
    }

    _sm83_forget(ctx, dst);
}

static void sm83_emulate(RDContext* ctx, const RDInstruction* instr,
                         RDProcessor* p) {
    RD_UNUSED(p);

    // clang-format off
    switch(instr->id) {
        case SM83_INSTR_LD:
        case SM83_INSTR_LDH: _sm83_emu_ld(ctx, instr); break;

        case SM83_INSTR_JP:
        case SM83_INSTR_JR:
        case SM83_INSTR_CALL:
        case SM83_INSTR_RST: _sm83_emu_branch(ctx, instr); break;

        case SM83_INSTR_INC:
        case SM83_INSTR_DEC:
        case SM83_INSTR_SWAP:
        case SM83_INSTR_RL:
        case SM83_INSTR_RLC:
        case SM83_INSTR_RR:
        case SM83_INSTR_RRC:
        case SM83_INSTR_SLA:
        case SM83_INSTR_SRA:
        case SM83_INSTR_SRL:
        case SM83_INSTR_SET:
        case SM83_INSTR_RES: _sm83_emu_alu(ctx, instr, SM83_ALU_RMW); break;

        case SM83_INSTR_ADD:
        case SM83_INSTR_ADC:
        case SM83_INSTR_SUB:
        case SM83_INSTR_SBC:
        case SM83_INSTR_AND:
        case SM83_INSTR_OR:
        case SM83_INSTR_XOR:
        case SM83_INSTR_POP: _sm83_emu_alu(ctx, instr, SM83_ALU_WRITE); break;

        case SM83_INSTR_CP:
        case SM83_INSTR_BIT: _sm83_emu_alu(ctx, instr, SM83_ALU_READ); break;

        // implicit 'a'
        case SM83_INSTR_RLCA:
        case SM83_INSTR_RLA:
        case SM83_INSTR_RRCA:
        case SM83_INSTR_RRA:
        case SM83_INSTR_CPL:
        case SM83_INSTR_DAA: rd_del_regval_id(ctx, SM83_OP_AF); break;

        default: break; // nop, push, ret, ei, di, halt, scf, ccf, ...
    }
    // clang-format on

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

    if(op->kind == RD_OP_DISPL) {
        rd_renderer_norm(r, "[");
        rd_renderer_reg(r, op->displ.base);

        // use RGBDS spelling: hli / hld
        i32 step = (i32)op->userdata1;
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
