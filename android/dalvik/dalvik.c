#include "dalvik.h"
#include "dalvik/formats.h"
#include "dalvik/opcodes.h"
#include "dex/map.h"
#include <stdio.h>

#define DALVIK_MAX_REG 0xFFFF

static void _dalvik_read_map(RDContext* ctx, Dalvik* dalvik) {
    DEXMap map;
    if(!dex_read_map(rd_get_reader(ctx), &dalvik->header, &map)) return;

    const DEXMapItem* it = dex_map_find(&map, DEX_TYPE_CALL_SITE_ID_ITEM);

    if(it) {
        dalvik->call_site_ids_off = it->offset;
        dalvik->call_site_ids_size = it->size;
    }

    it = dex_map_find(&map, DEX_TYPE_METHOD_HANDLE_ITEM);

    if(it) {
        dalvik->method_handles_off = it->offset;
        dalvik->method_handles_size = it->size;
    }
}

static void _dalvik_fill_array_data(RDContext* ctx, RDAddress payload) {
    u16 ident, element_width;
    u32 size;
    if(!rd_read_le16(ctx, payload, &ident)) return;
    if(!rd_read_le16(ctx, payload + sizeof(u16), &element_width)) return;
    if(!rd_read_le32(ctx, payload + (sizeof(u16) * 2), &size)) return;

    if(ident != DALVIK_PAYLOAD_FILL_ARRAY_DATA || !element_width || !size)
        return;

    rd_library_type(ctx, payload, "DALVIK_FILL_ARRAY_DATA", 0, RD_TYPE_NONE);

    RDAddress data = payload + 8;
    const char* tname = rd_integral_from_size(element_width);
    if(tname) rd_library_type(ctx, data, tname, size, RD_TYPE_NONE);
}

static void _dalvik_switch_payload(RDContext* ctx, const RDInstruction* instr,
                                   RDAddress payload) {
    u16 ident, size;
    if(!rd_read_le16(ctx, payload, &ident)) return;
    if(!rd_read_le16(ctx, payload + sizeof(u16), &size)) return;
    if(!size) return;

    RDAddress keys = 0, targets;
    const char* tname;

    if(ident == DALVIK_PAYLOAD_PACKED_SWITCH) {
        tname = "DALVIK_PACKED_SWITCH";
        targets = payload + 8; // ident, size, first_key
    }
    else if(ident == DALVIK_PAYLOAD_SPARSE_SWITCH) {
        tname = "DALVIK_SPARSE_SWITCH";
        keys = payload + 4;
        targets = keys + ((u64)size * sizeof(u32));
    }
    else
        return;

    rd_library_type(ctx, payload, tname, 0, RD_TYPE_NONE);
    if(keys) rd_library_type(ctx, keys, "i32", size, RD_TYPE_NONE);
    rd_library_type(ctx, targets, "i32", size, RD_TYPE_NONE);

    for(u16 i = 0; i < size; i++) {
        u32 off;
        if(!rd_read_le32(ctx, targets + ((u64)i * sizeof(u32)), &off)) break;

        RDAddress dest = instr->address + ((i64)(i32)off * 2);
        rd_add_xref(ctx, instr->address, dest, RD_CR_JUMP);
        rd_flow(ctx, dest); // nothing else reaches a case target
    }
}

static void dalvik_setup(RDContext* ctx, RDProcessor* p) {
    rd_kb_load(ctx, "platform/android/dalvik");

    Dalvik* dalvik = (Dalvik*)p;

    RDReader* r = rd_get_reader(ctx);
    rd_reader_seek(r, 0);
    dalvik->is_valid = dex_read_header(r, &dalvik->header) != 0;
    if(dalvik->is_valid) _dalvik_read_map(ctx, dalvik);
}

static RDProcessor* dalvik_create(const RDProcessorPlugin* plugin) {
    RD_UNUSED(plugin);

    Dalvik* dalvik = (Dalvik*)rd_alloc0(1, sizeof(Dalvik));
    dalvik->raw_buf = rd_scratch_create();
    dalvik->string_buf = rd_scratch_create();
    return (RDProcessor*)dalvik;
}

static void dalvik_destroy(RDProcessor* p) {
    Dalvik* dalvik = (Dalvik*)p;
    rd_scratch_destroy(dalvik->string_buf);
    rd_scratch_destroy(dalvik->raw_buf);
    rd_free(dalvik);
}

static void dalvik_decode(RDContext* ctx, RDInstruction* instr,
                          RDProcessor* p) {
    Dalvik* dalvik = (Dalvik*)p;

    u16 unit0;
    if(!rd_read_le16(ctx, instr->address, &unit0)) return;

    u8 opcode = (u8)(unit0 & 0xFF);
    if(!opcode && (unit0 >> 8)) return; // switch/array payload

    const DalvikOpcode* op = &DALVIK_OPCODES[opcode];
    if(!op->mnemonic) return;

    switch(op->format) {
        case DALVIK_FMT_10T:
            if(!dalvik_decode_10t(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_10X: break; // no operands

        case DALVIK_FMT_11N:
            if(!dalvik_decode_11n(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_11X:
            if(!dalvik_decode_11x(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_12X:
            if(!dalvik_decode_12x(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_20T:
            if(!dalvik_decode_20t(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_21C:
            if(!dalvik_decode_21c(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_21H:
            if(!dalvik_decode_21h(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_21S:
            if(!dalvik_decode_21s(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_21T:
            if(!dalvik_decode_21t(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_22B:
            if(!dalvik_decode_22b(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_22C:
            if(!dalvik_decode_22c(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_22S:
            if(!dalvik_decode_22s(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_22T:
            if(!dalvik_decode_22t(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_22X:
            if(!dalvik_decode_22x(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_23X:
            if(!dalvik_decode_23x(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_30T:
            if(!dalvik_decode_30t(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_31C:
            if(!dalvik_decode_31c(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_31I:
            if(!dalvik_decode_31i(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_31T:
            if(!dalvik_decode_31t(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_32X:
            if(!dalvik_decode_32x(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_35C:
            if(!dalvik_decode_35c(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_3RC:
            if(!dalvik_decode_3rc(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_45CC:
            if(!dalvik_decode_45cc(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_4RCC:
            if(!dalvik_decode_4rcc(ctx, instr, op, unit0, dalvik)) return;
            break;

        case DALVIK_FMT_51L:
            if(!dalvik_decode_51l(ctx, instr, op, unit0, dalvik)) return;
            break;

        default:
            // RD_LOG_WARN("unhandled format %d @ %08lx", op->format,
            //             instr->address);
            break;
    }

    instr->id = opcode;
    instr->length = dalvik_u2b(DALVIK_FORMATS[op->format].units);
    instr->mnemonic = op->mnemonic;
    instr->flow = op->flow;
}

static void dalvik_emulate(RDContext* ctx, const RDInstruction* instr,
                           RDProcessor* p) {
    RD_UNUSED(p);

    if(instr->id == DALVIK_ID_PACKED_SWITCH ||
       instr->id == DALVIK_ID_SPARSE_SWITCH) {
        rd_add_xref(ctx, instr->address, instr->operands[1].addr,
                    RD_DR_ADDRESS);
        _dalvik_switch_payload(ctx, instr, instr->operands[1].addr);
    }
    else if(instr->id == DALVIK_ID_FILL_ARRAY_DATA) {
        rd_add_xref(ctx, instr->address, instr->operands[1].addr,
                    RD_DR_ADDRESS);
        _dalvik_fill_array_data(ctx, instr->operands[1].addr);
    }
    else {
        rd_foreach_operand(i, op, instr) {
            if(op->kind != RD_OP_ADDR) continue;

            if(rd_instr_is_call(instr))
                rd_add_xref(ctx, instr->address, op->addr, RD_CR_CALL);
            else if(rd_instr_is_jump(instr))
                rd_add_xref(ctx, instr->address, op->addr, RD_CR_JUMP);
            else
                rd_add_xref(ctx, instr->address, op->addr, RD_DR_ADDRESS);
        }
    }

    if(rd_instr_can_flow(instr)) rd_flow(ctx, instr->address + instr->length);
}

static bool dalvik_render_operand(RDRenderer* r, const RDInstruction* instr,
                                  int idx, RDProcessor* p) {
    Dalvik* dalvik = (Dalvik*)p;
    RDContext* ctx = rd_renderer_get_context(r);
    const RDOperand* op = &instr->operands[idx];

    switch(op->kind) {
        case DALVIK_OP_REGLIST: {
            rd_renderer_norm(r, "{");

            for(u16 i = 0; i < op->count; i++) {
                if(i) rd_renderer_norm(r, ",");
                rd_renderer_reg(r, dalvik_reglist_reg(op->cnst, i));
            }

            rd_renderer_norm(r, "}");
            return true;
        }

        case DALVIK_OP_REGRANGE: {
            rd_renderer_norm(r, "{");
            rd_renderer_reg(r, op->reg);

            if(op->count > 1) {
                rd_renderer_norm(r, "..");
                rd_renderer_reg(r, (op->reg + op->count - 1));
            }

            rd_renderer_norm(r, "}");
            return true;
        }

        case DALVIK_OP_INDEX: {
            rd_renderer_norm(r, dalvik_index_prefix((u8)op->userdata1));
            rd_renderer_num(r, (i64)op->cnst, 16, 4, RD_NUM_NOADDR);
            return true;
        }

        case RD_OP_CNST: {
            rd_renderer_num(r, (i64)op->cnst, 10, 0, RD_NUM_NOADDR);
            return true;
        }

        default: break;
    }

    return false;
}

static const char* dalvik_get_mnemonic(const RDInstruction* instr,
                                       RDProcessor* p) {
    RD_UNUSED(p);
    return instr->mnemonic;
}

static bool dalvik_query_reg(RDQueryReg* q, RDProcessor* p) {
    Dalvik* dalvik = (Dalvik*)p;

    switch(q->kind) {
        case RD_QUERY_REG_BY_ID: {
            if(q->id > DALVIK_MAX_REG) return false;

            snprintf(dalvik->reg_buf, sizeof(dalvik->reg_buf), "v%u",
                     (unsigned)q->id);
            q->name = dalvik->reg_buf;
            break;
        }

        case RD_QUERY_REG_BY_NAME: {
            if(!q->name || q->name[0] != 'v' || !q->name[1]) return false;

            unsigned long v = 0;

            for(const char* s = q->name + 1; *s; s++) {
                if(*s < '0' || *s > '9') return false;
                v = (v * 10) + (unsigned long)(*s - '0');
                if(v > DALVIK_MAX_REG) return false;
            }

            q->id = (RDReg)v;
            break;
        }

        default: return false;
    }

    // no sub-registers: a Dalvik register is always accessed whole
    if(q->want & RD_QUERY_REG_WANT_MASK) {
        q->mask.mask = RD_REGMASK_FULL;
        q->mask.shift = 0;
    }

    // no aliases either: vN is its own canonical form
    if(q->want & RD_QUERY_REG_WANT_CANONICAL) q->canonical_name = q->name;

    return true;
}

const RDProcessorPlugin DALVIK = {
    .id = "android_dalvik",
    .name = "Dalvik",
    .ptr_size = sizeof(u32),
    .create = dalvik_create,
    .destroy = dalvik_destroy,
    .setup = dalvik_setup,
    .get_mnemonic = dalvik_get_mnemonic,
    .query_reg = dalvik_query_reg,
    .decode = dalvik_decode,
    .emulate = dalvik_emulate,
    .render_operand = dalvik_render_operand,
};
