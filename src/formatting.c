#include "formatting.h"
#include "console.h"
#include "defines.h"
#include "globals.h"
#include "keyboard_buffer.h"
#include "uart.h"
#include "printf.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

bool paginated_exit;
static uint8_t paginated_row, paginated_col, paginated_page;
static bool paginated_enabled, paginated_suppress_lf;

void paginated_start(bool enabled)
{
	paginated_row = 0;
	paginated_col = 0;
	paginated_page = 0;
	paginated_suppress_lf = false;
	paginated_exit = false;
	/* Do pause for any key after each page of output */
	paginated_enabled = enabled;
}

void set_color(uint8_t col)
{
	putch(17);
	putch(col);
}

static void clear_line()
{
	putch('\r');
	for (uint8_t i = scrcols - 1; i > 0; i--) {
		putch(' ');
	}
	putch('\r');
}

uint8_t get_secondary_color()
{
	// yellow in all modes except 2 color mode
	return scrcolours == 4 ? 2 : 11;
}

uint8_t get_primary_color()
{
	// cyan in 64, 16 col. yellow in 4 col.
	return scrcolours > 2 ? 14 & (scrcolours - 1) : 15;
}

static void handle_newline()
{
	const uint8_t oldFgCol = active_console->get_fg_color_index();
	struct keyboard_event_t ev;
	paginated_col = 0;
	paginated_row = paginated_row + 1;
	if (paginated_row >= scrrows - 2) {
		paginated_page++;
		paginated_row = 0;
		set_color(oldFgCol);
		if (!paginated_enabled) {
			if (kbuf_poll_event(&ev) && ev.isdown) {
				paginated_enabled = true;
				if (ev.ascii == 27) {
					paginated_exit = true;
					return;
				}
			}
		}
		if (paginated_enabled) {
			// yellow in most modes. visible in all
			set_color(get_primary_color());
			kprintf("--Page %d-- (ESC/q/c/any key)", paginated_page);
			kbuf_wait_keydown(&ev);
			if (ev.ascii == 27 || ev.ascii == 'q' || ev.ascii == 'Q') {
				paginated_exit = true;
			}
			if (ev.ascii == 'c' || ev.ascii == 'C') {
				paginated_enabled = false;
			}
			clear_line();
			set_color(oldFgCol);
		}
	}
}

void paginated_putch(uint8_t c)
{
	if (c == '\t') {
		for (uint8_t i = 0; i < 8; i++)
			paginated_putch(' ');
		return;
	} else if (c == '\n') {
		if (!paginated_suppress_lf) {
			putch('\r');
			putch('\n');
			handle_newline();
		}
		paginated_suppress_lf = false;
		return;
	} else {
		if (c < 32 || c == 127) {
			// escape it
			putch(27);
		}
		putch(c);
		paginated_col++;
		if (paginated_col == scrcols) {
			paginated_suppress_lf = true;
			handle_newline();
		} else {
			paginated_suppress_lf = false;
		}
	}
}

/**
 * Writes literal characters, not VDP control codes (only exception is \n)
 * Control codes will be escaped.
 */
void paginated_write(const char *buf, int len)
{
	for (int i = 0; i < len && !paginated_exit; i++) {
		paginated_putch(buf[i]);
	}
}

static void _paginated_putch_wrapper(int c, void *data)
{
	(void)data;
	paginated_putch(c);
}

void paginated_printf(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	npf_vpprintf(&_paginated_putch_wrapper, NULL, format, ap);
}
