#include <stdio.h>
#include "gb.h"

/* Total number of lines (including vertical blanking) */
#define VTOTAL 154U

void gb_gpu_set_lcd_stat(struct gb *gb, uint8_t stat) {
     struct gb_gpu *gpu = &gb->gpu;

     gpu->iten_mode0 = stat & 0x8;
     gpu->iten_mode1 = stat & 0x10;
     gpu->iten_mode2 = stat & 0x20;
     gpu->iten_lyc   = stat & 0x40;

     fprintf(stderr,
             "GPU ITEN: mode0: %d, mode1: %d, mode2: %d, lyc: %d\n",
             gpu->iten_mode0,
             gpu->iten_mode1,
             gpu->iten_mode2,
             gpu->iten_lyc);
}

uint8_t gb_gpu_get_lcd_stat(struct gb *gb) {
     struct gb_gpu *gpu = &gb->gpu;
     uint8_t r = 0;

     r |= gpu->iten_mode0 << 3;
     r |= gpu->iten_mode1 << 4;
     r |= gpu->iten_mode2 << 5;
     r |= gpu->iten_lyc << 6;

     /* XXX: Set LYC coincidence */
     /* XXX: Set mode bits */

     return r;
}

void gb_gpu_set_lcdc(struct gb *gb, uint8_t ctrl) {
     gb->gpu.lcdc = ctrl;
     fprintf(stderr, "GPU LCDC: 0x%02x\n", ctrl);
}

uint8_t gb_gpu_get_lcdc(struct gb *gb) {
     return gb->gpu.lcdc;
}

uint8_t gb_gpu_get_ly(struct gb *gb) {
     struct gb_gpu *gpu = &gb->gpu;

     /* XXX: Hack to prevent infinite loops in games waiting for a particular
      * value of LY */
     gpu->ly = (gpu->ly + 1) % VTOTAL;
     return gpu->ly;
}