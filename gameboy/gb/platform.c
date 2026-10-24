#include "platform.h"

// Interrupt Enable register
#define GB_IE 0xFFFF

typedef struct GBInterrupt {
    u8 bit;
    RDAddress vector;
    const char* name;
} GBInterrupt;

static const GBInterrupt GB_INTERRUPTS[] = {
    {0, 0x40, "vblank_isr"}, {1, 0x48, "stat_isr"},   {2, 0x50, "timer_isr"},
    {3, 0x58, "serial_isr"}, {4, 0x60, "joypad_isr"},
};

// Nothing in the ROM says which interrupts are used: the only evidence is the
// program enabling them in IE.
// Vectors of interrupts that are never enabled are left alone, they often
// contain padding or data.
static void _gb_enable_interrupt(RDContext* ctx, const GBInterrupt* irq,
                                 RDAddress from) {
    // record who enabled it, even if the vector is already a function
    // (another write may enable it too)
    rd_add_xref(ctx, from, irq->vector, RD_DR_ADDRESS);

    // idempotent: the same write can be emulated several times
    if(rd_is_function(ctx, irq->vector)) return;

    if(!rd_set_function(ctx, irq->vector)) return;
    rd_auto_name(ctx, irq->vector, irq->name);
}

static void _gb_bus_write(RDContext* ctx, const RDHookEvent* e,
                          void* userdata) {
    RD_UNUSED(userdata);

    if(e->bus.address != GB_IE) return;

    // value unknown: can't tell which interrupts are enabled
    if(!e->bus.has_value) return;

    RDRegValue ie = e->bus.value;

    for(usize i = 0; i < rd_count_of(GB_INTERRUPTS); i++) {
        if(ie & (1U << GB_INTERRUPTS[i].bit))
            _gb_enable_interrupt(ctx, &GB_INTERRUPTS[i], e->bus.from);
    }
}

static void gb_setup(RDContext* ctx, RDPlatform* p) {
    RD_UNUSED(p);
    rd_register_hook(ctx, RD_HOOK_BUS, "redasm.bus_write", _gb_bus_write, NULL);
}

const RDPlatformPlugin GB_PLATFORM = {
    .id = "gameboy",
    .name = "Game Boy",
    .setup = gb_setup,
};
