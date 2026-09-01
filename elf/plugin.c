#include "constants.h"
#include "format.h"
#include <inttypes.h>
#include <redasm/redasm.h>

static const char* _elf_detect_xtensa_processor(u64 entry) {
    // esp-idf components/esp_system/ld/<chip>/memory.ld.in
    if(entry >= 0x40100000 && entry < 0x40200000)
        return "xtensa_esp8266"; // ESP8266_RTOS_SDK esp8266.ld: iram0_0_seg org
    if(entry >= 0x40370000 && entry < 0x403D0000)
        return "xtensa_esp32s3"; // SRAM_IRAM_START
    if(entry >= 0x40080000 && entry < 0x400A0000)
        return "xtensa_esp32"; // iram0_0_seg org
    if(entry >= 0x40020000 && entry < 0x40070000)
        return "xtensa_esp32s2"; // RAM_IRAM_START

    RD_LOG_WARN("xtensa: could not identify chip from entry point %" PRIx64
                ", defaulting to esp32",
                entry);

    return "xtensa_esp32";
}

static u32 _elf_prg_perm(u32 p_flags) {
    u32 perm = 0;
    if(p_flags & ELF_PF_R) perm |= RD_SP_R;
    if(p_flags & ELF_PF_W) perm |= RD_SP_W;
    if(p_flags & ELF_PF_X) perm |= RD_SP_X;
    return perm;
}

static u32 _elf_shdr_perm(u64 sh_flags) {
    u32 perm = RD_SP_R;
    if(sh_flags & ELF_SHF_WRITE) perm |= RD_SP_W;
    if(sh_flags & ELF_SHF_EXECINSTR) perm |= RD_SP_X;
    return perm;
}

static const char* _elf_sym_name(RDReader* r, const ELFShdr* strtab,
                                 u32 st_name) {
    if(!strtab->sh_size || st_name >= strtab->sh_size) return NULL;
    rd_reader_seek(r, strtab->sh_offset + st_name);
    return rd_reader_read_str(r, NULL);
}

static void _elf_load_sections(ELFFormat* elf, RDContext* ctx) {
    RDReader* r = rd_get_input_reader(ctx);

    for(u16 i = 0; i < elf->ehdr.e_shnum; i++) {
        ELFShdr shdr;
        if(!elf_read_shdr(elf, r, i, &shdr)) continue;
        if(!(shdr.sh_flags & ELF_SHF_ALLOC)) continue;
        if(!shdr.sh_addr || !shdr.sh_size) continue;

        const char* name = elf_read_shname(elf, r, shdr.sh_name);
        if(!name || !(*name)) name = rd_format("seg_%u", (unsigned)i);

        u32 perm = _elf_shdr_perm(shdr.sh_flags);

        if(!rd_map_segment_n(ctx, name, shdr.sh_addr, shdr.sh_size, perm))
            continue;

        if(shdr.sh_type != ELF_SHT_NOBITS && shdr.sh_offset && shdr.sh_size)
            rd_map_input_n(ctx, shdr.sh_offset, shdr.sh_addr, shdr.sh_size);
    }
}

static void _elf_load_program(ELFFormat* elf, RDContext* ctx) {
    RDReader* r = rd_get_input_reader(ctx);

    for(u16 i = 0; i < elf->ehdr.e_phnum; i++) {
        ELFPhdr phdr;
        if(!elf_read_phdr(elf, r, i, &phdr)) continue;
        if(phdr.p_type != ELF_PT_LOAD) continue;
        if(phdr.p_memsz == 0) continue;

        const char* name = rd_format("LOAD_%d", (int)i);
        u32 perm = _elf_prg_perm(phdr.p_flags);

        if(!rd_map_segment_n(ctx, name, phdr.p_vaddr, phdr.p_memsz, perm))
            continue;

        // p_filesz <= p_memsz always BSS tail has no file backing
        if(phdr.p_filesz > 0) {
            rd_map_input_n(ctx, phdr.p_offset, phdr.p_vaddr,
                           phdr.p_filesz < phdr.p_memsz ? phdr.p_filesz
                                                        : phdr.p_memsz);
        }
    }
}

static void _elf_process_sym(RDContext* ctx, const ELFFormat* elf, RDReader* r,
                             const ELFShdr* strtab, const ELFSym* sym) {
    u8 bind = ELF_ST_BIND(sym->st_info);
    u8 type = ELF_ST_TYPE(sym->st_info);

    // skip local, section, file, and untyped symbols
    if(bind == ELF_STB_LOCAL) return;
    if(type == ELF_STT_NOTYPE || type == ELF_STT_SECTION ||
       type == ELF_STT_FILE || type == ELF_STT_TLS)
        return;

    // read symbol name from the section's associated strtab
    rd_reader_seek(r, strtab->sh_offset + sym->st_name);
    const char* name = rd_reader_read_str(r, NULL);
    if(!name || !(*name)) return;

    bool is_imported = sym->st_shndx == ELF_SHN_UNDEF;
    RDAddress symaddr = elf_norm(ctx, elf, (RDAddress)sym->st_value);

    if(type == ELF_STT_FUNC) {
        if(is_imported)
            rd_set_external(ctx, symaddr, NULL, RD_EXT_IMPORTED);
        else
            rd_set_function(ctx, symaddr);

        rd_library_name(ctx, symaddr, name);
    }
    else if(type == ELF_STT_OBJECT) {
        if(is_imported) rd_set_external(ctx, symaddr, NULL, RD_EXT_IMPORTED);
        rd_library_name(ctx, symaddr, name);
    }

    // mark globals defined in this binary as exported
    if(!is_imported && bind == ELF_STB_GLOBAL)
        rd_set_external(ctx, symaddr, NULL, RD_EXT_EXPORTED);
}

static void _elf_read_symtab(ELFFormat* elf, RDContext* ctx, RDReader* r,
                             const ELFShdr* symtab) {
    if(!symtab->sh_size || !symtab->sh_entsize) return;

    // sh_link points to the associated string table
    ELFShdr strtab;
    if(!elf_read_shdr(elf, r, (u16)symtab->sh_link, &strtab)) return;

    u64 count = symtab->sh_size / symtab->sh_entsize;

    // skip entry 0, always STN_UNDEF (null symbol)
    for(u64 i = 1; i < count; i++) {
        ELFSym sym;
        if(!elf_read_sym(elf, r, symtab->sh_offset, i, &sym)) continue;

        _elf_process_sym(ctx, elf, r, &strtab, &sym);
    }
}

static void _elf_read_relocs(ELFFormat* elf, RDContext* ctx, RDReader* r,
                             const ELFShdr* rel_shdr) {
    if(!rel_shdr->sh_size || !rel_shdr->sh_entsize) return;

    // sh_link points to the associated string table (dynsym)
    ELFShdr dynsym;
    if(!elf_read_shdr(elf, r, (u16)rel_shdr->sh_link, &dynsym)) return;

    // dynsym.sh_link: associated string table (dynstr)
    ELFShdr dynstr;
    if(!elf_read_shdr(elf, r, (u16)dynsym.sh_link, &dynstr)) return;

    u64 count = rel_shdr->sh_size / rel_shdr->sh_entsize;
    bool is_rela = rel_shdr->sh_type == ELF_SHT_RELA;
    bool is64 = elf_get_bits(elf) == 64;

    for(u64 i = 0; i < count; i++) {
        u64 r_offset, sym_idx;

        if(is_rela) {
            ELFRela rela;
            if(!elf_read_rela(elf, r, rel_shdr->sh_offset, i, &rela)) continue;
            r_offset = rela.r_offset;
            sym_idx =
                is64 ? ELF_R_SYM64(rela.r_info) : ELF_R_SYM32(rela.r_info);
        }
        else {
            ELFRel rel;
            if(!elf_read_rel(elf, r, rel_shdr->sh_offset, i, &rel)) continue;
            r_offset = rel.r_offset;
            sym_idx = is64 ? ELF_R_SYM64(rel.r_info) : ELF_R_SYM32(rel.r_info);
        }

        if(!sym_idx) continue; // symbol index 0: STN_UNDEF
        if(!r_offset) continue;

        ELFSym sym;
        if(!elf_read_sym(elf, r, dynsym.sh_offset, sym_idx, &sym)) continue;

        const char* name = _elf_sym_name(r, &dynstr, sym.st_name);
        if(!name || !(*name)) continue;

        rd_set_external(ctx, r_offset, NULL, RD_EXT_IMPORTED);
        rd_library_name(ctx, r_offset, name);
    }
}

static void _elf_load_symbols(ELFFormat* elf, RDContext* ctx) {
    RDReader* r = rd_get_input_reader(ctx);

    for(u16 i = 0; i < elf->ehdr.e_shnum; i++) {
        ELFShdr shdr;
        if(!elf_read_shdr(elf, r, i, &shdr)) continue;

        switch(shdr.sh_type) {
            case ELF_SHT_SYMTAB:
            case ELF_SHT_DYNSYM: _elf_read_symtab(elf, ctx, r, &shdr); break;

            case ELF_SHT_REL:
            case ELF_SHT_RELA: _elf_read_relocs(elf, ctx, r, &shdr); break;

            default: break;
        }
    }
}

static bool elf_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    ELFFormat* elf = (ELFFormat*)ldr;
    rd_reader_read(req->input, &elf->ident, sizeof(elf->ident));
    if(rd_reader_has_error(req->input)) return false;

    if(elf->ident.ei_magic[0] != ELF_MAG0 ||
       elf->ident.ei_magic[1] != ELF_MAG1 ||
       elf->ident.ei_magic[2] != ELF_MAG2 ||
       elf->ident.ei_magic[3] != ELF_MAG3 ||
       elf->ident.ei_version != ELF_VER_CURRENT)
        return false;

    if(elf->ident.ei_class != ELF_CLASS32 && elf->ident.ei_class != ELF_CLASS64)
        return false;

    if(elf->ident.ei_data != ELF_DATA2LSB && elf->ident.ei_data != ELF_DATA2MSB)
        return false;

    if(!elf_read_ehdr(elf, req->input)) return false;

    if(elf->ehdr.e_shstrndx && elf->ehdr.e_shstrndx < elf->ehdr.e_shnum) {
        return elf_read_shdr(elf, req->input, elf->ehdr.e_shstrndx,
                             &elf->shstrtab);
    }

    return true;
}

static bool elf_load(RDLoader* ldr, RDContext* ctx) {
    ELFFormat* elf = (ELFFormat*)ldr;

    if(elf->ehdr.e_shnum > 0 && elf->ehdr.e_shstrndx < elf->ehdr.e_shnum)
        _elf_load_sections(elf, ctx);
    else if(elf->ehdr.e_phnum > 0)
        _elf_load_program(elf, ctx);
    else
        return false;

    _elf_load_symbols(elf, ctx);

    // set entry point: only meaningful for ET_EXEC and ET_DYN
    if(elf->ehdr.e_type == ELF_ET_EXEC ||
       elf->ehdr.e_type == ELF_ET_DYN && elf->ehdr.e_entry) {
        rd_set_entry_point(ctx, elf_norm(ctx, elf, elf->ehdr.e_entry), NULL);
    }

    elf_load_kb(ctx, elf);
    return true;
}

static const char* elf_get_name(const RDLoader* ldr) {
    const ELFFormat* elf = (const ELFFormat*)ldr;
    if(elf_get_bits(elf) == 64) return "ELF64 Executable";
    return "ELF32 Executable";
}

static const char* elf_get_processor(const RDLoader* ldr) {
    const ELFFormat* elf = (const ELFFormat*)ldr;
    bool is_be = elf_is_be(elf);

    switch(elf->ehdr.e_machine) {
        case ELF_EM_386: return "x86_32";
        case ELF_EM_X86_64: return "x86_64";
        case ELF_EM_ARM: return is_be ? "arm32_be" : "arm32_le";
        case ELF_EM_AARCH64: return is_be ? "arm64_be" : "arm64_le";
        case ELF_EM_PPC: return "ppc32_be";
        case ELF_EM_PPC64: return is_be ? "ppc64_be" : "ppc64_le";

        case ELF_EM_XTENSA:
            return _elf_detect_xtensa_processor(elf->ehdr.e_entry);

        case ELF_EM_MIPS: {
            if(elf->ehdr.e_flags & ELF_EF_MIPS_ABI_EABI64)
                return is_be ? "mips64_be" : "mips64_le";
            return is_be ? "mips32_be" : "mips32_le";
        }

        default: return NULL;
    }

    return NULL;
}

static const RDLoaderPlugin ELF_LOADER = {
    .id = "elf",
    .instance_size = sizeof(ELFFormat),
    .get_name = elf_get_name,
    .get_processor = elf_get_processor,
    .parse = elf_parse,
    .load = elf_load,
};

static void elf_module_load(void) { rd_register_loader(&ELF_LOADER); }

RD_MODULE_EXPORT = {
    .api_version = RD_API_VERSION,
    .load = elf_module_load,
};
