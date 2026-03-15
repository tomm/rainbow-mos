#include "console.h"
#include "formatting.h"
#include "globals.h"
#include "uart.h"
#include "version.h"
#include "printf.h"
#include <string.h>

static uint8_t quickrand(void)
{
	uint8_t out;
	asm volatile("ld a,r\n"
		     : "=%a"(out));
	return out;
}

static void rainbow_msg(char *msg)
{
	const uint8_t fgcol = active_console->get_fg_color_index();
	uint8_t i = quickrand() & (scrcolours - 1);
	if (strcmp(msg, "Rainbow") != 0) {
		kprintf("%s", msg);
		return;
	}
	if (i == 0)
		i++;
	for (; *msg; msg++) {
		putch(17);
		putch(i);
		putch(*msg);
		i = (i + 1 < scrcolours) ? i + 1 : 1;
	}
	set_color(fgcol);
}

void mos_bootmsg(void)
{
	kprintf("Agon ");
	rainbow_msg(VERSION_VARIANT);
	kprintf(" MOS " VERSION_GITREF);

// Show version subtitle, if we have one
#ifdef VERSION_SUBTITLE
	kprintf(" ");
	rainbow_msg(VERSION_SUBTITLE);
#endif

	kprintf("\n\r\n\r");
}
