#include "format.h"
#include <inttypes.h>
#include <string.h>

#define DEX_HASH_CHUNK_SIZE 4096

#define DEX_ADLER32_START_OFFSET 12
#define DEX_SHA1_START_OFFSET 32

#define DEX_HEADER_SIZE 0x70
#define DEX_ENDIAN_CONSTANT 0x12345678
#define DEX_REVERSE_ENDIAN_CONSTANT 0x78563412

static inline u32 _dex_parse_version(const DEXHeader* hdr) {
    if(hdr->magic[7] != '\0') return 0;

    u32 v = 0;
    for(int i = 4; i < 7; i++) {
        if(hdr->magic[i] < '0' || hdr->magic[i] > '9') return 0;
        v = (v * 10) + (u32)(hdr->magic[i] - '0');
    }

    switch(v) {
        case 35:
        // case 36: (never shipped)
        case 37:
        case 38:
        case 39:
        case 40: return v;
        default: return 0;
    }
}

static void _dex_hash_chunk(const char* data, usize n, void* userdata) {
    RDHash* h = (RDHash*)userdata;
    rd_hash_update(h, data, n);
}

static bool _dex_hash_range(RDReader* r, RDHashKind kind, u32 off, u32 len,
                            u8* digest, usize cap) {
    RDHash* h = rd_hash_create(kind);
    if(!h) return false;

    bool ok = false;
    rd_reader_save(r);
    rd_reader_seek(r, off);

    usize n_read =
        rd_reader_read_block(r, len, DEX_HASH_CHUNK_SIZE, _dex_hash_chunk, h);

    if(n_read == len) ok = rd_hash_final_to(h, digest, cap);

done:
    rd_reader_restore(r);
    rd_hash_destroy(h);
    return ok;
}

u32 dex_read_header(RDReader* r, DEXHeader* v) {
    if(!rd_reader_read_exact(r, &v->magic, sizeof(v->magic))) return 0;
    if(memcmp(v->magic, "dex\n", 4) != 0) return 0;

    rd_reader_read_le32(r, &v->checksum);
    rd_reader_read_exact(r, &v->signature, sizeof(v->signature));
    rd_reader_read_le32(r, &v->file_size);
    rd_reader_read_le32(r, &v->header_size);
    rd_reader_read_le32(r, &v->endian_tag);
    rd_reader_read_le32(r, &v->link_size);
    rd_reader_read_le32(r, &v->link_off);
    rd_reader_read_le32(r, &v->map_off);
    rd_reader_read_le32(r, &v->string_ids_size);
    rd_reader_read_le32(r, &v->string_ids_off);
    rd_reader_read_le32(r, &v->type_ids_size);
    rd_reader_read_le32(r, &v->type_ids_off);
    rd_reader_read_le32(r, &v->proto_ids_size);
    rd_reader_read_le32(r, &v->proto_ids_off);
    rd_reader_read_le32(r, &v->field_ids_size);
    rd_reader_read_le32(r, &v->field_ids_off);
    rd_reader_read_le32(r, &v->method_ids_size);
    rd_reader_read_le32(r, &v->method_ids_off);
    rd_reader_read_le32(r, &v->class_defs_size);
    rd_reader_read_le32(r, &v->class_defs_off);
    rd_reader_read_le32(r, &v->data_size);
    rd_reader_read_le32(r, &v->data_off);

    return !rd_reader_has_error(r) ? _dex_parse_version(v) : 0;
}

bool dex_read_string_id(RDReader* r, DEXStringId* v) {
    rd_reader_read_le32(r, &v->string_data_off);
    return !rd_reader_has_error(r);
}

bool dex_read_type_id(RDReader* r, DEXTypeId* v) {
    rd_reader_read_le32(r, &v->descriptor_idx);
    return !rd_reader_has_error(r);
}

bool dex_read_proto_id(RDReader* r, DEXProtoId* v) {
    rd_reader_read_le32(r, &v->shorty_idx);
    rd_reader_read_le32(r, &v->return_type_idx);
    rd_reader_read_le32(r, &v->parameters_off);
    return !rd_reader_has_error(r);
}

bool dex_read_field_id(RDReader* r, DEXFieldId* v) {
    rd_reader_read_le16(r, &v->class_idx);
    rd_reader_read_le16(r, &v->type_idx);
    rd_reader_read_le32(r, &v->name_idx);
    return !rd_reader_has_error(r);
}

bool dex_read_method_id(RDReader* r, DEXMethodId* v) {
    rd_reader_read_le16(r, &v->class_idx);
    rd_reader_read_le16(r, &v->proto_idx);
    rd_reader_read_le32(r, &v->name_idx);
    return !rd_reader_has_error(r);
}

bool dex_read_class_def(RDReader* r, DEXClassDef* v) {
    rd_reader_read_le32(r, &v->class_idx);
    rd_reader_read_le32(r, &v->access_flags);
    rd_reader_read_le32(r, &v->superclass_idx);
    rd_reader_read_le32(r, &v->interfaces_off);
    rd_reader_read_le32(r, &v->source_file_idx);
    rd_reader_read_le32(r, &v->annotations_off);
    rd_reader_read_le32(r, &v->class_data_off);
    rd_reader_read_le32(r, &v->static_values_off);
    return !rd_reader_has_error(r);
}

bool dex_validate_header(const RDReader* r, const DEXFormat* dex) {
    const DEXHeader* h = &dex->header;

    if(h->header_size != DEX_HEADER_SIZE) {
        RD_LOG_FAIL("header_size %" PRIu32 " unsupported "
                    "(v041 containers are not handled)",
                    h->header_size);
        return false;
    }

    if(h->endian_tag == DEX_REVERSE_ENDIAN_CONSTANT) {
        RD_LOG_FAIL("byte-swapped files are not supported");
        return false;
    }

    if(h->endian_tag != DEX_ENDIAN_CONSTANT) {
        RD_LOG_FAIL("bad endian_tag %08" PRIx32, h->endian_tag);
        return false;
    }

    if(h->file_size > rd_reader_get_length(r)) {
        RD_LOG_FAIL("file_size %" PRIu32 " exceeds input length %" PRIu64,
                    h->file_size, rd_reader_get_length(r));
        return false;
    }

    if(h->map_off && h->map_off >= h->file_size) {
        RD_LOG_FAIL("map_off %" PRIu32 " is out of bounds", h->map_off);
        return false;
    }

    return true;
}

bool dex_validate_checksum(RDReader* r, const DEXFormat* dex) {
    if(dex->header.file_size < DEX_ADLER32_START_OFFSET) return false;

    u8 digest[RD_HASH_ADLER32_LENGTH];

    if(!_dex_hash_range(r, RD_HASH_ADLER32, DEX_ADLER32_START_OFFSET,
                        dex->header.file_size - DEX_ADLER32_START_OFFSET,
                        digest, sizeof(digest)))
        return false;

    return rd_loadbe32(digest) == dex->header.checksum;
}

bool dex_validate_signature(RDReader* r, const DEXFormat* dex) {
    if(dex->header.file_size < DEX_SHA1_START_OFFSET) return false;

    u8 digest[RD_HASH_SHA1_LENGTH];

    if(!_dex_hash_range(r, RD_HASH_SHA1, DEX_SHA1_START_OFFSET,
                        dex->header.file_size - DEX_SHA1_START_OFFSET, digest,
                        sizeof(digest)))
        return false;

    return memcmp(digest, dex->header.signature, sizeof(digest)) == 0;
}
