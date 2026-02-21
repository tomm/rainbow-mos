/*
 * Title:			AGON MOS - MOS line editor
 * Author:			Dean Belfield
 * Created:			18/09/2022
 * Last Updated:	31/03/2023
 *
 * Modinfo:
 * 28/09/2022:		Added clear parameter to mos_EDITLINE
 * 20/02/2023:		Fixed mos_EDITLINE to handle the full CP-1252 character set
 * 09/03/2023:		Added support for virtual keys; improved editing functionality
 * 14/03/2023:		Tweaks ready for command history
 * 21/03/2023:		Improved backspace, and editing of long lines, after scroll, at bottom of screen
 * 22/03/2023:		Added a single-entry command line history
 * 31/03/2023:		Added timeout for VDP protocol
 */

#include "mos_editor.h"
#include "console.h"
#include "defines.h"
#include "ez80f92.h"
#include "formatting.h"
#include "globals.h"
#include "keyboard_buffer.h"
#include "mos.h"
#include "strings.h"
#include "timer.h"
#include "uart.h"
#include "vkey.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool tab_complete_state_showall;
static bool editHistoryDown(char *buffer, int buffer_capacity, int insertPos, int len);
static bool editHistorySet(char *buffer, int buffer_capacity, int insertPos, int len, int index);
static bool editHistoryUp(char *buffer, int buffer_capacity, int insertPos, int len);
static void editHistoryPush(char *buffer, int buffer_capacity);

// Storage for the command history
//
static char *cmd_history[cmd_historyDepth];

char *hotkey_strings[12] = {};

// Move cursor left
//
static void doLeftCursor()
{
	active_console->get_cursor_pos();
	if (cursorX > 0) {
		putch(0x08);
	} else {
		while (cursorX < (scrcols - 1)) {
			putch(0x09);
			cursorX++;
		}
		putch(0x0B);
	}
}

// Move Cursor Right
//
static void doRightCursor()
{
	active_console->get_cursor_pos();
	if (cursorX < (scrcols - 1)) {
		putch(0x09);
	} else {
		while (cursorX > 0) {
			putch(0x08);
			cursorX--;
		}
		putch(0x0A);
	}
}

// Insert a character in the input string
// Returns:
// - true if the character was inserted, otherwise false
//
static bool insertCharacter(char *buffer, int buffer_capacity, char c, int insertPos)
{
	int i;
	int count = 0;
	const int len = strnlen(buffer, buffer_capacity);

	if (len < buffer_capacity - 1) {
		putch(c);
		for (i = len; i >= insertPos; i--) {
			buffer[i + 1] = buffer[i];
		}
		buffer[insertPos] = c;

		for (i = insertPos + 1; i <= len; i++, count++) {
			putch(buffer[i]);
		}
		for (i = 0; i < count; i++) {
			doLeftCursor();
		}
		return 1;
	}
	return 0;
}

// Remove a character from the input string
// Parameters:
// - buffer: Pointer to the line edit buffer
// - insertPos: Position in the input string of the character to be deleted
// - len: Length of the input string before the character is deleted
// Returns:
// - true if the character was deleted, otherwise false
//
static bool deleteCharacter(char *buffer, int insertPos, int len)
{
	int i;
	int count = 0;
	if (insertPos > 0) {
		doLeftCursor();
		for (i = insertPos - 1; i < len; i++, count++) {
			uint8_t b = buffer[i + 1];
			buffer[i] = b;
			putch(b ? b : ' ');
		}
		for (i = 0; i < count; i++) {
			doLeftCursor();
		}
		return 1;
	}
	return 0;
}

static uint8_t deleteWord(char *buffer, int insertPos, int len)
{
	uint8_t num_deleted = 0;
	// First the trailing spaces
	while (insertPos > 0 && buffer[insertPos - 1] == ' ') {
		deleteCharacter(buffer, insertPos, len);
		num_deleted++;
		insertPos--;
	}
	// Then 1 'word'
	while (insertPos > 0 && buffer[insertPos - 1] != ' ') {
		deleteCharacter(buffer, insertPos, len);
		num_deleted++;
		insertPos--;
	}
	return num_deleted;
}

// handle HOME
//
static int gotoEditLineStart(int insertPos)
{
	while (insertPos > 0) {
		doLeftCursor();
		insertPos--;
	}
	return insertPos;
}

// handle END
//
static int gotoEditLineEnd(int insertPos, int len)
{
	while (insertPos < len) {
		doRightCursor();
		insertPos++;
	}
	return insertPos;
}

// remove current edit line
//
static void removeEditLine(char *buffer, int insertPos, int len)
{
	// goto start of line
	insertPos = gotoEditLineStart(insertPos);
	// set buffer to be spaces up to len
	memset(buffer, ' ', len);
	// print the buffer to erase old line from screen
	printf("%s", buffer);
	// clear the buffer
	buffer[0] = 0;
	gotoEditLineStart(len);
}

// Handle hotkey, if defined
// Returns:
// - 1 if the hotkey was handled, otherwise 0
//
static bool handleHotkey(uint8_t fkey, char *buffer, int bufferLength, int insertPos, int len)
{
	if (hotkey_strings[fkey] != NULL) {
		char *wildcardPos = strstr(hotkey_strings[fkey], "%s");

		if (wildcardPos == NULL) { // No wildcard in the hotkey string
			removeEditLine(buffer, insertPos, len);
			buffer[0] = 0;
			strbuf_append(buffer, bufferLength, hotkey_strings[fkey], bufferLength);
			printf("%s", buffer);
		} else {
			uint8_t prefixLength = wildcardPos - hotkey_strings[fkey];
			uint8_t replacementLength = strlen(buffer);
			uint8_t suffixLength = strlen(wildcardPos + 2);
			char *result;

			if (prefixLength + replacementLength + suffixLength + 1 >= bufferLength) {
				// Exceeds max command length (256 chars)
				putch(0x07);								 // Beep
				return 0;
			}

			const int result_capacity = prefixLength + replacementLength + suffixLength + 1; // +1 for null terminator
			result = umm_malloc(result_capacity);
			if (!result) {
				// Memory allocation failed
				return 0;
			}

			// Copy the portion preceding the wildcard to the buffer
			result[0] = 0;
			strbuf_append(result, result_capacity, hotkey_strings[fkey], prefixLength);
			strbuf_append(result, result_capacity, buffer, strnlen(buffer, bufferLength));
			strbuf_append(result, result_capacity, wildcardPos + 2, result_capacity);

			removeEditLine(buffer, insertPos, len);
			buffer[0] = 0;
			strbuf_append(buffer, bufferLength, result, result_capacity);
			printf("%s", buffer);

			umm_free(result);
		}
		return 1;
		// Key was present, so drop through to ASCII key handling
	}
	return 0;
}

void try_tab_expand_internal_cmd(struct tab_expansion_context *ctx);

void notify_tab_expansion(struct tab_expansion_context *ctx, enum TabExpansionType type, const char *fullExpansion, int fullExpansionLen, const char *expansion, int expansionLen)
{
	DEBUG_STACK();

	if (tab_complete_state_showall) {
		bool oom = false;

		tab_expansion_t e = {
			.type = type,
			.expansion = mos_strndup(fullExpansion, fullExpansionLen)
		};

		if (e.expansion == NULL) {
			oom = true;
		} else {
			if (!vec_push(&ctx->candidates, &e)) {
				oom = true;
				umm_free(e.expansion);
			}
		}

		if (oom) {
			// out-of-memory fallback. just print the expansion now
			printf("%.*s ", fullExpansionLen, fullExpansion);
		}
	}
	if (ctx->num_matches == 0) {
		size_t count = MIN((size_t)expansionLen, sizeof(ctx->expansion) - 1);
		memcpy(ctx->expansion, expansion, count);
		ctx->expansion[count] = 0;
	} else {
		for (size_t j = 0; j < strlen(ctx->expansion); j++) {
			if (expansion[j] == 0 || toupper(ctx->expansion[j]) != toupper(expansion[j])) {
				ctx->expansion[j] = 0;
				break;
			}
		}
	}
	ctx->num_matches++;
}

static void try_tab_expand_bin_name(struct tab_expansion_context *ctx)
{
	FRESULT fr;
	DIR dj;
	FILINFO fno;
	char search_term[128];

	DEBUG_STACK();

	search_term[0] = 0;
	strbuf_append(search_term, sizeof(search_term), ctx->cmdline, ctx->cmdline_insertpos);
	strbuf_append(search_term, sizeof(search_term), "*.bin", sizeof(search_term));

	// Try local .bin
	fr = f_findfirst(&dj, &fno, "", search_term);
	while ((fr == FR_OK && fno.fname[0])) {
		notify_tab_expansion(ctx, ExpandNormal, fno.fname, strlen(fno.fname) - 4, fno.fname + ctx->cmdline_insertpos, strlen(fno.fname) - ctx->cmdline_insertpos - 4);
		fr = f_findnext(&dj, &fno);
	}

	if (strcmp(cwd, "/mos") != 0) {
		fr = f_findfirst(&dj, &fno, "/mos/", search_term);
		while (fr == FR_OK && fno.fname[0]) { // Now try MOSlets
			notify_tab_expansion(ctx, ExpandNormal, fno.fname, strlen(fno.fname) - 4, fno.fname + ctx->cmdline_insertpos, strlen(fno.fname) - ctx->cmdline_insertpos - 4);
			fr = f_findnext(&dj, &fno);
		}
	}

	if (strcmp(cwd, "/bin") != 0) {
		// Otherwise try /bin/
		fr = f_findfirst(&dj, &fno, "/bin/", search_term);
		while ((fr == FR_OK && fno.fname[0])) {
			notify_tab_expansion(ctx, ExpandNormal, fno.fname, strlen(fno.fname) - 4, fno.fname + ctx->cmdline_insertpos, strlen(fno.fname) - ctx->cmdline_insertpos - 4);
			fr = f_findnext(&dj, &fno);
		}
	}
}

static const char *slice_strrchr(const char *s, int len, char needle)
{
	for (int i = len - 1; i >= 0; i--) {
		if (s[i] == needle) return &s[i];
	}
	return NULL;
}

static void try_tab_expand_argument(struct tab_expansion_context *ctx)
{
	char search_prefix[256];
	char *path;
	char *term;

	FRESULT fr;
	DIR dj;
	FILINFO fno;
	const char *searchTermStart;

	DEBUG_STACK();

	const char *word_start = slice_strrchr(ctx->cmdline, ctx->cmdline_insertpos, ' ');

	if (word_start == NULL) {
		// expanding from start of cmdline
		word_start = ctx->cmdline;
	} else {
		word_start++;
	}

	const size_t word_len = ctx->cmdline_insertpos - (word_start - ctx->cmdline);

	// don't autocomplete filename arguments containing wildcards
	if (slice_strrchr(word_start, word_len, '*') || slice_strrchr(word_start, word_len, '?')) {
		return;
	}

	search_prefix[0] = 0;
	strbuf_append(search_prefix, sizeof(search_prefix), word_start, word_len);
	strbuf_append(search_prefix, sizeof(search_prefix), "*", 1);

	char *last_slash = strrchr(search_prefix, '/');
	if (last_slash && last_slash != &search_prefix[0]) {
		*last_slash = 0;
		path = search_prefix;
		term = last_slash + 1;
	} else if (search_prefix[0] == '/') {
		path = "/";
		term = search_prefix + 1;
	} else {
		path = "";
		term = search_prefix;
	}

	// printf("Path:\"%s\" Pattern:\"%s\"\r\n", path, term);

	// Special case: '..'
	if (strcmp(term, ".*") == 0) {
		notify_tab_expansion(ctx, ExpandDirectory, "..", 2, "./", 2);
	}
	if (strcmp(term, "..*") == 0) {
		notify_tab_expansion(ctx, ExpandDirectory, "..", 2, "/", 1);
	}

	fr = f_findfirst(&dj, &fno, path, term);

	while (fr == FR_OK && fno.fname[0]) {
		// unsafe
		char expansion[128];
		expansion[0] = 0;
		strbuf_append(expansion, sizeof(expansion), fno.fname + strlen(term) - 1, sizeof(expansion) - 2);
		if (fno.fattrib & AM_DIR) {
			strbuf_append(expansion, sizeof(expansion), "/", 1);
		}
		notify_tab_expansion(ctx, fno.fattrib & AM_DIR ? ExpandDirectory : ExpandNormal, fno.fname, strlen(fno.fname), expansion, strlen(expansion));
		fr = f_findnext(&dj, &fno);
	}
}

static char find_first_nonspace_chr(const char *str, int max)
{
	int i = 0;
	for (; i < max && str[i] == ' '; i++) { }
	return str[i];
}

static int cmp_tab_candidate(const tab_expansion_t *a, const tab_expansion_t *b)
{
	if (a->type == b->type) {
		return strcasecmp(a->expansion, b->expansion);
	} else if (a->type == ExpandDirectory) {
		return -1;
	} else {
		return 1;
	}
}

static void print_expansion_candidates(Vec *candidates)
{
	uint8_t oldTextFg = active_console->get_fg_color_index();
	uint8_t longest_candidate = 0;

	if (candidates->len > 1) {
		qsort(candidates->data, candidates->len, sizeof(tab_expansion_t), (int (*)(const void *, const void *)) & cmp_tab_candidate);
	}

	// Find length of longest candidate
	vec_foreach(candidates, tab_expansion_t, item)
	{
		const uint8_t l = strlen(item->expansion);
		if (l > longest_candidate) longest_candidate = l;
	}
	longest_candidate = MIN(scrcols, longest_candidate + 1);
	const uint8_t maxCols = MAX(1, scrcols / longest_candidate);

	// newline away from partial CMD entry
	printf("\n");
	paginated_start(true);

	uint8_t col = 0;
	vec_foreach(candidates, tab_expansion_t, item)
	{
		if (col == maxCols) {
			col = 0;
			paginated_printf("\n");
		}

		if (item->type != ExpandNormal) {
			set_color(get_secondary_color());
		}
		paginated_printf("%-*s", col == (maxCols - 1) ? longest_candidate - 1 : longest_candidate,
		    item->expansion);
		set_color(oldTextFg);
		col++;
	}
	paginated_printf("\n");
}

static void do_tab_complete(char *buffer, int buffer_len, int *out_InsertPos)
{
	struct tab_expansion_context tab_ctx = {
		.num_matches = 0,
		.cmdline = buffer,
		.cmdline_insertpos = *out_InsertPos,
		.expansion = "\0"
	};

	DEBUG_STACK();

	vec_init(&tab_ctx.candidates, sizeof(tab_expansion_t));

	const char first_char = find_first_nonspace_chr(buffer, buffer_len);

	if (first_char == '.' || first_char == '/' || slice_strrchr(buffer, *out_InsertPos, ' ')) {
		try_tab_expand_argument(&tab_ctx);
	} else {
		try_tab_expand_internal_cmd(&tab_ctx);
		try_tab_expand_bin_name(&tab_ctx);
	}

	const int num_chars_added = strlen(tab_ctx.expansion);
	if (tab_ctx.num_matches > 0 && tab_complete_state_showall) {
		print_expansion_candidates(&tab_ctx.candidates);

		// do a full redraw of cmd line
		putch('\r');
		mos_print_prompt();
		printf("%s", buffer);
		uint8_t insert_pos_adjust = strlen(buffer) - (*out_InsertPos);
		while (insert_pos_adjust--) {
			doLeftCursor();
		}
	}

	if (tab_ctx.num_matches > 1 && num_chars_added == 0) {
		tab_complete_state_showall = true;
	}

	if (num_chars_added > 0 || (num_chars_added == 0 && tab_ctx.num_matches == 1)) {
		if (tab_ctx.num_matches == 1 && tab_ctx.expansion[num_chars_added - 1] != '/') {
			strbuf_append(tab_ctx.expansion, sizeof(tab_ctx.expansion), " ", 1);
		}
		const bool append_at_eol = (*out_InsertPos) == (int)strlen(buffer);
		int chars_inserted = strbuf_insert(buffer, buffer_len, tab_ctx.expansion, *out_InsertPos);
		printf("%.*s", chars_inserted, tab_ctx.expansion);

		*out_InsertPos = (*out_InsertPos) + chars_inserted;
		if (!append_at_eol) {
			// also need to redraw part of cmd after insert pos
			int len_tail = strlen(&buffer[*out_InsertPos]);
			printf("%.*s", len_tail, &buffer[*out_InsertPos]);
			// then move back to insert pos
			while (len_tail--) {
				doLeftCursor();
			}
		}
	}

	vec_foreach(&tab_ctx.candidates, tab_expansion_t, item)
	{
		umm_free(item->expansion);
	}
	vec_free(&tab_ctx.candidates);
}

// The main line edit function
// Parameters:
// - buffer: Pointer to the line edit buffer
// - bufferLength: Size of the buffer in bytes
// - flags: Set bit0 to 0 to not clear, 1 to clear on entry
// Returns:
// - The exit key pressed (ESC or CR)
//
uint24_t mos_EDITLINE(char *buffer, int bufferLength, uint8_t flags)
{
	bool clear = flags & 0x01;		// Clear the buffer on entry
	bool enableTab = flags & 0x02;		// Enable tab completion (default off)
	bool enableHotkeys = !(flags & 0x04);	// Enable hotkeys (default on)
	bool enableHistory = !(flags & 0x08);	// Enable history (default on)
	uint8_t keya = 0;			// The ASCII key
	uint8_t keyr = 0;			// The ASCII key to return back to the calling program

	tab_complete_state_showall = false;
	int insertPos;				// The insert position
	int len = 0;				// Length of current input
	history_no = history_size;		// Ensure our current "history" is the end of the list

	active_console->get_mode_information(); // Get the current screen dimensions

	if (clear) {				// Clear the buffer as required
		// memset(buffer, 0, bufferLength);
		buffer[0] = 0;
		insertPos = 0;
	} else {
		printf("%s", buffer);	    // Otherwise output the current buffer
		insertPos = strlen(buffer); // And set the insertpos to the end
	}

	// Loop until an exit key is pressed
	//
	while (keyr == 0) {
		struct keyboard_event_t ev;
		uint8_t historyAction = 0;
		len = strlen(buffer);
		kbuf_wait_keydown(&ev);
		keya = ev.ascii;

		if (keya != '\t') {
			tab_complete_state_showall = false;
		}

		switch (ev.vkey) {
		//
		// First any extended (non-ASCII keys)
		//
		case VK_HOME: {	    // HOME
			insertPos = gotoEditLineStart(insertPos);
		} break;
		case VK_END: {	    // END
			insertPos = gotoEditLineEnd(insertPos, len);
		} break;

		case VK_PAGEUP: {   // PgUp
			historyAction = 2;
		} break;

		case VK_PAGEDOWN: { // PgDn
			historyAction = 3;
		} break;

		case VK_LEFT:
		case VK_KP_LEFT:
			if (insertPos > 0) {
				doLeftCursor();
				insertPos--;
			}
			break;
		case VK_F1:
		case VK_F2:
		case VK_F3:
		case VK_F4:
		case VK_F5:
		case VK_F6:
		case VK_F7:
		case VK_F8:
		case VK_F9:
		case VK_F10:
		case VK_F11:
		case VK_F12: {
			uint8_t fkey = ev.vkey - VK_F1;
			if (enableHotkeys && handleHotkey(fkey, buffer, bufferLength, insertPos, len)) {
				len = strlen(buffer);
				insertPos = len;
				keya = 0x0D;
				// Key was present, so drop through to ASCII key handling
			} else
				break; // key wasn't present, so do nothing
		}

		//
		// Now the ASCII keys
		//
		default:
			if (keya == 0)
				break;
			if (keya >= 0x20 && keya != 0x7F) {
				if (insertCharacter(buffer, bufferLength, keya, insertPos)) {
					insertPos++;
				}
			} else {
				switch (keya) {
				case 0x1:		   // CTRL-A
					insertPos = gotoEditLineStart(insertPos);
					break;
				case 0x2:		   // ctrl-b
					if (insertPos > 0) {
						doLeftCursor();
						insertPos--;
					}
					break;
				case 0x5:		   // CTRL-E
					insertPos = gotoEditLineEnd(insertPos, len);
					break;
				case 0x09:		   // Tab
					if (enableTab) {
						do_tab_complete(buffer, bufferLength, &insertPos);
						len = strlen(buffer);
					}
					break;
				case 0x0A:		   // Cursor Down
					historyAction = 3;
					break;
				case 0x0B:		   // Cursor Up
					historyAction = 2;
					break;
				case 0x0D:		   // Enter
					historyAction = 1;
					keyr = keya;
					break;
				case 0x0E:		   // CTRL-N
					historyAction = 3; // Next history item
					break;
				case 0x10:		   // CTRL-P
					historyAction = 2; // Previous history item
					break;
				case 0x6:		   // ctrl-f
				case 0x15:		   // Cursor Right
					if (insertPos < len) {
						doRightCursor();
						insertPos++;
					}
					break;
				case 0x17:		   // CTRL-W
					// Delete last word
					insertPos -= deleteWord(buffer, insertPos, len);
					break;
				case 0x1B: // Escape
					keyr = keya;
					break;
				case 0x08: // CTRL-h
				case 0x7F: // Backspace
					if (deleteCharacter(buffer, insertPos, len)) {
						insertPos--;
					}
					break;
				}
			}
		}

		if (enableHistory) {
			bool lineChanged = false;
			switch (historyAction) {
			case 1:				    // Push new item to stack
				editHistoryPush(buffer, bufferLength);
				break;
			case 2:				    // Move up in history
				lineChanged = editHistoryUp(buffer, bufferLength, insertPos, len);
				break;
			case 3:				    // Move down in history
				lineChanged = editHistoryDown(buffer, bufferLength, insertPos, len);
				break;
			}

			if (lineChanged) {
				printf("%s", buffer);	    // Output the buffer
				insertPos = strlen(buffer); // Set cursor to end of string
				len = strlen(buffer);
			}
		}
	}
	len -= insertPos;				    // Now just need to cursor to end of line; get # of characters to cursor

	while (len >= scrcols) {			    // First cursor down if possible
		putch(0x0A);
		len -= scrcols;
	}
	while (len-- > 0)
		putch(0x09);				    // Then cursor right for the remainder

	return keyr;					    // Finally return the keycode
}

void editHistoryInit()
{
	int i;
	history_no = 0;
	history_size = 0;

	for (i = 0; i < cmd_historyDepth; i++) {
		cmd_history[i] = NULL;
	}
}

static void editHistoryPush(char *buffer, int buffer_capacity)
{
	int len = strnlen(buffer, buffer_capacity);

	if (len > 0) { // If there is data in the buffer
		char *newEntry = NULL;

		// if the new entry is the same as the last entry, then don't save it
		if (history_size > 0 && strcmp(buffer, cmd_history[history_size - 1]) == 0) {
			return;
		}

		newEntry = mos_strndup(buffer, buffer_capacity);

		// If we're at the end of the history, then we need to shift all our entries up by one
		if (history_size == cmd_historyDepth) {
			int i;
			umm_free(cmd_history[0]);
			for (i = 1; i < history_size; i++) {
				cmd_history[i - 1] = cmd_history[i];
			}
			history_size--;
		}
		cmd_history[history_size++] = newEntry;
	}
}

static bool editHistoryUp(char *buffer, int buffer_capacity, int insertPos, int len)
{
	int index = -1;
	if (history_no > 0) {
		index = history_no - 1;
	} else if (history_size > 0) {
		// we're at the top of our history list
		// replace current line (which may have been edited) with first entry
		index = 0;
	}
	return editHistorySet(buffer, buffer_capacity, insertPos, len, index);
}

static bool editHistoryDown(char *buffer, int buffer_capacity, int insertPos, int len)
{
	if (history_no < history_size) {
		if (history_no == history_size - 1) {
			// already at most recent entry - just leave an empty line
			removeEditLine(buffer, insertPos, len);
			history_no = history_size;
			return true;
		}
		return editHistorySet(buffer, buffer_capacity, insertPos, len, ++history_no);
	}
	return false;
}

static bool editHistorySet(char *buffer, int buffer_capacity, int insertPos, int len, int index)
{
	if (index >= 0 && index < history_size) {
		removeEditLine(buffer, insertPos, len);
		buffer[0] = 0;
		strbuf_append(buffer, buffer_capacity, cmd_history[index], buffer_capacity - 1);
		history_no = index;
		return true;
	}
	return false;
}
