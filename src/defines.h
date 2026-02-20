/*
 * Title:			AGON MOS - MOS defines
 * Author:			Dean Belfield
 * Created:			21/03/2023
 * Last Updated:	10/11/2023
 *
 * Modinfo:
 * 22/03/2023:		The VDP commands are now indexed from 0x80
 * 24/03/2023:		Added DEBUG
 * 10/11/2023:		Added VDP_consolemode
 */

#ifndef MOS_DEFINES_H
#define MOS_DEFINES_H

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#include "../src_umm_malloc/umm_malloc.h"
#include "debug.h"
#include <stdbool.h>
#include <stdint.h>

/* For sanity */
typedef int int24_t;
typedef unsigned int uint24_t;

/* Some legacy types to avoid huge search/replace right now */
typedef uint24_t UINT;
typedef int24_t INT;

// Linker script symbols
extern int8_t __MOS_systemAddress[];
extern int8_t __heapbot[];
extern int8_t __heaptop[];
extern int8_t _stack[];
extern int8_t __rodata_end[];
extern int8_t __data_start[];
extern int8_t __data_len[];
extern int8_t _low_romdata[];
extern int _len_data;

// Just guaranteed space before bumping into a potential GPIO framebuffer.
// Since SP starts at bottom of MOS ram (0xbe000), it really can be used
// all the way down user RAM
#define SPL_STACK_SIZE 5856

#define HEAP_LEN ((int)__heaptop - (int)__heapbot)

// VDP specific (for VDU 23,0,n commands)
//
#define VDP_gp 0x80
#define VDP_keycode 0x81
#define VDP_cursor 0x82
#define VDP_scrchar 0x83
#define VDP_scrpixel 0x84
#define VDP_audio 0x85
#define VDP_mode 0x86
#define VDP_rtc 0x87
#define VDP_keystate 0x88
#define VDP_palette 0x94
#define VDP_logicalcoords 0xC0
#define VDP_consolemode 0xFE
#define VDP_terminalmode 0xFF

#endif /* MOS_DEFINES_H */
