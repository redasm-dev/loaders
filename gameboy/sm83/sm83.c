#include "sm83.h"
#include "sm83/tables.h"
#include <string.h>

#define SM83_REGISTERS(X)                                                      \
    X(SM83_OP_AF, "af")                                                        \
    X(SM83_OP_BC, "bc")                                                        \
    X(SM83_OP_DE, "de")                                                        \
    X(SM83_OP_HL, "hl")                                                        \
    X(SM83_OP_SP, "sp")                                                        \
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
        if(op->kind == RD_OP_MEM) {
            // only stores (ld [nn], r / ldh [n], a) have it as destination
            rd_add_xref(ctx, instr->address, op->mem,
                        i == 0 ? RD_DR_WRITE : RD_DR_READ);
            continue;
        }

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
        if(!q->name) return false;
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

static bool _sm83_is_rmw(u32 id) {
    switch(id) {
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
        case SM83_INSTR_RES: return true;
        default: break;
    }

    return false;
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

// 'info' describes a register that is being used as an address: if it was
// loaded by 'ld <reg>, nn' (the setter is checked, it may be stale or a
// partial write), that immediate is an address
static void _sm83_mark_pointer(RDContext* ctx, RDReg reg,
                               const RDRegValueInfo* info) {
    RDInstruction ld;
    if(!rd_decode(ctx, info->setter, &ld)) return;
    if(ld.id != SM83_INSTR_LD) return;
    if(ld.operands[0].kind != RD_OP_REG || ld.operands[0].reg != reg) return;
    if(ld.operands[1].kind != RD_OP_IMM || ld.operands[1].imm != info->value)
        return;

    rd_add_xref(ctx, info->setter, (RDAddress)info->value, RD_DR_ADDRESS);
}

// Address used by a [reg] operand, when known.
// 'pointer' is set when the address comes from a 16-bit register loaded by 'ld
// rr, nn'.
static bool _sm83_ind_address(RDContext* ctx, const RDOperand* op,
                              RDAddress* addr, RDRegValueInfo* info) {
    if(op->kind != SM83_OP_KIND_IND_REG) return false;

    if(op->reg == SM83_OP_C) { // ldh [c]: $FF00 + c
        if(!rd_get_regval_info_id(ctx, SM83_OP_C, info)) return false;
        *addr = 0xFF00 | (info->value & 0xFF);
        return true;
    }

    if(!_sm83_is_pair(op->reg)) return false;
    if(!rd_get_regval_info_id(ctx, op->reg, info)) return false;

    *addr = (RDAddress)(info->value & 0xFFFF);
    return true;
}

// Accesses through a register with a known value are accesses to an address
static void _sm83_deref(RDContext* ctx, const RDInstruction* instr) {
    bool rmw = _sm83_is_rmw(instr->id);
    bool store = instr->id == SM83_INSTR_LD || instr->id == SM83_INSTR_LDH;

    rd_foreach_operand(i, op, instr) {
        RDAddress addr;
        RDRegValueInfo info;
        if(!_sm83_ind_address(ctx, op, &addr, &info)) continue;
        if(!rd_is_address(ctx, addr)) continue;

        if(i == 0 && (store || rmw))
            rd_add_xref(ctx, instr->address, addr, RD_DR_WRITE);

        if(i > 0 || rmw) rd_add_xref(ctx, instr->address, addr, RD_DR_READ);

        if(_sm83_is_pair(op->reg)) _sm83_mark_pointer(ctx, op->reg, &info);
    }

    // jp hl
    if(instr->id == SM83_INSTR_JP && instr->operands[0].kind == RD_OP_REG &&
       instr->operands[0].reg == SM83_OP_HL) {
        RDRegValueInfo info;

        if(rd_get_regval_info_id(ctx, SM83_OP_HL, &info) &&
           rd_is_address(ctx, (RDAddress)info.value)) {
            rd_add_xref(ctx, instr->address, (RDAddress)info.value, RD_CR_JUMP);
            _sm83_mark_pointer(ctx, SM83_OP_HL, &info);
        }
    }
}

// 'add a, l' with hl loaded by 'ld hl, nn': hl is the base of a table
static void _sm83_base_index(RDContext* ctx, const RDInstruction* instr) {
    if(instr->id != SM83_INSTR_ADD) return;

    const RDOperand* dst = &instr->operands[0];
    const RDOperand* src = &instr->operands[1];
    if(dst->kind != RD_OP_REG || dst->reg != SM83_OP_A) return;
    if(src->kind != RD_OP_REG) return;

    RDReg pair = _sm83_pair_of(src->reg);
    if(pair == src->reg || !_sm83_is_pair(pair)) return; // not a half

    RDRegValueInfo info;
    if(!rd_get_regval_info_id(ctx, pair, &info)) return;
    if(!rd_is_address(ctx, (RDAddress)info.value)) return;

    _sm83_mark_pointer(ctx, pair, &info);
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

            if(dst->kind != RD_OP_REG) break;

            if(src->kind == RD_OP_CNST) {
                rd_set_regval_id(ctx, dst->reg, src->cnst);
            }
            else if(src->kind == RD_OP_IMM) {
                rd_set_regval_id(ctx, dst->reg, src->imm);
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

// Reports stores whose address is known: a plain bus address or a pointer
// register with a known value. The value is attached when it is known,
// otherwise the write is still reported.
static void _sm83_report_writes(RDContext* ctx, const RDInstruction* instr) {
    bool store = instr->id == SM83_INSTR_LD || instr->id == SM83_INSTR_LDH;
    if(!store && !_sm83_is_rmw(instr->id)) return;

    const RDOperand* dst = &instr->operands[0];
    const RDOperand* src = &instr->operands[1];

    RDAddress addr;

    if(dst->kind == RD_OP_MEM) {
        addr = dst->mem;
    }
    else {
        RDRegValueInfo info;
        if(!_sm83_ind_address(ctx, dst, &addr, &info)) return;
    }

    if(!store) { // read-modify-write: the new value is not computed
        rd_bus_write_unknown(ctx, addr, 1);
        return;
    }

    if(src->kind == RD_OP_CNST) {
        rd_bus_write(ctx, addr, 1, src->cnst);
        return;
    }

    if(src->kind != RD_OP_REG) {
        rd_bus_write_unknown(ctx, addr, 1);
        return;
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

    _sm83_emulate_default(ctx, instr);

    // all of these see the registers as they were before the instruction
    _sm83_deref(ctx, instr);
    _sm83_base_index(ctx, instr);
    _sm83_report_writes(ctx, instr);
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
