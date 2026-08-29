#include "format.h"
#include "constants.h"
#include "header/header32.h"
#include "header/header64.h"

static bool _elf_read8(RDReader* r, u8* out) {
    return rd_reader_read_byte(r, out);
}

static bool _elf_read16(const ELFFormat* self, RDReader* r, u16* out) {
    return elf_is_be(self) ? rd_reader_read_be16(r, out)
                           : rd_reader_read_le16(r, out);
}

static bool _elf_read32(const ELFFormat* self, RDReader* r, u32* out) {
    return elf_is_be(self) ? rd_reader_read_be32(r, out)
                           : rd_reader_read_le32(r, out);
}

static bool _elf_read64(const ELFFormat* self, RDReader* r, u64* out) {
    return elf_is_be(self) ? rd_reader_read_be64(r, out)
                           : rd_reader_read_le64(r, out);
}
static bool _elf_read_addr(const ELFFormat* self, RDReader* r, u64* out) {
    if(elf_get_bits(self) == 64) return _elf_read64(self, r, out);

    u32 v = 0;
    if(!_elf_read32(self, r, &v)) return false;
    *out = (u64)v;
    return true;
}

bool elf_is_be(const ELFFormat* self) {
    return self->ident.ei_data == ELF_DATA2MSB;
}

int elf_get_bits(const ELFFormat* self) {
    return self->ident.ei_class == ELF_CLASS64 ? 64 : 32;
}

bool elf_read_ehdr(ELFFormat* self, RDReader* reader) {
    rd_reader_seek(reader, sizeof(ELFIdent));

    u32 e_version; // read and discard, validated in elf_parse already

    _elf_read16(self, reader, &self->ehdr.e_type);
    _elf_read16(self, reader, &self->ehdr.e_machine);
    _elf_read32(self, reader, &e_version);
    _elf_read_addr(self, reader, &self->ehdr.e_entry);
    _elf_read_addr(self, reader, &self->ehdr.e_phoff);
    _elf_read_addr(self, reader, &self->ehdr.e_shoff);
    _elf_read32(self, reader, &self->ehdr.e_flags);

    u16 e_ehsize; // read and discard
    _elf_read16(self, reader, &e_ehsize);

    _elf_read16(self, reader, &self->ehdr.e_phentsize);
    _elf_read16(self, reader, &self->ehdr.e_phnum);
    _elf_read16(self, reader, &self->ehdr.e_shentsize);
    _elf_read16(self, reader, &self->ehdr.e_shnum);
    _elf_read16(self, reader, &self->ehdr.e_shstrndx);

    return !rd_reader_has_error(reader);
}

bool elf_read_phdr(const ELFFormat* self, RDReader* reader, u16 idx,
                   ELFPhdr* out) {
    if(idx >= self->ehdr.e_phnum) return false;

    u64 offset = self->ehdr.e_phoff + ((u64)idx * self->ehdr.e_phentsize);
    rd_reader_seek(reader, offset);

    if(elf_get_bits(self) == 64) {
        _elf_read32(self, reader, &out->p_type);
        _elf_read32(self, reader, &out->p_flags);
        _elf_read64(self, reader, &out->p_offset);
        _elf_read64(self, reader, &out->p_vaddr);
        _elf_read64(self, reader, &out->p_paddr);
        _elf_read64(self, reader, &out->p_filesz);
        _elf_read64(self, reader, &out->p_memsz);
        _elf_read64(self, reader, &out->p_align);
    }
    else {
        u32 p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;

        _elf_read32(self, reader, &out->p_type);
        _elf_read32(self, reader, &p_offset);
        _elf_read32(self, reader, &p_vaddr);
        _elf_read32(self, reader, &p_paddr);
        _elf_read32(self, reader, &p_filesz);
        _elf_read32(self, reader, &p_memsz);
        _elf_read32(self, reader, &out->p_flags);
        _elf_read32(self, reader, &p_align);

        out->p_offset = p_offset;
        out->p_vaddr = p_vaddr;
        out->p_paddr = p_paddr;
        out->p_filesz = p_filesz;
        out->p_memsz = p_memsz;
        out->p_align = p_align;
    }

    return !rd_reader_has_error(reader);
}

bool elf_read_shdr(const ELFFormat* self, RDReader* reader, u16 idx,
                   ELFShdr* out) {
    if(idx >= self->ehdr.e_shnum) return false;

    u64 offset = self->ehdr.e_shoff + ((u64)idx * self->ehdr.e_shentsize);
    rd_reader_seek(reader, offset);

    _elf_read32(self, reader, &out->sh_name);
    _elf_read32(self, reader, &out->sh_type);
    _elf_read_addr(self, reader, &out->sh_flags);
    _elf_read_addr(self, reader, &out->sh_addr);
    _elf_read_addr(self, reader, &out->sh_offset);
    _elf_read_addr(self, reader, &out->sh_size);
    _elf_read32(self, reader, &out->sh_link);
    _elf_read32(self, reader, &out->sh_info);
    _elf_read_addr(self, reader, &out->sh_addralign);
    _elf_read_addr(self, reader, &out->sh_entsize);

    return !rd_reader_has_error(reader);
}

bool elf_read_sym(const ELFFormat* self, RDReader* reader, u64 offset, u64 idx,
                  ELFSym* out) {
    if(elf_get_bits(self) == 64) {
        rd_reader_seek(reader, offset + (idx * sizeof(Elf64Sym)));

        _elf_read32(self, reader, &out->st_name);
        _elf_read8(reader, &out->st_info);
        _elf_read8(reader, &out->st_other);
        _elf_read16(self, reader, &out->st_shndx);
        _elf_read64(self, reader, &out->st_value);
        _elf_read64(self, reader, &out->st_size);
    }
    else {
        rd_reader_seek(reader, offset + (idx * sizeof(Elf32Sym)));

        u32 st_value, st_size;

        _elf_read32(self, reader, &out->st_name);
        _elf_read32(self, reader, &st_value);
        _elf_read32(self, reader, &st_size);
        _elf_read8(reader, &out->st_info);
        _elf_read8(reader, &out->st_other);
        _elf_read16(self, reader, &out->st_shndx);

        out->st_value = st_value;
        out->st_size = st_size;
    }

    return !rd_reader_has_error(reader);
}

bool elf_read_rel(const ELFFormat* self, RDReader* reader, u64 offset, u64 idx,
                  ELFRel* out) {
    if(elf_get_bits(self) == 64) {
        rd_reader_seek(reader, offset + (idx * sizeof(Elf64Rel)));
        _elf_read64(self, reader, &out->r_offset);
        _elf_read64(self, reader, &out->r_info);
    }
    else {
        rd_reader_seek(reader, offset + (idx * sizeof(Elf32Rel)));
        u32 r_offset, r_info;
        _elf_read32(self, reader, &r_offset);
        _elf_read32(self, reader, &r_info);
        out->r_offset = r_offset;
        out->r_info = r_info;
    }

    return !rd_reader_has_error(reader);
}

bool elf_read_rela(const ELFFormat* self, RDReader* reader, u64 offset, u64 idx,
                   ELFRela* out) {
    if(elf_get_bits(self) == 64) {
        rd_reader_seek(reader, offset + (idx * sizeof(Elf64Rela)));
        _elf_read64(self, reader, &out->r_offset);
        _elf_read64(self, reader, &out->r_info);
        _elf_read64(self, reader, (u64*)&out->r_addend);
    }
    else {
        rd_reader_seek(reader, offset + (idx * sizeof(Elf32Rela)));
        u32 r_offset, r_info;
        i32 r_addend;
        _elf_read32(self, reader, &r_offset);
        _elf_read32(self, reader, &r_info);
        _elf_read32(self, reader, (u32*)&r_addend);
        out->r_offset = r_offset;
        out->r_info = r_info;
        out->r_addend = r_addend;
    }

    return !rd_reader_has_error(reader);
}

const char* elf_read_shname(const ELFFormat* self, RDReader* reader,
                            u32 sh_name) {
    if(!self->shstrtab.sh_size || sh_name >= self->shstrtab.sh_size)
        return NULL;

    rd_reader_seek(reader, self->shstrtab.sh_offset + sh_name);
    return rd_reader_read_str(reader, NULL);
}

RDAddress elf_norm(RDContext* ctx, const ELFFormat* elf, RDAddress address) {
    if(elf->ehdr.e_machine == ELF_EM_ARM) {
        if(address & 1) {
            rd_library_sregval(ctx, address & ~1, "T", 1);
            return address & ~1;
        }

        rd_library_sregval(ctx, address, "T", 0);
    }

    return address;
}

void elf_load_kb(RDContext* ctx, const ELFFormat* elf) {
    switch(elf->ehdr.e_machine) {
        case ELF_EM_386: rd_kb_load(ctx, "os/unix/libc/x86_32"); break;
        case ELF_EM_X86_64: rd_kb_load(ctx, "os/unix/libc/x86_64"); break;
        default: break;
    }
}
