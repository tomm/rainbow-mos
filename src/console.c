#include "console.h"
#include "defines.h"
#include "fbconsole.h"
#include "globals.h"
#include "mos.h"
#include "timer.h"
#include "uart.h"

// Get the current cursor position from the VPD
//
void vdpGetCursorPos()
{
	vpd_protocol_flags &= 0xFE; // Clear the semaphore flag
	putch(23);		    // Request the cursor position
	putch(0);
	putch(VDP_cursor);
	wait_VDP(0x01);		    // Wait until the semaphore has been set, or a timeout happens
}

// Get the current screen dimensions from the VDU
//
void vdpGetModeInformation()
{
	vpd_protocol_flags &= 0xEF; // Clear the semaphore flag
	putch(23);
	putch(0);
	putch(VDP_mode);
	wait_VDP(0x10);		    // Wait until the semaphore has been set, or a timeout happens
}

// Get palette entry
//
uint8_t vdpReadPalette(uint8_t entry)
{
	vpd_protocol_flags &= 0xFB; // Clear the semaphore flag
	putch(23);
	putch(0);
	putch(VDP_palette);
	putch(entry);
	wait_VDP(0x04);
	return scrpixelIndex;
}

uint8_t vdp_get_fg_color_index()
{
	return vdpReadPalette(128);
}

uint8_t vdp_get_bg_color_index()
{
	return vdpReadPalette(129);
}

void fbGetCursorPos()
{
	cursorX = fb_curs_x;
	cursorY = fb_curs_y;
}

void fbGetModeInformation()
{
	// Nothing to do here. fbconsole.asm term_init() handles this
}

uint8_t fb_get_fg_color_index()
{
	for (int i = 0; i < 16; i++) {
		if (fb_vdp_palette[i] == fbterm_fg) {
			return i;
		}
	}
	return 15;
}

uint8_t fb_get_bg_color_index()
{
	for (int i = 0; i < 16; i++) {
		if (fb_vdp_palette[i] == fbterm_fg) {
			return i;
		}
	}
	return 0;
}

extern void UART0_serial_TX();
extern void fbconsole_putch();

struct console_driver_t vdp_console = {
	.get_cursor_pos = &vdpGetCursorPos,
	.get_mode_information = &vdpGetModeInformation,
	.get_fg_color_index = &vdp_get_fg_color_index,
	.get_bg_color_index = &vdp_get_bg_color_index,
};

struct console_driver_t fb_console = {
	.get_cursor_pos = &fbGetCursorPos,
	.get_mode_information = &fbGetModeInformation,
	.get_fg_color_index = &fb_get_fg_color_index,
	.get_bg_color_index = &fb_get_bg_color_index,
};

struct console_driver_t *active_console = &vdp_console;

void console_enable_fb()
{
	active_console = &fb_console;
	/* Call mos_api_setresetvector to set rst10 vector */
	asm volatile(
	    "push de\n"
	    "push hl\n"
	    "ld a,0x61 \n"
	    "ld e,0x10 \n"
	    "ld hl,_fbconsole_rst10_handler \n"
	    "rst.lil 8\n"
	    "pop hl\n"
	    "pop de\n");
}

void console_enable_vdp()
{
	active_console = &vdp_console;
	/* Call mos_api_setresetvector to set rst10 vector */
	asm volatile(
	    "push de\n"
	    "push hl\n"
	    "ld a,0x61 \n"
	    "ld e,0x10 \n"
	    "ld hl,rst_10_handler \n"
	    "rst.lil 8\n"
	    "pop hl\n"
	    "pop de\n");
}
