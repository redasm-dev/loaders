#include "sym.h"
#include "gb/loader.h"
#include <redasm/support/logging.h>
#include <redasm/support/path.h>
#include <redasm/support/scratch.h>
#include <string.h>

// RGBDS symbol files (https://rgbds.gbdev.io/sym)
#define GB_SYM_MAX_NAME 256
#define GB_SYM_MAX_TOKENS 3

typedef struct GBSymToken {
    const char* data;
    usize length;
} GBSymToken;

// locations are hexadecimal and carry no base prefix
static bool _gb_sym_hex(const char* s, usize n, u64* v) {
    if(!n || n > 8) return false;

    u64 r = 0;

    for(usize i = 0; i < n; i++) {
        char c = s[i];

        if(c >= '0' && c <= '9')
            r = (r << 4) | (u64)(c - '0');
        else if(c >= 'a' && c <= 'f')
            r = (r << 4) | (u64)(c - 'a' + 10);
        else if(c >= 'A' && c <= 'F')
            r = (r << 4) | (u64)(c - 'A' + 10);
        else
            return false;
    }

    *v = r;
    return true;
}

static bool _gb_sym_is_name(const GBSymToken* t) {
    if(!t->length) return false;
    char c = t->data[0]; // [A-Za-z_] starts a name
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static usize _gb_sym_split(const char* line, usize n, GBSymToken* tok) {
    usize ntok = 0, i = 0;

    while(i < n && ntok < GB_SYM_MAX_TOKENS) {
        while(i < n && (line[i] == ' ' || line[i] == '\t'))
            i++;

        if(i >= n) break;
        usize start = i;

        while(i < n && line[i] != ' ' && line[i] != '\t')
            i++;

        tok[ntok].data = line + start;
        tok[ntok].length = i - start;
        ntok++;
    }

    return ntok;
}

static void _gb_sym_parse_line(RDContext* ctx, const char* line, usize n,
                               usize* count) {
    // comments run to the end of the line and cannot be escaped
    for(usize i = 0; i < n; i++) {
        if(line[i] == ';') {
            n = i;
            break;
        }
    }

    GBSymToken tok[GB_SYM_MAX_TOKENS];
    usize ntok = _gb_sym_split(line, n, tok);

    // a one token line is ignored: reserved for future extensions
    if(ntok < 2) return;

    const GBSymToken* loc = &tok[0];

    // the boot rom is not part of the image
    if(loc->length > 5 && !memcmp(loc->data, "BOOT:", 5)) return;

    u64 bank = 0, addr = 0;
    const char* colon = memchr(loc->data, ':', loc->length);

    if(colon) {
        usize n_bank = (usize)(colon - loc->data);
        if(!_gb_sym_hex(loc->data, n_bank, &bank)) return;
        if(!_gb_sym_hex(colon + 1, loc->length - n_bank - 1, &addr)) return;
    }
    else if(!_gb_sym_hex(loc->data, loc->length, &addr)) {
        return;
    }

    if(addr > 0xFFFF) return;

    // names start with a letter or '_': block extensions ('.code:1A' and
    // friends) are not names and are skipped here
    const GBSymToken* sym = &tok[1];
    if(!_gb_sym_is_name(sym)) return;

    if(ntok > 2 && tok[2].data[0] != '@') {
        RD_LOG_WARN("ignored unknown symbol metadata '%.*s'",
                    (int)tok[2].length, tok[2].data);
    }

    RDAddress address =
        bank ? GB_BANKED(bank, (RDAddress)addr) : (RDAddress)addr;

    // unmapped (a bank the image doesn't have, or a region we don't map)
    if(!rd_is_address(ctx, address)) return;

    char name[GB_SYM_MAX_NAME];
    usize n_name = sym->length < sizeof(name) ? sym->length : sizeof(name) - 1;
    memcpy(name, sym->data, n_name);
    name[n_name] = 0;

    if(rd_library_name(ctx, address, name)) (*count)++;
}

void gb_sym_load(RDContext* ctx) {
    RDScratchBuffer* buf = rd_scratch_create();
    rd_scratch_puts(buf, rd_path_stem(rd_get_file_name(ctx)));
    rd_scratch_puts(buf, ".sym");

    const char* sympath =
        rd_path_join(rd_get_working_dir(ctx), rd_scratch_data(buf));

    if(rd_path_isfile(sympath)) {
        RDScratchBuffer* content = rd_scratch_create_from_file(sympath);

        if(content) {
            const char* data = rd_scratch_data(content);
            usize n = rd_scratch_length(content), start = 0, count = 0;

            for(usize i = 0; i <= n; i++) {
                if(i < n && data[i] != '\n') continue;

                usize len = i - start;
                if(len && data[start + len - 1] == '\r') len--; // CRLF
                _gb_sym_parse_line(ctx, data + start, len, &count);
                start = i + 1;
            }

            RD_LOG_INFO("loaded %zu symbol(s) from %s", count, sympath);
        }

        rd_scratch_destroy(content);
    }

    rd_scratch_destroy(buf);
}
