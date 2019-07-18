#include <stdio.h>
#include "gb.h"

void gb_cpu_reset(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->sp = 0;
     cpu->a  = 0;
     cpu->b  = 0;
     cpu->c  = 0;
     cpu->d  = 0;
     cpu->e  = 0;
     cpu->h  = 0;
     cpu->l  = 0;

     cpu->f_z = false;
     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = false;

     /* XXX For the time being we don't emulate the BOOTROM so we start the
      * execution just past it */
     cpu->pc = 0x100;
}

static uint16_t gb_cpu_bc(struct gb *gb) {
     uint16_t b = gb->cpu.b;
     uint16_t c = gb->cpu.c;

     return (b << 8) | c;
}

static void gb_cpu_set_bc(struct gb *gb, uint16_t v) {
     gb->cpu.b = v >> 8;
     gb->cpu.c = v & 0xff;
}

static uint16_t gb_cpu_de(struct gb *gb) {
     uint16_t d = gb->cpu.d;
     uint16_t e = gb->cpu.e;

     return (d << 8) | e;
}

static void gb_cpu_set_de(struct gb *gb, uint16_t v) {
     gb->cpu.d = v >> 8;
     gb->cpu.e = v & 0xff;
}

static uint16_t gb_cpu_hl(struct gb *gb) {
     uint16_t h = gb->cpu.h;
     uint16_t l = gb->cpu.l;

     return (h << 8) | l;
}

static void gb_cpu_set_hl(struct gb *gb, uint16_t v) {
     gb->cpu.h = v >> 8;
     gb->cpu.l = v & 0xff;
}

static void gb_cpu_dump(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     fprintf(stderr, "Flags: %c %c %c %c\n",
             cpu->f_z ? 'Z' : '-',
             cpu->f_n ? 'N' : '-',
             cpu->f_h ? 'H' : '-',
             cpu->f_c ? 'C' : '-');
     fprintf(stderr, "PC: 0x%04x [%02x %02x %02x]\n",
             cpu->pc,
             gb_memory_readb(gb, cpu->pc),
             gb_memory_readb(gb, cpu->pc + 1),
             gb_memory_readb(gb, cpu->pc + 2));
     fprintf(stderr, "SP: 0x%04x [%02x %02x %02x]\n",
             cpu->sp,
             gb_memory_readb(gb, cpu->sp),
             gb_memory_readb(gb, cpu->sp + 1),
             gb_memory_readb(gb, cpu->sp + 2));
     fprintf(stderr, "A : 0x%02x\n",   cpu->a);
     fprintf(stderr, "B : 0x%02x  C : 0x%02x  BC : 0x%04x\n",
             cpu->b, cpu->c, gb_cpu_bc(gb));
     fprintf(stderr, "D : 0x%02x  E : 0x%02x  DE : 0x%04x\n",
             cpu->d, cpu->e, gb_cpu_de(gb));
     fprintf(stderr, "H : 0x%02x  L : 0x%02x  HL : 0x%04x\n",
             cpu->h, cpu->l, gb_cpu_hl(gb));
     fprintf(stderr, "\n");
}

static void gb_cpu_load_pc(struct gb *gb, uint16_t new_pc) {
     gb->cpu.pc = new_pc;
}

static void gb_cpu_pushb(struct gb *gb, uint8_t b) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->sp = (cpu->sp - 1) & 0xffff;

     gb_memory_writeb(gb, cpu->sp, b);
}

static uint8_t gb_cpu_popb(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     uint8_t b = gb_memory_readb(gb, cpu->sp);

     cpu->sp = (cpu->sp + 1) & 0xffff;

     return b;
}

static void gb_cpu_pushw(struct gb *gb, uint16_t w) {
     gb_cpu_pushb(gb, w >> 8);
     gb_cpu_pushb(gb, w & 0xff);
}

static uint16_t gb_cpu_popw(struct gb *gb) {
     uint16_t b0 = gb_cpu_popb(gb);
     uint16_t b1 = gb_cpu_popb(gb);

     return b0 | (b1 << 8);
}

static uint8_t gb_cpu_next_i8(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     uint8_t i8 = gb_memory_readb(gb, cpu->pc);

     cpu->pc = (cpu->pc + 1) & 0xffff;

     return i8;
}

static uint16_t gb_cpu_next_i16(struct gb *gb) {
     uint16_t b0 = gb_cpu_next_i8(gb);
     uint16_t b1 = gb_cpu_next_i8(gb);

     return b0 | (b1 << 8);
}

/****************
 * Instructions *
 ****************/

typedef void (*gb_instruction_f)(struct gb *);

static void gb_i_unimplemented(struct gb* gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint16_t instruction_pc = (cpu->pc - 1) & 0xffff;
     uint8_t instruction = gb_memory_readb(gb, instruction_pc);

     fprintf(stderr,
             "Unimplemented instruction 0x%02x at 0x%04x\n",
             instruction, instruction_pc);
     die();
}

/*****************
 * Miscellaneous *
 *****************/

static void gb_i_nop(struct gb *gb) {
     // NOP
}

static void gb_i_di(struct gb *gb) {
     // XXX TODO: disable interrupts
}

/**************
 * Arithmetic *
 **************/

/* Add two 16 bit values, update the CPU flags and return the result */
static uint16_t gb_cpu_addw_set_flags(struct gb *gb, uint16_t a, uint16_t b) {
     struct gb_cpu *cpu = &gb->cpu;

     /* Widen to 32bits to get the carry */
     uint32_t wa = a;
     uint32_t wb = b;

     uint32_t r = a + b;

     cpu->f_n = false;
     cpu->f_c = r & 0x10000;
     cpu->f_h = (wa ^ wb ^ r) & 0x1000;
     /* cpu->f_z is not altered */

     return r;
}

static void gb_i_sub_a_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = 0;

     cpu->f_z = true;
     cpu->f_n = true;
     cpu->f_h = false;
     cpu->f_c = false;
}

static void gb_i_add_sp_si8(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     /* Offset is signed */
     int8_t i8 = gb_cpu_next_i8(gb);

     /* Use 32bit arithmetic to avoid signed integer overflow UB */
     int32_t r = cpu->sp;
     r += i8;

     cpu->f_z = false;
     cpu->f_n = false;
     /* Carry and Half-carry are for the low byte */
     cpu->f_h = (cpu->sp ^ i8 ^ r) & 0x10;
     cpu->f_c = (cpu->sp ^ i8 ^ r) & 0x100;

     cpu->sp = r;
}

static void gb_i_add_hl_sp(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);

     hl = gb_cpu_addw_set_flags(gb, hl, gb->cpu.sp);

     gb_cpu_set_hl(gb, hl);
}

static void gb_i_inc_sp(struct gb *gb) {
     uint16_t sp = gb->cpu.sp;

     sp = (sp + 1) & 0xffff;

     gb->cpu.sp = sp;
}

static void gb_i_inc_bc(struct gb *gb) {
     uint16_t bc = gb_cpu_bc(gb);

     bc = (bc + 1) & 0xffff;

     gb_cpu_set_bc(gb, bc);
}

static void gb_i_inc_de(struct gb *gb) {
     uint16_t de = gb_cpu_de(gb);

     de = (de + 1) & 0xffff;

     gb_cpu_set_de(gb, de);
}

static void gb_i_inc_hl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);

     hl = (hl + 1) & 0xffff;

     gb_cpu_set_hl(gb, hl);
}

/*********
 * Loads *
 *********/

static void gb_i_ld_a_i8(struct gb *gb) {
     uint8_t i8 = gb_cpu_next_i8(gb);

     gb->cpu.a = i8;
}

static void gb_i_ld_mi16_a(struct gb *gb) {
     uint16_t i16 = gb_cpu_next_i16(gb);

     gb_memory_writeb(gb, i16, gb->cpu.a);
}

static void gb_i_ldh_mi8_a(struct gb *gb) {
     uint8_t i8 = gb_cpu_next_i8(gb);
     uint16_t addr = 0xff00 | i8;

     gb_memory_writeb(gb, addr, gb->cpu.a);
}

static void gb_i_ld_sp_i16(struct gb *gb) {
     uint16_t i16 = gb_cpu_next_i16(gb);

     gb->cpu.sp = i16;
}

static void gb_i_ld_hl_i16(struct gb *gb) {
     uint16_t i16 = gb_cpu_next_i16(gb);

     gb_cpu_set_hl(gb, i16);
}

static void gb_i_ld_a_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_memory_readb(gb, hl);

     gb->cpu.a = v;
}

static void gb_i_ld_b_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_memory_readb(gb, hl);

     gb->cpu.b = v;
}

static void gb_i_ld_c_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_memory_readb(gb, hl);

     gb->cpu.c = v;
}

static void gb_i_ld_d_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_memory_readb(gb, hl);

     gb->cpu.d = v;
}

static void gb_i_ld_e_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_memory_readb(gb, hl);

     gb->cpu.e = v;
}

static void gb_i_ld_h_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_memory_readb(gb, hl);

     gb->cpu.h = v;
}

static void gb_i_ld_l_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_memory_readb(gb, hl);

     gb->cpu.l = v;
}

static void gb_i_push_hl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);

     gb_cpu_pushw(gb, hl);
}

/***************
 * Jumps/Calls *
 ***************/

static void gb_i_jp_i16(struct gb *gb) {
     uint16_t i16 = gb_cpu_next_i16(gb);

     gb_cpu_load_pc(gb, i16);
}

static void gb_i_call_i16(struct gb *gb) {
     uint16_t i16 = gb_cpu_next_i16(gb);

     gb_cpu_pushw(gb, gb->cpu.pc);

     gb_cpu_load_pc(gb, i16);
}

static void gb_i_ret(struct gb *gb) {
     uint16_t addr = gb_cpu_popw(gb);

     gb_cpu_load_pc(gb, addr);
}

static gb_instruction_f gb_instructions[0x100] = {
     // 0x00
     gb_i_nop,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_inc_bc,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     // 0x10
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_inc_de,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     // 0x20
     gb_i_unimplemented,
     gb_i_ld_hl_i16,
     gb_i_unimplemented,
     gb_i_inc_hl,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     // 0x30
     gb_i_unimplemented,
     gb_i_ld_sp_i16,
     gb_i_unimplemented,
     gb_i_inc_sp,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_add_hl_sp,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_ld_a_i8,
     gb_i_unimplemented,
     // 0x40
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_ld_b_mhl,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_ld_c_mhl,
     gb_i_unimplemented,
     // 0x50
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_ld_d_mhl,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_ld_e_mhl,
     gb_i_unimplemented,
     // 0x60
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_ld_h_mhl,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_ld_l_mhl,
     gb_i_unimplemented,
     // 0x70
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_ld_a_mhl,
     gb_i_unimplemented,
     // 0x80
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     // 0x90
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_sub_a_a,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     // 0xa0
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     // 0xb0
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     // 0xc0
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_jp_i16,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_ret,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_call_i16,
     gb_i_unimplemented,
     gb_i_unimplemented,
     // 0xd0
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     // 0xe0
     gb_i_ldh_mi8_a,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_push_hl,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_add_sp_si8,
     gb_i_unimplemented,
     gb_i_ld_mi16_a,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     // 0xf0
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_di,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
};

void gb_cpu_run_instruction(struct gb *gb) {
     uint8_t instruction;

     gb_cpu_dump(gb);

     instruction = gb_cpu_next_i8(gb);

     gb_instructions[instruction](gb);
}
