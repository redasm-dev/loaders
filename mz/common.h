#pragma once

#include "common.h"
#include <redasm/redasm.h>

#define IMAGE_DOS_SIGNATURE 0x5A4D

typedef struct ImageDosHeader {
    u16 e_magic, e_cblp, e_cp, e_crlc, e_cparhdr;
    u16 e_minalloc, e_maxalloc;
    u16 e_ss, e_sp, e_csum, e_ip, e_cs;
    u16 e_lfarlc, e_ovno, e_res[4];
    u16 e_oemid, e_oeminfo, e_res2[10];
    u32 e_lfanew;
} ImageDosHeader;

bool mz_read_dos_header(RDReader* r, ImageDosHeader* dh);
