#include "tap.h"
#include "basic.h"
#include "common.h"
#include "tap_format.h"
#include <inttypes.h>

#define TAP_RAM_BASE 0x4000
#define TAP_HEADER_SIZE 19

static bool _tap_compute_ram_size(RDReader* r, RDAddress* out_ram_end) {
    RDAddress cursor = TAP_RAM_BASE;

    while(!rd_reader_at_end(r)) {
        u16 len;
        if(!rd_reader_read_le16(r, &len)) return false;

        u64 block_start = rd_reader_tell(r);
        u8 flags;
        if(!rd_reader_read_byte(r, &flags)) return false;

        usize payload_len =
            (flags == 0x00) ? TAP_HEADER_SIZE : (usize)(len - 2);
        cursor = rd_align_up(cursor + (RDAddress)payload_len, 0x10);

        rd_reader_seek(r, block_start + len);
    }

    *out_ram_end = rd_align_up(cursor, 0x1000);
    return true;
}

static bool tap_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    RD_UNUSED(ldr);

    if(rd_stricmp(req->ext, "tap") != 0) return false;

    usize len = rd_reader_get_length(req->input);
    if(len < 4) return false; // can't even hold one minimal block

    u16 block_len;
    u8 flag;
    if(!rd_reader_read_le16(req->input, &block_len)) return false;

    if(block_len == 0 || (usize)(block_len + 2) > len)
        return false; // declared length can't exceed the file itself

    if(!rd_reader_read_byte(req->input, &flag)) return false;

    if(flag == 0x00) { // header block
        if(block_len != 19)
            return false; // header blocks are always exactly 19 bytes total

        TapBlockHeader hdr;
        if(!tap_read_block_header(req->input, &hdr)) return false;
        if(hdr.type > 3) return false;

        // verify the checksum for real, re-reading from the flag byte
        // rather than trusting the fields alone
        rd_reader_seek(req->input, 2);
        u8 checksum = 0;

        for(u16 i = 0; i < block_len; i++) {
            u8 b;
            if(!rd_reader_read_byte(req->input, &b)) return false;
            checksum ^= b;
        }

        if(checksum != 0) return false;
    }
    else if(flag != 0xFF)
        return false;

    return true;
}

static bool tap_load(RDLoader* ldr, RDContext* ctx) {
    RD_UNUSED(ldr);

    zx_setup_string_terminators(ctx);
    RDReader* r = rd_get_input_reader(ctx);

    RDAddress ram_end;
    if(!_tap_compute_ram_size(r, &ram_end)) return false;
    rd_reader_seek(r, 0);

    rd_map_segment(ctx, "ROM", 0x0000, 0x4000, RD_SP_R);
    rd_map_segment(ctx, "RAM", TAP_RAM_BASE, ram_end, RD_SP_RWX);

    RDAddress cursor = TAP_RAM_BASE;
    RDScratchBuffer* pending_basic = rd_scratch_create();
    char name[TAP_FILENAME_LENGTH + 1];
    TapBlockHeader prev_header = {.type = TAP_TYPE_INVALID};
    bool entry_set = false;

    while(!rd_reader_at_end(r)) {
        u16 len;
        if(!rd_reader_read_le16(r, &len)) goto fail;

        u64 block_start = rd_reader_tell(r);

        u8 flags;
        if(!rd_reader_read_byte(r, &flags)) goto fail;

        usize payload_start = 0, payload_len = 0;

        if(flags == 0x00) {
            TapBlockHeader blk;
            if(!tap_read_block_header(r, &blk)) goto fail;

            payload_start = block_start + 1; // just past the flag byte
            payload_len = TAP_HEADER_SIZE;
            prev_header = blk;
        }
        else {
            payload_start = block_start + 1;
            payload_len = len - 2;

            const char* entry_name =
                tap_trim_filename(prev_header.filename, name, sizeof(name));

            rd_map_input_n(ctx, payload_start, cursor, payload_len);

            if(prev_header.type == TAP_TYPE_PROGRAM) {
                rd_library_name(ctx, cursor, entry_name);
                rd_scratch_clear(pending_basic);

                for(usize k = 0; k < payload_len; k++) {
                    u8 b;
                    if(!rd_reader_read_byte(r, &b)) {
                        rd_scratch_destroy(pending_basic);
                        return false;
                    }
                    rd_scratch_push(pending_basic, (char)b);
                }

                zx_basic_attach_lines(ctx, cursor, pending_basic);
            }
            else if(prev_header.type == TAP_TYPE_BYTES_CODE) {
                if(!entry_set) {
                    rd_set_entry_point(ctx, cursor, entry_name);
                    entry_set = true;
                }
                else {
                    rd_set_function(ctx, cursor);
                    rd_library_name(ctx, cursor, entry_name);
                }

                rd_add_comment_before(
                    ctx, cursor,
                    rd_format("load address: 0x%04x", prev_header.address));
            }
            else if(prev_header.type != TAP_TYPE_INVALID) {
                RD_LOG_WARN("block type #%d ('%s' @ %" PRIx64
                            ") not decoded, mapped as raw data",
                            prev_header.type, entry_name, payload_start);
            }

            prev_header.type = TAP_TYPE_INVALID;
        }

        cursor = rd_align_up(cursor + (RDAddress)payload_len, 0x10);
        rd_reader_seek(r, block_start + len);
    }

    rd_scratch_destroy(pending_basic);
    return true;

fail:
    rd_scratch_destroy(pending_basic);
    return false;
}

static const char* tap_get_processor(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "z80";
}

static const char* tap_get_name(const RDLoader* ldr) {
    RD_UNUSED(ldr);
    return "ZX Spectrum TAP image";
}

const RDLoaderPlugin TAP_LOADER = {
    .id = "zx_spectrum_tap",
    .parse = tap_parse,
    .load = tap_load,
    .get_processor = tap_get_processor,
    .get_name = tap_get_name,
};
