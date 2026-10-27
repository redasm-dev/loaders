#pragma once

#include <redasm/redasm.h>

// Bank addressing: linear address = (bank << 16) | cpu_address.
// The low 16 bits are always the address the CPU puts on the bus, anything
// above is the bank id (never reachable by the CPU, so it can't collide).
// The default bank of every window keeps its plain address, so code that
// doesn't care about banking (DMG without MBC, bank 0) is unaffected.
#define GB_BANKED(bank, addr) (((RDRelAddress)(bank) << 16) | (addr))

extern const RDLoaderPlugin GB_LOADER;
