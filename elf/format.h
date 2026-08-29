#pragma once

#include "header/common.h"
#include "header/header.h"

typedef struct ELFFormat {
    ELFIdent ident;
    ELFEhdr ehdr;
    ELFShdr shstrtab;
} ELFFormat;

bool elf_is_be(const ELFFormat* self);
int elf_get_bits(const ELFFormat* self);

bool elf_read_ehdr(ELFFormat* self, RDReader* reader);
bool elf_read_phdr(const ELFFormat* self, RDReader* reader, u16 idx,
                   ELFPhdr* out);
bool elf_read_shdr(const ELFFormat* self, RDReader* reader, u16 idx,
                   ELFShdr* out);
bool elf_read_sym(const ELFFormat* self, RDReader* reader, u64 offset, u64 idx,
                  ELFSym* out);
bool elf_read_rel(const ELFFormat* self, RDReader* reader, u64 offset, u64 idx,
                  ELFRel* out);
bool elf_read_rela(const ELFFormat* self, RDReader* reader, u64 offset, u64 idx,
                   ELFRela* out);

const char* elf_read_shname(const ELFFormat* self, RDReader* reader,
                            u32 sh_name);

RDAddress elf_norm(RDContext* ctx, const ELFFormat* elf, RDAddress address);
void elf_load_kb(RDContext* ctx, const ELFFormat* elf);
