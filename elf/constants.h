#pragma once

// e_ident
#define ELF_MAG0 0x7f
#define ELF_MAG1 'E'
#define ELF_MAG2 'L'
#define ELF_MAG3 'F'

#define ELF_CLASSNONE 0
#define ELF_CLASS32 1
#define ELF_CLASS64 2

#define ELF_DATANONE 0
#define ELF_DATA2LSB 1
#define ELF_DATA2MSB 2

#define ELF_VER_NONE 0
#define ELF_VER_CURRENT 1

// e_type
#define ELF_ET_NONE 0x0000
#define ELF_ET_REL 0x0001
#define ELF_ET_EXEC 0x0002
#define ELF_ET_DYN 0x0003
#define ELF_ET_CORE 0x0004

// e_machine (selected)
#define ELF_EM_386 3
#define ELF_EM_MIPS 8
#define ELF_EM_PPC 20
#define ELF_EM_PPC64 21
#define ELF_EM_ARM 40
#define ELF_EM_X86_64 62
#define ELF_EM_XTENSA 94
#define ELF_EM_AARCH64 183

// e_flags
#define ELF_EF_MIPS_ABI_EABI64 0x00004000

// p_type
#define ELF_PT_NULL 0x00000000
#define ELF_PT_LOAD 0x00000001
#define ELF_PT_DYNAMIC 0x00000002
#define ELF_PT_INTERP 0x00000003
#define ELF_PT_NOTE 0x00000004
#define ELF_PT_PHDR 0x00000006
#define ELF_PT_TLS 0x00000007

// p_flags
#define ELF_PF_X 0x1
#define ELF_PF_W 0x2
#define ELF_PF_R 0x4

// sh_type
#define ELF_SHT_NULL 0
#define ELF_SHT_PROGBITS 1
#define ELF_SHT_SYMTAB 2
#define ELF_SHT_STRTAB 3
#define ELF_SHT_RELA 4
#define ELF_SHT_HASH 5
#define ELF_SHT_DYNAMIC 6
#define ELF_SHT_NOTE 7
#define ELF_SHT_NOBITS 8
#define ELF_SHT_REL 9
#define ELF_SHT_DYNSYM 11

// sh_flags
#define ELF_SHF_WRITE 0x1
#define ELF_SHF_ALLOC 0x2
#define ELF_SHF_EXECINSTR 0x4
#define ELF_SHF_MERGE 0x10
#define ELF_SHF_STRINGS 0x20
#define ELF_SHF_TLS 0x400

// st_info: binding
#define ELF_STB_LOCAL 0
#define ELF_STB_GLOBAL 1
#define ELF_STB_WEAK 2

// st_info: type
#define ELF_STT_NOTYPE 0
#define ELF_STT_OBJECT 1
#define ELF_STT_FUNC 2
#define ELF_STT_SECTION 3
#define ELF_STT_FILE 4
#define ELF_STT_TLS 6

// st_other: visibility
#define ELF_STV_DEFAULT 0
#define ELF_STV_INTERNAL 1
#define ELF_STV_HIDDEN 2
#define ELF_STV_PROTECTED 3

// st_info accessors
#define ELF_ST_BIND(info) ((info) >> 4)
#define ELF_ST_TYPE(info) ((info) & 0x0F)
#define ELF_ST_VISIBILITY(other) ((other) & 0x3)

// special section indices
#define ELF_SHN_UNDEF 0x0000
#define ELF_SHN_LORESERVE 0xFF00
#define ELF_SHN_ABS 0xFFF1
#define ELF_SHN_COMMON 0xFFF2

// r_info accessors: symbol index extraction
#define ELF_R_SYM32(info) ((info) >> 8)
#define ELF_R_SYM64(info) ((info) >> 32)
