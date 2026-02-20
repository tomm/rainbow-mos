/*
 * Title:			AGON MOS - MOS code
 * Author:			Dean Belfield
 * Created:			10/07/2022
 * Last Updated:	11/11/2023
 *
 * Modinfo:
 * 11/07/2022:		Added mos_cmdDIR, mos_cmdLOAD, removed mos_cmdBYE
 * 12/07/2022:		Added mos_cmdJMP
 * 13/07/2022:		Added mos_cmdSAVE, mos_cmdDEL, improved command parsing and file error reporting
 * 14/07/2022:		Added mos_cmdRUN
 * 25/07/2022:		Added mos_getkey; variable keycode is now declared as a volatile
 * 03/08/2022:		Added a handful of MOS API calls
 * 05/08/2022:		Added mos_FEOF
 * 05/09/2022:		Added mos_cmdREN, mos_cmdBOOT; moved mos_EDITLINE into mos_editline.c, default args for LOAD and RUN commands
 * 25/09/2022:		Added mos_GETERROR, mos_MKDIR; mos_input now sets first byte of buffer to 0
 * 03/10/2022:		Added mos_cmdSET
 * 13/10/2022:		Added mos_OSCLI and supporting code
 * 20/10/2022:		Tweaked error handling
 * 08/11/2022:		Fixed return value bug in mos_cmdRUN
 * 13/11/2022:		Case insensitive command processing with abbreviations; mos_exec now runs commands off SD card
 * 19/11/2022:		Added support for passing params to executables & ADL mode
 * 14/02/2023:		Added mos_cmdVDU, support for more keyboard layouts in mos_cmdSET
 * 20/02/2023:		Function mos_getkey now returns a uint8_t
 * 12/03/2023:		Renamed keycode to keyascii, keyascii now a uint8_t, added mos_cmdTIME, mos_cmdCREDITS, mos_DIR now accepts a path
 * 15/03/2023:		Added mos_cmdCOPY, mos_COPY, mos_GETRTC, aliase for mos_REN, made error messages a bit more user friendly
 * 19/03/2023:		Fixed compilation warnings in mos_cmdTIME
 * 21/03/2023:		Added mos_SETINTVECTOR, uses VDP values from defines.h
 * 26/03/2023:		Fixed SET KEYBOARD command
 * 14/04/2023:		Added fat_EOF
 * 15/04/2023:		Added mos_GETFIL, mos_FREAD, mos_FWRITE, mos_FLSEEK, refactored MOS file commands
 * 30/05/2023:		Fixed bug in mos_parseNumber to detect invalid numeric characters, mos_FGETC now returns EOF flag
 * 08/07/2023:		Added mos_trim function; mos_exec now trims whitespace from input string, various bug fixes
 * 15/09/2023:		Function mos_trim now includes the asterisk character as whitespace
 * 26/09/2023:		Refactored mos_GETRTC and mos_SETRTC
 * 10/11/2023:		Added CONSOLE to mos_cmdSET
 * 11/11/2023:		Added mos_cmdHELP, mos_cmdTYPE, mos_cmdCLS, mos_cmdMOUNT, mos_mount
 */

#include "defines.h"
#include "ez80f92.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bootmsg.h"
#include "clock.h"
#include "config.h"
#include "console.h"
#include "defines.h"
#include "keyboard_buffer.h"
#include "mos.h"
#include "mos_editor.h"
#include "strings.h"
#include "uart.h"
#ifdef FEAT_FRAMEBUFFER
#include "fbconsole.h"
#include "formatting.h"
#include "globals.h"
#include "vec.h"
#endif								     /* FEAT_FRAMEBUFFER */
#ifdef DEBUG
#include "tests.h"
#endif								     /* DEBUG */

char cmd[256];							     // Array for the command line handler

extern void *set_vector(unsigned int vector, void (*handler)(void)); // In vectors16.asm

extern int exec16(uint24_t addr, char *params);			     // In misc.asm
extern int exec24(uint24_t addr, char *params);			     // In misc.asm

extern uint8_t rtc;						     // In globals.asm

static FATFS fs;						     // Handle for the file system
static char *mos_strtok_ptr;					     // Pointer for current position in string tokeniser

char *cwd;							     // Hold current working directory.
bool sdcardDelay = false;

static FIL *mosFileObjects[MOS_maxOpenFiles];

bool vdpSupportsTextPalette = false;

// Array of MOS commands and pointer to the C function to run
// NB this list is iterated over, so the order is important
// for the help command
//
static const t_mosCommand mosCommands[] = {
	{ "CAT", &mos_cmdDIR, HELP_CAT_ARGS, HELP_CAT },
	{ "CD", &mos_cmdCD, HELP_CD_ARGS, HELP_CD },
	{ "CDIR", &mos_cmdCD, HELP_CD_ARGS, HELP_CD },
	{ "CLS", &mos_cmdCLS, NULL, HELP_CLS },
	{ "COPY", &mos_cmdCOPY, HELP_COPY_ARGS, HELP_COPY },
	{ "CP", &mos_cmdCOPY, HELP_COPY_ARGS, HELP_COPY },
	{ "CREDITS", &mos_cmdCREDITS, NULL, HELP_CREDITS },
	{ "DELETE", &mos_cmdDEL, HELP_DELETE_ARGS, HELP_DELETE },
	{ "DIR", &mos_cmdDIR, HELP_CAT_ARGS, HELP_CAT },
	{ "DISC", &mos_cmdDISC, NULL, NULL },
	{ "ECHO", &mos_cmdECHO, HELP_ECHO_ARGS, HELP_ECHO },
	{ "ERASE", &mos_cmdDEL, HELP_DELETE_ARGS, HELP_DELETE },
	{ "EXEC", &mos_cmdEXEC, HELP_EXEC_ARGS, HELP_EXEC },
	{ "FBMODE", &mos_cmdFBMODE, HELP_FBMODE_ARGS, HELP_FBMODE },
	{ "HELP", &mos_cmdHELP, HELP_HELP_ARGS, HELP_HELP },
	{ "JMP", &mos_cmdJMP, HELP_JMP_ARGS, HELP_JMP },
	{ "LOAD", &mos_cmdLOAD, HELP_LOAD_ARGS, HELP_LOAD },
	{ "LS", &mos_cmdDIR, HELP_CAT_ARGS, HELP_CAT },
	{ "HOTKEY", &mos_cmdHOTKEY, HELP_HOTKEY_ARGS, HELP_HOTKEY },
	{ "MEM", &mos_cmdMEM, NULL, HELP_MEM },
	{ "MEMDUMP", &mos_cmdMEMDUMP, HELP_MEMDUMP_ARGS, HELP_MEMDUMP },
	{ "MKDIR", &mos_cmdMKDIR, HELP_MKDIR_ARGS, HELP_MKDIR },
	{ "MOUNT", &mos_cmdMOUNT, NULL, HELP_MOUNT },
	{ "MOVE", &mos_cmdREN, HELP_RENAME_ARGS, HELP_RENAME },
	{ "MV", &mos_cmdREN, HELP_RENAME_ARGS, HELP_RENAME },
	{ "PRINTF", &mos_cmdPRINTF, HELP_PRINTF_ARGS, HELP_PRINTF },
	{ "RENAME", &mos_cmdREN, HELP_RENAME_ARGS, HELP_RENAME },
	{ "RM", &mos_cmdDEL, HELP_DELETE_ARGS, HELP_DELETE },
	{ "RUN", &mos_cmdRUN, HELP_RUN_ARGS, HELP_RUN },
	{ "SAVE", &mos_cmdSAVE, HELP_SAVE_ARGS, HELP_SAVE },
	{ "SIDELOAD", &mos_cmdSIDELOAD, NULL, NULL },
	{ "SET", &mos_cmdSET, HELP_SET_ARGS, HELP_SET },
	{ "TIME", &mos_cmdTIME, HELP_TIME_ARGS, HELP_TIME },
	{ "TYPE", &mos_cmdTYPE, HELP_TYPE_ARGS, HELP_TYPE },
	{ "VDU", &mos_cmdVDU, HELP_VDU_ARGS, HELP_VDU },
#ifdef DEBUG
	{ "RUN_MOS_TESTS", &mos_cmdTEST, NULL, "Run the MOS OS test suite" },
#endif /* DEBUG */
};

#define mosCommands_count (sizeof(mosCommands) / sizeof(t_mosCommand))

// Array of file errors; mapped by index to the error numbers returned by FatFS
//
static const char *mos_errors[] = {
	"OK",
	"Error accessing SD card",
	"Assertion failed",
	"SD card failure",
	"Could not find file",
	"Could not find path",
	"Invalid path name",
	"Access denied or directory full",
	"Access denied",
	"Invalid file/directory object",
	"SD card is write protected",
	"Logical drive number is invalid",
	"Volume has no work area",
	"No valid FAT volume",
	"Error occurred during mkfs",
	"Volume timeout",
	"Volume locked",
	"LFN working buffer could not be allocated",
	"Too many open files",
	"Invalid parameter",
	// MOS-specific errors beyond this point (index 20+)
	"Invalid command",
	"Invalid executable",
	"Out of memory",
	"Not implemented",
	"Load overlaps system area",
	"Bad string",
	"Invalid parameter",
};

#define mos_errors_count (sizeof(mos_errors) / sizeof(char *))

// Output a file error
// Parameters:
// - error: The FatFS error number
//
void mos_error(MOSRESULT error)
{
	if (error >= 0 && error < mos_errors_count) {
		printf("\n\r%s\n\r", mos_errors[error]);
	}
}

static void update_cwd()
{
	char buf[256];

	DEBUG_STACK();

	f_getcwd(buf, sizeof(buf));
	if (cwd) umm_free(cwd);
	cwd = mos_strndup(buf, sizeof(buf));
}

// Wait for a keycode character from the VPD
// Returns:
// - ASCII keycode
//
uint8_t mos_getkey()
{
	uint8_t ch = 0;
	while (ch == 0) {      // Loop whilst no key pressed
		ch = keyascii; // Variable keyascii is updated by interrupt
	}
	keyascii = 0;	       // Reset keycode to debounce the key
	return ch;
}

// Call the line editor from MOS
// Used by main loop to get input from the user
// Parameters:
// - buffer: Pointer to the line edit buffer
// - bufferLength: Size of the line edit buffer in bytes
// Returns:
// - The keycode (ESC or CR)
//
uint24_t mos_input(char *buffer, int bufferLength)
{
	int24_t retval;
	mos_print_prompt();
	retval = mos_EDITLINE(buffer, bufferLength, 3);
	printf("\n\r");
	return retval;
}

void mos_print_prompt(void)
{
	uint8_t oldTextFg = active_console->get_fg_color_index();
	set_color(get_primary_color());
	printf("%s %c", cwd, MOS_prompt);
	set_color(oldTextFg);
}

/**
 * Return number of characters appended to buffer
 */
void try_tab_expand_internal_cmd(struct tab_expansion_context *ctx)
{
	uint24_t i;
	t_mosCommand *cmd;
	for (i = 0; i < mosCommands_count; i++) {
		if (strncasecmp(ctx->cmdline, mosCommands[i].name, ctx->cmdline_insertpos) == 0) {
			notify_tab_expansion(ctx, ExpandNormal, mosCommands[i].name, strlen(mosCommands[i].name), mosCommands[i].name + ctx->cmdline_insertpos, strlen(mosCommands[i].name) - ctx->cmdline_insertpos);
		}
	}
}

// Parse a MOS command from the line edit buffer
// Parameters:
// - ptr: Pointer to the MOS command in the line edit buffer
// Returns:
// - Function pointer, or 0 if command not found
//
const t_mosCommand *mos_getCommand(char *ptr)
{
	uint24_t i;
	const t_mosCommand *cmd;
	for (i = 0; i < mosCommands_count; i++) {
		cmd = &mosCommands[i];
		if (strncasecmp(cmd->name, ptr, 256) == 0) {
			// return cmd->func;
			return cmd;
		}
	}
	return NULL;
}

// String trim function
// NB: This also includes the asterisk character as whitespace
// Parameters:
// - s: Pointer to the string to trim
// Returns:
// - s: Pointer to the start of the new string
//
char *mos_trim(char *s)
{
	char *ptr;

	if (!s) {			   // Return NULL if a null string is passed
		return NULL;
	}
	if (!*s) {
		return s;		   // Handle empty string
	}
	while (isspace(*s) || *s == '*') { // Advance the pointer to the first non-whitespace or asterisk character in the string
		s++;
	}
	ptr = s + strlen(s) - 1;
	while (ptr > s && isspace(*ptr)) {
		ptr--;
	}
	ptr[1] = '\0';
	return s;
}

// String tokeniser
// Parameters:
// - s1: String to tokenise
// - s2: Delimiter
// - ptr: Pointer to store the current position in (mos_strtok_r)
// Returns:
// - Pointer to tokenised string
//
char *mos_strtok(char *s1, char *s2)
{
	return mos_strtok_r(s1, s2, &mos_strtok_ptr);
}

char *mos_strtok_r(char *s1, const char *s2, char **ptr)
{
	char *end;

	if (s1 == NULL) {
		s1 = *ptr;
	}

	if (*s1 == '\0') {
		*ptr = s1;
		return NULL;
	}
	// Scan leading delimiters
	//
	s1 += strspn(s1, s2);
	if (*s1 == '\0') {
		*ptr = s1;
		return NULL;
	}
	// Find the end of the token
	//
	end = s1 + strcspn(s1, s2);
	if (*end == '\0') {
		*ptr = end;
		return s1;
	}
	// Terminate the token and make *SAVE_PTR point past it
	//
	*end = '\0';
	*ptr = end + 1;

	return s1;
}

// Parse a number from the line edit buffer
// Parameters:
// - ptr: Pointer to the number in the line edit buffer
// - p_Value: Pointer to the return value
// Returns:
// - true if the function succeeded, otherwise false
//
bool mos_parseNumber(char *ptr, uint24_t *p_Value)
{
	char *p = ptr;
	char *e;
	int base = 10;
	long value;

	p = mos_strtok(p, " ");
	if (p == NULL) {
		return 0;
	}
	if (*p == '&' || *p == '$') {
		base = 16;
		p++;
	}
	if (*p == '0' && tolower(p[1]) == 'x') {
		base = 16;
		p += 2;
	}
	value = strtol(p, &e, base);
	if (*e != 0) {
		return 0;
	}
	*p_Value = value;
	return 1;
}

// Parse a string from the line edit buffer
// Parameters:
// - ptr: Pointer to the string in the line edit buffer
// - p_Value: Pointer to the return value
// Returns:
// - true if the function succeeded, otherwise false
//
bool mos_parseString(char *ptr, char **p_Value)
{
	char *p = ptr;

	p = mos_strtok(p, " ");
	if (p == NULL) {
		return 0;
	}
	*p_Value = p;
	return 1;
}

int mos_runBin(uint24_t addr)
{
	uint8_t mode = mos_execMode((uint8_t *)addr);
	switch (mode) {
	case 0:	 // Z80 mode
		return exec16(addr, mos_strtok_ptr);
		break;
	case 1:	 // ADL mode
		return exec24(addr, mos_strtok_ptr);
		break;
	default: // Unrecognised header
		return MOS_INVALID_EXECUTABLE;
		break;
	}
}

// Execute a MOS command
// Parameters:
// - buffer: Pointer to a zero terminated string that contains the MOS command with arguments
// Returns:
// - MOS error code
//
int mos_exec(char *buffer, bool in_mos)
{
	char *ptr;
	int fr = 0;
	char path[256];
	uint8_t mode;
	const t_mosCommand *cmd;

	ptr = mos_trim(buffer);
	if (ptr != NULL && *ptr == '#') {
		return FR_OK;
	}

	ptr = mos_strtok(ptr, " ");
	if (ptr == NULL) return fr;

	cmd = mos_getCommand(ptr);
	if (cmd != NULL && cmd->func != 0) {
		return cmd->func(ptr);
	}

	// some absolute or relative path. try to load directly
	if (strchr(ptr, '/')) {
		fr = mos_LOAD(ptr, MOS_defaultLoadAddress, 0);
		if (fr == FR_OK) {
			return mos_runBin(MOS_defaultLoadAddress);
		} else {
			return fr;
		}
	}

	snprintf(path, sizeof(path), "/mos/%s.bin", ptr);
	fr = mos_LOAD(path, MOS_starLoadAddress, 0);
	if (fr == FR_OK) {
		return mos_runBin(MOS_starLoadAddress);
	}
	if (fr == MOS_OVERLAPPING_SYSTEM) {
		return fr;
	}

	if (in_mos) {
		snprintf(path, sizeof(path), "%s.bin", ptr);
		fr = mos_LOAD(path, MOS_defaultLoadAddress, 0);
		if (fr == FR_OK) {
			return mos_runBin(MOS_defaultLoadAddress);
		}
		if (fr == MOS_OVERLAPPING_SYSTEM) {
			return fr;
		}
		snprintf(path, sizeof(path), "/bin/%s.bin", ptr);
		fr = mos_LOAD(path, MOS_defaultLoadAddress, 0);
		if (fr == FR_OK) {
			return mos_runBin(MOS_defaultLoadAddress);
		}
		if (fr == MOS_OVERLAPPING_SYSTEM) {
			return fr;
		}
	}
	if (fr == FR_NO_FILE || fr == FR_NO_PATH) {
		return MOS_INVALID_COMMAND;
	}
	return fr;
}

// Get the MOS Z80 execution mode
// Parameters:
// - ptr: Pointer to the code block
// Returns:
// - 0: Z80 mode
// - 1: ADL mode
//
uint8_t mos_execMode(uint8_t *ptr)
{
	if (
	    *(ptr + 0x40) == 'M' && *(ptr + 0x41) == 'O' && *(ptr + 0x42) == 'S') {
		return *(ptr + 0x44);
	}
	return 0xFF;
}

int mos_cmdDISC(char *ptr)
{
	sdcardDelay = true;
	return 0;
}

// DIR command
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdDIR(char *ptr)
{
	bool longListing = false;
	char *path;

	for (;;) {
		if (!mos_parseString(NULL, &path)) {
			return mos_DIR(".", longListing);
		}
		if (strcasecmp(path, "-l") == 0) {
			longListing = true;
		} else {
			break;
		}
	}
	return mos_DIR(path, longListing);
}

// Assumes isxdigit(digit)
static int xdigit_to_int(char digit)
{
	digit = toupper(digit);
	if (digit < 'A') {
		return digit - '0';
	} else {
		return digit - 55;
	}
}

int mos_cmdECHO(char *ptr)
{
	int ret = mos_cmdPRINTF(ptr);
	putch('\r');
	putch('\n');
	return ret;
}

// PRINTF command
//
int mos_cmdPRINTF(char *ptr)
{
	int c;
	const char *p = mos_strtok_ptr;

	while (*p) {
		switch (*p) {
		case '\\': {
			// interpret escaped characters
			p++;
			if (*p == '\\') {
				putch('\\');
				p++;
			} else if (*p == 'r') {
				putch('\r');
				p++;
			} else if (*p == 'n') {
				putch('\n');
				p++;
			} else if (*p == 'f') {
				putch(12);
				p++;
			} else if (*p == 't') {
				putch('\t');
				p++;
			} else if (*p == 'x') {
				p++;
				c = 0;
				if (isxdigit(*p)) {
					c = xdigit_to_int(*p);
					p++;
					if (isxdigit(*p)) {
						c = c * 16 + xdigit_to_int(*p);
						p++;
					}
				}
				putch(c);
			} else {
				// invalid. skip it entirely
				if (*p) p++;
			}
			break;
		}
		default:
			putch(*p);
			p++;
			break;
		}
	}

	return 0;
}

// HOTKEY command
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdHOTKEY(char *ptr)
{
	uint24_t fn_number = 0;
	char *hotkey_string;

	if (!mos_parseNumber(NULL, &fn_number)) {
		uint8_t key;
		printf("Hotkey assignments:\r\n\r\n");

		for (key = 0; key < 12; key++) {
			printf("F%d: %s\r\n", key + 1, hotkey_strings[key] == NULL ? "N/A" : hotkey_strings[key]);
		}

		printf("\r\n");
		return 0;
	}

	if (fn_number < 1 || fn_number > 12) {
		printf("Invalid FN-key number.\r\n");
		return 0;
	}

	if (strlen(mos_strtok_ptr) < 1) {
		if (hotkey_strings[fn_number - 1] != NULL) {
			umm_free(hotkey_strings[fn_number - 1]);
			hotkey_strings[fn_number - 1] = NULL;
			printf("F%u cleared.\r\n", fn_number);
		} else
			printf("F%u already clear, no hotkey command provided.\r\n", fn_number);

		return 0;
	}

	if (mos_strtok_ptr[0] == '\"' && mos_strtok_ptr[strlen(mos_strtok_ptr) - 1] == '\"') {
		mos_strtok_ptr[strlen(mos_strtok_ptr) - 1] = '\0';
		mos_strtok_ptr++;
	}

	if (hotkey_strings[fn_number - 1] != NULL) umm_free(hotkey_strings[fn_number - 1]);

	hotkey_strings[fn_number - 1] = mos_strndup(mos_strtok_ptr, 256);
	if (!hotkey_strings[fn_number - 1]) return FR_INT_ERR;

	return 0;
}

// LOAD <filename> <addr> command
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdLOAD(char *ptr)
{
	FRESULT fr;
	char *filename;
	uint24_t addr;

	if (
	    !mos_parseString(NULL, &filename)) {
		return FR_INVALID_PARAMETER;
	}
	if (!mos_parseNumber(NULL, &addr)) addr = MOS_defaultLoadAddress;
	fr = mos_LOAD(filename, addr, 0);
	return fr;
}

// EXEC <filename>
//   Run a batch file containing MOS commands
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdEXEC(char *ptr)
{
	FRESULT fr;
	char *filename;
	uint24_t addr;
	char buf[256];

	DEBUG_STACK();

	if (
	    !mos_parseString(NULL, &filename)) {
		return FR_INVALID_PARAMETER;
	}
	fr = mos_EXEC(filename, buf, sizeof buf);
	return fr;
}

// SAVE <filename> <addr> <len> command
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdSAVE(char *ptr)
{
	FRESULT fr;
	char *filename;
	uint24_t addr;
	uint24_t size;

	if (
	    !mos_parseString(NULL, &filename) || !mos_parseNumber(NULL, &addr) || !mos_parseNumber(NULL, &size)) {
		return FR_INVALID_PARAMETER;
	}
	fr = mos_SAVE(filename, addr, size);
	return fr;
}

// DEL <filename> command
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdDEL(char *ptr)
{
	FRESULT fr;
	DIR dir;
	FILINFO fno;
	char *dirPath = NULL;
	char *pattern = NULL;
	bool usePattern = false;
	bool force = false;
	char *filename;
	char *lastSeparator;
	char verify[7];

	DEBUG_STACK();

	if (
	    !mos_parseString(NULL, &filename)) {
		return FR_INVALID_PARAMETER;
	}

	if (strcasecmp(filename, "-f") == 0) {
		force = true;
		if (!mos_parseString(NULL, &filename)) {
			return FR_INVALID_PARAMETER;
		}
	}

	fr = FR_INT_ERR;

	lastSeparator = strrchr(filename, '/');

	if (strchr(filename, '*') != NULL) {
		usePattern = true;
		if (filename[0] == '/' && strchr(filename + 1, '/') == NULL) {
			dirPath = mos_strdup("/");
			if (!dirPath) return FR_INT_ERR;
			if (strchr(filename + 1, '*') != NULL) {
				pattern = mos_strdup(filename + 1);
				if (!pattern) goto cleanup;
			}
		} else if (lastSeparator != NULL) {
			dirPath = mos_strndup(filename, lastSeparator - filename);
			if (!dirPath) return FR_INT_ERR;

			pattern = mos_strdup(lastSeparator + 1);
			if (!pattern) {
				umm_free(dirPath);
				return FR_INT_ERR;
			}
		} else {
			dirPath = mos_strdup(".");
			pattern = mos_strdup(filename);
			if (!dirPath || !pattern) {
				if (dirPath) umm_free(dirPath);
				if (pattern) umm_free(pattern);
				return FR_INT_ERR;
			}
		}
	} else {
		dirPath = mos_strdup(filename);
		if (!dirPath) return FR_INT_ERR;
	}

	if (usePattern) {
		fr = f_opendir(&dir, dirPath);
		if (fr != FR_OK) goto cleanup;

		fr = f_findfirst(&dir, &fno, dirPath, pattern);
		while (fr == FR_OK && fno.fname[0] != '\0') {
			size_t fullPathLen = strlen(dirPath) + strlen(fno.fname) + 2;
			char *fullPath = umm_malloc(fullPathLen);
			if (!fullPath) {
				fr = FR_INT_ERR;
				break;
			}

			snprintf(fullPath, fullPathLen, "%s/%s", dirPath, fno.fname); // Construct full path

			if (!force) {
				int24_t retval;
				// we could potentially support "All" here, and when detected changing `force` to true
				printf("Delete %s? (Yes/No/Cancel) ", fullPath);
				retval = mos_EDITLINE(verify, sizeof(verify), 13);
				printf("\n\r");
				if (retval == 13) {
					if (strcasecmp(verify, "Cancel") == 0 || strcasecmp(verify, "C") == 0) {
						printf("Cancelled.\r\n");
						umm_free(fullPath);
						break;
					}
					if (strcasecmp(verify, "Yes") == 0 || strcasecmp(verify, "Y") == 0) {
						printf("Deleting %s.\r\n", fullPath);
						fr = f_unlink(fullPath);
					}
				} else {
					printf("Cancelled.\r\n");
					umm_free(fullPath);
					break;
				}
			} else {
				printf("Deleting %s\r\n", fullPath);
				fr = f_unlink(fullPath);
			}
			umm_free(fullPath);

			if (fr != FR_OK) break;
			fr = f_findnext(&dir, &fno);
		}

		f_closedir(&dir);
		printf("\r\n");
	} else {
		fr = f_unlink(filename);
	}

cleanup:
	if (dirPath) umm_free(dirPath);
	if (pattern) umm_free(pattern);
	return fr;
}

// JMP <addr> command
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdJMP(char *ptr)
{
	uint24_t addr;
	void (*dest)(void) = 0;
	if (!mos_parseNumber(NULL, &addr)) {
		return FR_INVALID_PARAMETER;
	};
	dest = (void *)addr;
	dest();
	kbuf_clear();
	return 0;
}

// RUN <addr> command
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdRUN(char *ptr)
{
	uint24_t addr;

	if (!mos_parseNumber(NULL, &addr)) {
		addr = MOS_defaultLoadAddress;
	}
	return mos_runBin(addr);
}

// CD <path> command
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdCD(char *ptr)
{
	char *path;

	FRESULT fr;

	if (
	    !mos_parseString(NULL, &path)) {
		return FR_INVALID_PARAMETER;
	}
	fr = f_chdir(path);
	update_cwd();
	return fr;
}

// REN <filename1> <filename2> command
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdREN(char *ptr)
{
	FRESULT fr;
	char *filename1;
	char *filename2;

	if (
	    !mos_parseString(NULL, &filename1) || !mos_parseString(NULL, &filename2)) {
		return FR_INVALID_PARAMETER;
	}
	fr = mos_REN(filename1, filename2, true);
	return fr;
}

// COPY <filename1> <filename2> command
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdCOPY(char *ptr)
{
	FRESULT fr;
	char *filename1;
	char *filename2;

	if (
	    !mos_parseString(NULL, &filename1) || !mos_parseString(NULL, &filename2)) {
		return FR_INVALID_PARAMETER;
	}
	fr = mos_COPY(filename1, filename2, true);
	return fr;
}

// MKDIR <filename> command
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdMKDIR(char *ptr)
{
	char *filename;

	FRESULT fr;

	if (
	    !mos_parseString(NULL, &filename)) {
		return FR_INVALID_PARAMETER;
	}
	fr = mos_MKDIR(filename);
	return fr;
}

// SET <option> <value> command
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdSET(char *ptr)
{
	char *command;
	uint24_t value;

	if (
	    !mos_parseString(NULL, &command) || !mos_parseNumber(NULL, &value)) {
		return FR_INVALID_PARAMETER;
	}
	if (strcasecmp(command, "KEYBOARD") == 0) {
		putch(23);
		putch(0);
		putch(VDP_keycode);
		putch(value & 0xFF);
		return 0;
	}
	if (strcasecmp(command, "CONSOLE") == 0 && value <= 1) {
		putch(23);
		putch(0);
		putch(VDP_consolemode);
		putch(value & 0xFF);
		return 0;
	}
	return FR_INVALID_PARAMETER;
}

// VDU <char1> <char2> ... <charN>
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdVDU(char *ptr)
{
	char *value_str;
	uint24_t value = 0;

	while (mos_parseString(NULL, &value_str)) {
		uint8_t is_word = 0;
		uint8_t base = 10;
		char *endPtr;
		size_t len = strlen(value_str);

		// Strip semicolon notation and set as word
		if (len > 0 && value_str[len - 1] == ';') {
			value_str[len - 1] = '\0';
			len--;
			is_word = 1;
		}

		// Check for '0x' or '0X' prefix
		if (len > 2 && (value_str[0] == '0' && tolower(value_str[1] == 'x'))) {
			base = 16;
		}

		// Check for '&' prefix
		if (value_str[0] == '&') {
			base = 16;
			value_str++;
			len--;
		}
		// Check for 'h' suffix
		if (len > 0 && tolower(value_str[len - 1]) == 'h') {
			value_str[len - 1] = '\0';
			base = 16;
		}

		value = strtol(value_str, &endPtr, base);

		if (*endPtr != '\0' || value > 65535) {
			return FR_INVALID_PARAMETER;
		}

		if (value > 255) {
			is_word = 1;
		}

		if (is_word) {
			putch(value & 0xFF); // write LSB
			putch(value >> 8);   // write MSB
		} else {
			putch(value);
		}
	}

	return 0;
}

// TIME
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - 0
//
int mos_cmdTIME(char *ptr)
{
	uint24_t yr, mo, da, ho, mi, se;
	char buffer[64];

	// If there is a first parameter
	//
	if (mos_parseNumber(NULL, &yr)) {
		//
		// Fetch the rest of the parameters
		//
		if (
		    !mos_parseNumber(NULL, &mo) || !mos_parseNumber(NULL, &da) || !mos_parseNumber(NULL, &ho) || !mos_parseNumber(NULL, &mi) || !mos_parseNumber(NULL, &se)) {
			return FR_INVALID_PARAMETER;
		}
		buffer[0] = yr - EPOCH_YEAR;
		buffer[1] = mo;
		buffer[2] = da;
		buffer[3] = ho;
		buffer[4] = mi;
		buffer[5] = se;
		mos_SETRTC((uint24_t)buffer);
	}
	// Return the new time
	//
	mos_GETRTC(buffer);
	printf("%s\n\r", buffer);
	return 0;
}

extern uint8_t sysvars[];

// MEM
// Returns:
// - MOS error code
//
int mos_cmdMEM(char *ptr)
{
	int try_len = HEAP_LEN;

	printf("ROM      &000000-&01ffff     %2d%% used\r\n", ((int)__rodata_end + (int)__data_len) / 1311);
	if (fb_mode != 255) {
		printf("USER:LO  &%06x-&%06x %6d bytes\r\n", 0x40000, (int)fb_base - 1, (int)fb_base - 0x40000);
		printf("FRAMEBUF &%06x-&%06x %6d bytes\r\n", (uint24_t)fb_base, (int)_stack - SPL_STACK_SIZE - 1, (int)_stack - SPL_STACK_SIZE - (int)fb_base);
	} else {
		printf("USER:LO  &%06x-&%06x %6d bytes\r\n", 0x40000, (int)_stack - SPL_STACK_SIZE - 1, (int)_stack - SPL_STACK_SIZE - 0x40000);
	}
	printf("STACK24  &%06x-&%06x %6d bytes\r\n", (int)_stack - SPL_STACK_SIZE, (int)_stack - 1, SPL_STACK_SIZE);
	// data and bss together
	printf("MOS:DATA &%06x-&%06x %6d bytes\r\n", (int)__data_start, (int)__heapbot - 1, (int)__heapbot - (int)__data_start);
	printf("MOS:HEAP &%06x-&%06x %6d bytes\r\n", (int)__heapbot, (int)__heaptop - 1, HEAP_LEN);
	printf("RESERVED &b7e000-&b7ffff   8192 bytes\r\n");
	printf("\r\n");

	// find largest kmalloc contiguous region
	for (; try_len > 0; try_len -= 8) {
		void *p = umm_malloc(try_len);
		if (p) {
			umm_free(p);
			break;
		}
	}

	printf("Largest free MOS:HEAP fragment: %d b\r\n", try_len);
	printf("Sysvars at &%06x\r\n", (uint24_t)sysvars);
#ifdef DEBUG
	printf("Stack highwatermark: &%06x (%d b)\r\n", stack_highwatermark, (uint24_t)_stack - stack_highwatermark);
#endif /* DEBUG */
	printf("\r\n");

	return 0;
}

int mos_cmdMEMDUMP(char *ptr)
{
	size_t addr, len;
	if (!mos_parseNumber(NULL, &addr)) {
		return FR_INVALID_PARAMETER;
	}
	if (!mos_parseNumber(NULL, &len)) {
		len = 0x100;
	}
	size_t i = 0;
	const int width = scrcols <= 40 ? 8 : 16;

	paginated_start(true);

	while (i < len) {
		paginated_printf("%06x:", addr + i);
		for (int c = 0; c < width; c++) {
			if ((c & 3) == 0) putch(' ');
			paginated_printf("%02x", *(uint8_t *)(addr + i + c));
		}
		putch(' ');
		for (int c = 0; c < width; c++) {
			putch(27);
			putch(*(uint8_t *)(addr + i + c));
		}
		paginated_printf("\r\n");
		if (paginated_exit) break;
		i += width;
	}
	return 0;
}

// CREDITS
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdCREDITS(char *ptr)
{
	mos_bootmsg();
	printf("Agon Quark MOS (c) 2022 Dean Belfield\n\r");
	printf("FabGL 1.0.8 (c) 2019-2022 by Fabrizio Di Vittorio\n\r");
	printf("FatFS R0.14b (c) 2021 ChaN\n\r");
	printf("umm_malloc Copyright (c) 2015 Ralph Hempel\n\r");
	printf("\n\r");
#ifdef DEBUG
	printf("This is a DEBUG build\r\n");
#endif /* DEBUG */
	return 0;
}

// TYPE <filename>
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdTYPE(char *ptr)
{
	FRESULT fr;
	char *filename;

	if (!mos_parseString(NULL, &filename))
		return FR_INVALID_PARAMETER;

	fr = mos_TYPE(filename);
	return fr;
}

// CLS
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdCLS(char *ptr)
{
	putch(12);
	return 0;
}

// MOUNT
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// - MOS error code
//
int mos_cmdMOUNT(char *ptr)
{
	int fr;

	fr = mos_mount();
	if (fr != FR_OK)
		mos_error(fr);
	update_cwd();
	return 0;
}

static void printCommandInfo(const t_mosCommand *cmd, bool full)
{
	int aliases = 0;
	size_t i;

	if (cmd->help == NULL) return;

	paginated_printf("%s", cmd->name);
	if (cmd->args != NULL)
		paginated_printf(" %s", cmd->args);

	// find aliases
	for (i = 0; i < mosCommands_count; ++i) {
		if (mosCommands[i].func == cmd->func && mosCommands[i].name != cmd->name) {
			aliases++;
		}
	}
	if (aliases > 0) {
		// print the aliases
		paginated_printf(" (Aliases: ");
		for (i = 0; i < mosCommands_count; ++i) {
			if (mosCommands[i].func == cmd->func && mosCommands[i].name != cmd->name) {
				paginated_printf("%s", mosCommands[i].name);
				if (aliases == 2) {
					paginated_printf(" and ");
				} else if (aliases > 1) {
					paginated_printf(", ");
				}
				aliases--;
			}
		}
		paginated_printf(")");
	}

	paginated_printf("\n");
	if (full) {
		paginated_printf("%s\n", cmd->help);
	}
}

// HELP
// Parameters:
// - ptr: Pointer to the argument string in the line edit buffer
// Returns:
// -  0: Success
//
int mos_cmdHELP(char *ptr)
{
	size_t i;
	char *cmd;

	bool hasCmd = mos_parseString(NULL, &cmd);
	if (!hasCmd) {
		cmd = "help";
	}

	paginated_start(true);

	for (i = 0; i < mosCommands_count; ++i) {
		if (strcasecmp(cmd, mosCommands[i].name) == 0) {
			printCommandInfo(&mosCommands[i], true);
			if (!hasCmd) {
				// must be showing "help" command with no args, so show list of all commands
				size_t col = 0;
				size_t maxCol = scrcols;
				paginated_printf("List of commands:\n");
				for (i = 1; i < mosCommands_count; ++i) {
					if (mosCommands[i].help == NULL) continue;
					if (col + strlen(mosCommands[i].name) + 2 >= maxCol) {
						paginated_printf("\n");
						col = 0;
					}
					paginated_printf("%s", mosCommands[i].name);
					if (i < mosCommands_count - 1) {
						paginated_printf(", ");
					}
					col += strlen(mosCommands[i].name) + 2;
				}
				paginated_printf("\n");
			}
			return 0;
		}
	}

	if (hasCmd && strcasecmp(cmd, "all") == 0) {
		for (i = 0; i < mosCommands_count; ++i) {
			printCommandInfo(&mosCommands[i], false);
			if (paginated_exit) break;
		}
		return 0;
	}

	paginated_printf("Command not found: %s\n", cmd);
	return 0;
}

// Load a file from SD card to memory
// Parameters:
// - filename: Path of file to load
// - address: Address in RAM to load the file into
// - size: Number of bytes to load, 0 for maximum file size
// Returns:
// - FatFS return code
//
uint24_t mos_LOAD(char *filename, uint24_t address, uint24_t size)
{
	FRESULT fr;
	FIL fil;
	UINT br;
	FSIZE_t fSize;

	fr = f_open(&fil, filename, FA_READ);
	if (fr == FR_OK) {
		fSize = f_size(&fil);
		if (size) {
			// Maximize load according to size parameter
			if (fSize < size) size = fSize;
		} else {
			// Load the full file size
			size = fSize;
		}
		// Check potential system area overlap
		if ((address <= MOS_externLastRAMaddress) && ((address + size) > (size_t)__MOS_systemAddress)) {
			fr = (FRESULT)MOS_OVERLAPPING_SYSTEM;
		} else {
			fr = f_read(&fil, (void *)address, size, &br);
		}
	}
	f_close(&fil);
	return fr;
}

// Save a file from memory to SD card
// Parameters:
// - filename: Path of file to save
// - address: Address in RAM to save the file from
// - size: Number of bytes to save
// Returns:
// - FatFS return code
//
uint24_t mos_SAVE(char *filename, uint24_t address, uint24_t size)
{
	FRESULT fr;
	FIL fil;
	UINT br;

	fr = f_open(&fil, filename, FA_WRITE | FA_CREATE_NEW);
	if (fr == FR_OK) {
		fr = f_write(&fil, (void *)address, size, &br);
	}
	f_close(&fil);
	return fr;
}

// Display a file from SD card on the screen
// Parameters:
// - filename: Path of file to load
// Returns:
// - FatFS return code
//
uint24_t mos_TYPE(char *filename)
{
	FRESULT fr;
	FIL fil;
	UINT br;
	char buf[512];
	int i;
	struct keyboard_event_t ev;
	bool do_wait = false;

	DEBUG_STACK();

	fr = f_open(&fil, filename, FA_READ);
	if (fr != FR_OK) {
		return fr;
	}

	paginated_start(true);

	while (1) {
		fr = f_read(&fil, (void *)buf, sizeof buf, &br);
		if (br == 0)
			break;
		paginated_write(buf, br);
		if (paginated_exit) break;
	}

	f_close(&fil);
	return FR_OK;
}

// Change directory
// Parameters:
// - filename: Path of file to save
// Returns:
// - FatFS return code
//
uint24_t mos_CD(char *path)
{
	FRESULT fr;

	fr = f_chdir(path);
	return fr;
}

// Check if a path is a directory
bool isDirectory(char *path)
{
	FILINFO fil;
	FRESULT fr;

	DEBUG_STACK();

	if (strcmp(path, ".") == 0 || strcmp(path, "..") == 0 || strcmp(path, "/") == 0) {
		return true;
	}

	// check if destination is a directory
	fr = f_stat(path, &fil);

	return (fr == FR_OK) && fil.fname[0] && (fil.fattrib & AM_DIR);
}

static uint24_t get_num_dirents(const char *path, int *cnt)
{
	FRESULT fr;
	DIR dir;
	FILINFO fno;

	DEBUG_STACK();

	*cnt = 0;

	fr = f_opendir(&dir, path);

	if (fr == FR_OK) {
		for (;;) {
			fr = f_readdir(&dir, &fno);
			if (fr != FR_OK || fno.fname[0] == 0) {
				if (*cnt == 0 && fr == FR_DISK_ERR) {
					fr = FR_NO_PATH;
				}
				break; // Break on error or end of dir
			}
			*cnt = *cnt + 1;
		}
	}

	f_closedir(&dir);

	return fr;
}

typedef struct SmallFilInfo {
	FSIZE_t fsize;	 /* File size */
	WORD fdate;	 /* Modified date */
	WORD ftime;	 /* Modified time */
	uint8_t fattrib; /* File attribute */
	char *fname;	 /* umm_malloc'ed */
} SmallFilInfo;

static int cmp_filinfo(const SmallFilInfo *a, const SmallFilInfo *b)
{
	if ((a->fattrib & AM_DIR) == (b->fattrib & AM_DIR)) {
		return strcasecmp(a->fname, b->fname);
	} else if (a->fattrib & AM_DIR) {
		return -1;
	} else {
		return 1;
	}
}

// Directory listing, for MOS API compatibility
// Returns:
// - FatFS return code
//
uint24_t mos_DIR_API(char *inputPath)
{
	return mos_DIR(inputPath, true);
}

uint24_t mos_DIRFallback(const char path[static 1], const char *pattern, bool longListing)
{
	FRESULT fr;
	DIR dir;
	FILINFO fno;
	int yr, mo, da, hr, mi;
	char str[12];

	DEBUG_STACK();

	fr = f_getlabel("", str, 0);
	if (fr != 0) {
		return fr;
	}

	fr = f_opendir(&dir, path);
	if (fr != FR_OK) return fr;

	paginated_start(true);
	paginated_printf("Volume: ");
	if (strlen(str) > 0) {
		paginated_printf("%s", str);
	} else {
		paginated_printf("<No Volume Label>");
	}
	paginated_printf("\n\n");

	if (pattern) {
		fr = f_findfirst(&dir, &fno, path, pattern);
	} else {
		fr = f_readdir(&dir, &fno);
	}
	while (!paginated_exit) {
		if (fr != FR_OK || fno.fname[0] == 0) {
			break;				 // Break on error or end of dir
		}
		if (longListing) {
			yr = (fno.fdate & 0xFE00) >> 9;	 // Bits 15 to  9, from 1980
			mo = (fno.fdate & 0x01E0) >> 5;	 // Bits  8 to  5
			da = (fno.fdate & 0x001F);	 // Bits  4 to  0
			hr = (fno.ftime & 0xF800) >> 11; // Bits 15 to 11
			mi = (fno.ftime & 0x07E0) >> 5;	 // Bits 10 to  5

			paginated_printf("%04d/%02d/%02d %02d:%02d %c %*lu %s\n", yr + 1980, mo, da, hr, mi, fno.fattrib & AM_DIR ? 'D' : ' ', 8, fno.fsize, fno.fname);
		} else {
			paginated_printf("%s%s\n", fno.fname, fno.fattrib & AM_DIR ? "/" : "");
		}
		if (pattern) {
			fr = f_findnext(&dir, &fno);
		} else {
			fr = f_readdir(&dir, &fno);
		}
	}
	f_closedir(&dir);

	return fr;
}

/*
 * Extract directory and possibly glob pattern
 * *out_dirPath and *out_pattern will be umm_malloc'd.
 * Can return MOS_OUT_OF_MEMORY
 */
static MOSRESULT extract_dir_and_pattern(const char inputPath[static 1], char **out_dirPath, char **out_pattern)
{
	const char *last_path_elem = strrchr_pathsep(inputPath);
	last_path_elem = last_path_elem ? last_path_elem + 1 : inputPath;
	if (strchr(last_path_elem, '?') != 0 || strchr(last_path_elem, '*') != 0) {
		*out_pattern = mos_strdup(last_path_elem);
		*out_dirPath = mos_strndup(inputPath, last_path_elem - inputPath);
		if (!*out_pattern || !*out_dirPath) {
			goto handle_oom;
		}
	} else {
		*out_dirPath = mos_strdup(inputPath);
		if (!*out_dirPath) goto handle_oom;
	}
	return (MOSRESULT)FR_OK;
handle_oom:
	if (*out_pattern) umm_free(*out_pattern);
	if (*out_dirPath) umm_free(*out_dirPath);
	return MOS_OUT_OF_MEMORY;
}

// Directory listing
// Returns:
// - FatFS return code
//
uint24_t mos_DIR(const char inputPath[static 1], bool longListing)
{
	FRESULT fr;
	DIR dir;
	char *dirPath = NULL;
	char *pattern = NULL;
	bool useColour = scrcolours > 2 && vdpSupportsTextPalette;
	char str[12]; // Buffer for volume label
	int longestFilename = 0;
	FILINFO filinfo;
	uint8_t textBg;
	uint8_t textFg = 15;
	uint8_t dirColour = get_secondary_color();
	uint8_t fileColour = 15;
	Vec entries;

	DEBUG_STACK();

	fr = f_getlabel("", str, 0);
	if (fr != FR_OK) {
		return fr;
	}

	vec_init(&entries, sizeof(SmallFilInfo));

	fr = (FRESULT)extract_dir_and_pattern(inputPath, &dirPath, &pattern);

	if (fr == MOS_OUT_OF_MEMORY) {
		fr = mos_DIRFallback(inputPath, NULL, longListing);
		goto cleanup;
	}
	// printf("dirPath %s, pattern %s\n", dirPath, pattern ? pattern : "(none)");

	if (useColour) {
		textFg = active_console->get_fg_color_index();
		fileColour = textFg;
		textBg = active_console->get_bg_color_index();
		while (dirColour == textBg || dirColour == fileColour) {
			dirColour = (dirColour + 1) % scrcolours;
		}
	}

	fr = f_opendir(&dir, dirPath);
	if (fr != FR_OK) {
		goto cleanup;
	}

	if (pattern) {
		fr = f_findfirst(&dir, &filinfo, dirPath, pattern);
	} else {
		fr = f_readdir(&dir, &filinfo);
	}

	// Collect the entries into entries vector
	while (fr == FR_OK && filinfo.fname[0]) {
		SmallFilInfo entry;

		entry.fsize = filinfo.fsize;
		entry.fdate = filinfo.fdate;
		entry.ftime = filinfo.ftime;
		entry.fattrib = filinfo.fattrib;
		const int fname_len = strlen(filinfo.fname);
		entry.fname = mos_strndup(filinfo.fname, 256);
		if (!entry.fname) {
			goto oom_fallback;
		}
		if (fname_len > longestFilename) {
			longestFilename = fname_len;
		}

		if (!vec_push(&entries, &entry)) {
			// out-of-memory
			umm_free(entry.fname);
			goto cleanup;
		}

		if (pattern) {
			fr = f_findnext(&dir, &filinfo);
		} else {
			fr = f_readdir(&dir, &filinfo);
		}

		if (!pattern && filinfo.fname[0] == 0)
			break;
	}
	f_closedir(&dir);

	int col = 0;

	// Pad one space. Don't exceed screen length
	longestFilename = MIN(scrcols, longestFilename + 1);
	int maxCols = MAX(1, scrcols / longestFilename);

	if (entries.len > 1) {
		qsort(entries.data, entries.len, sizeof(SmallFilInfo), (int (*)(const void *, const void *)) & cmp_filinfo);
	}

	paginated_start(true);
	paginated_printf("Volume: ");
	if (strlen(str) > 0) {
		paginated_printf("%s", str);
	} else {
		paginated_printf("<No Volume Label>");
	}
	paginated_printf("\n");

	if (strcmp(dirPath, ".") == 0) {
		update_cwd();
		paginated_printf("Directory: %s\n\n", cwd);
	} else
		paginated_printf("Directory: %s\n\n", dirPath);

	vec_foreach(&entries, SmallFilInfo, fno)
	{
		if (paginated_exit) break;
		if (longListing) {
			int yr, mo, da, hr, mi;
			yr = (fno->fdate & 0xFE00) >> 9;  // Bits 15 to  9, from 1980
			mo = (fno->fdate & 0x01E0) >> 5;  // Bits  8 to  5
			da = (fno->fdate & 0x001F);	  // Bits  4 to  0
			hr = (fno->ftime & 0xF800) >> 11; // Bits 15 to 11
			mi = (fno->ftime & 0x07E0) >> 5;  // Bits 10 to  5

			bool isDir = fno->fattrib & AM_DIR;
			if (useColour) set_color(textFg);
			paginated_printf("%04d/%02d/%02d %02d:%02d %c %*lu ", yr + 1980, mo, da, hr, mi, isDir ? 'D' : ' ', 8, fno->fsize);
			if (useColour) set_color(isDir ? dirColour : fileColour);
			paginated_printf("%s\n", fno->fname);
		} else {
			if (col == maxCols) {
				col = 0;
				paginated_printf("\n");
			}

			if (useColour) {
				set_color(fno->fattrib & AM_DIR ? dirColour : fileColour);
			}
			paginated_printf("%-*s", col == (maxCols - 1) ? longestFilename - 1 : longestFilename, fno->fname);
			col++;
		}
	}

	if (!longListing) {
		paginated_printf("\n");
	}

	if (useColour) {
		set_color(textFg);
	}

cleanup:
	vec_foreach(&entries, SmallFilInfo, item)
	{
		if (item->fname) umm_free(item->fname);
	}
	vec_free(&entries);
	if (dirPath) umm_free(dirPath);
	if (pattern) umm_free(pattern);
	return fr;

oom_fallback:
	fr = mos_DIRFallback(dirPath, pattern, longListing);
	goto cleanup;
}

// Delete file
// Parameters:
// - filename: Path of file to delete
// Returns:
// - FatFS return code
//
uint24_t mos_DEL(char *filename)
{
	FRESULT fr;

	fr = f_unlink(filename);
	return fr;
}

// Rename file
// Parameters:
// - srcPath: Source path of file to rename
// - dstPath: Destination file path
// Returns:
// - FatFS return code
//
uint24_t mos_REN_API(char *srcPath, char *dstPath)
{
	return mos_REN(srcPath, dstPath, false);
}

// Rename file
// Parameters:
// - srcPath: Source path of file to rename
// - dstPath: Destination file path
// Returns:
// - FatFS return code
//
uint24_t mos_REN(char *srcPath, char *dstPath, bool verbose)
{
	FRESULT fr;
	DIR dir;
	FILINFO fno;
	char *srcDir = NULL, *pattern = NULL, *fullSrcPath = NULL, *fullDstPath = NULL, *srcFilename = NULL;

	DEBUG_STACK();

	if (strchr(dstPath, '*') != NULL) {
		// printf("Wildcards permitted in source only.\r\n");
		return FR_INVALID_PARAMETER;
	}

	fr = (FRESULT)extract_dir_and_pattern(srcPath, &srcDir, &pattern);
	if (fr != FR_OK) {
		goto cleanup;
	}

	if (pattern) {
		if (!isDirectory(dstPath)) {
			fr = FR_INVALID_PARAMETER;
			goto cleanup;
		}

		fr = f_opendir(&dir, srcDir);
		if (fr != FR_OK) goto cleanup;

		fr = f_findfirst(&dir, &fno, srcDir, pattern);
		while (fr == FR_OK && fno.fname[0] != '\0') {
			size_t srcPathLen = strlen(srcDir) + strlen(fno.fname) + 1;
			size_t dstPathLen = strlen(dstPath) + strlen(fno.fname) + 2; // +2 for '/' and null terminator
			fullSrcPath = umm_malloc(srcPathLen);
			fullDstPath = umm_malloc(dstPathLen);

			if (!fullSrcPath || !fullDstPath) {
				fr = FR_INT_ERR;				     // Out of memory
				if (fullSrcPath) umm_free(fullSrcPath);
				if (fullDstPath) umm_free(fullDstPath);
				break;
			}

			snprintf(fullSrcPath, srcPathLen, "%s%s", srcDir, fno.fname);
			snprintf(fullDstPath, dstPathLen, "%s%s%s", dstPath, (dstPath[strlen(dstPath) - 1] == '/' ? "" : "/"), fno.fname);

			if (verbose) printf("Moving %s to %s\r\n", fullSrcPath, fullDstPath);
			fr = f_rename(fullSrcPath, fullDstPath);
			umm_free(fullSrcPath);
			umm_free(fullDstPath);
			fullSrcPath = NULL;
			fullDstPath = NULL;

			if (fr != FR_OK) break;
			fr = f_findnext(&dir, &fno);
		}

		f_closedir(&dir);

	} else {
		if (isDirectory(dstPath)) {
			// copy into a directory, keeping name
			size_t fullDstPathLen = strlen(dstPath) + strlen(srcPath) + 2; // +2 for potential '/' and null terminator
			fullDstPath = umm_malloc(fullDstPathLen);
			if (!fullDstPath) {
				fr = FR_INT_ERR;
				goto cleanup;
			}
			srcFilename = strrchr(srcPath, '/');
			srcFilename = (srcFilename != NULL) ? srcFilename + 1 : srcPath;
			snprintf(fullDstPath, fullDstPathLen, "%s%s%s", dstPath, (dstPath[strlen(dstPath) - 1] == '/' ? "" : "/"), srcFilename);

			fr = f_rename(srcPath, fullDstPath);
			umm_free(fullDstPath);
		} else {
			fr = f_rename(srcPath, dstPath);
		}
	}

cleanup:
	if (srcDir) umm_free(srcDir);
	if (pattern) umm_free(pattern);
	return fr;
}

// Copy file
// Parameters:
// - srcPath: Source path of file to copy
// - dstPath: Destination file path
// Returns:
// - FatFS return code
//
uint24_t mos_COPY_API(char *srcPath, char *dstPath)
{
	return mos_COPY(srcPath, dstPath, false);
}

static FRESULT copy_file(char *srcPath, char *destPath, bool verbose)
{
	FIL fsrc, fdst;
	FRESULT fr;
	uint8_t buffer[512];
	UINT br, bw;

	DEBUG_STACK();

	fr = f_open(&fsrc, srcPath, FA_READ);
	if (fr != FR_OK) {
		return fr;
	}
	fr = f_open(&fdst, destPath, FA_WRITE | FA_CREATE_NEW);
	if (fr != FR_OK) {
		f_close(&fsrc);
		return fr;
	}

	if (verbose) printf("Copying %s to %s\r\n", srcPath, destPath);
	while (1) {
		fr = f_read(&fsrc, buffer, sizeof(buffer), &br);
		if (br == 0 || fr != FR_OK) break;
		fr = f_write(&fdst, buffer, br, &bw);
		if (bw < br || fr != FR_OK) break;
	}
	f_close(&fsrc);
	f_close(&fdst);

	return 0;
}

// Copy file
// Parameters:
// - srcPath: Source path of file to copy
// - dstPath: Destination file path
// - verbose: Print progress messages
// Returns:
// - FatFS return code
//
uint24_t mos_COPY(char *srcPath, char *dstPath, bool verbose)
{
	FRESULT fr;
	DIR dir;
	FILINFO *fno;
	char *srcDir = NULL, *pattern = NULL, *fullSrcPath = NULL, *fullDstPath = NULL, *srcFilename = NULL;

	DEBUG_STACK();

	if (strchr(dstPath, '*') != NULL) {
		return FR_INVALID_PARAMETER; // Wildcards not allowed in destination path
	}

	fno = umm_malloc(sizeof(FILINFO));
	if (!fno) return MOS_OUT_OF_MEMORY;

	fr = (FRESULT)extract_dir_and_pattern(srcPath, &srcDir, &pattern);
	if (fr != FR_OK) {
		goto cleanup;
	}

	if (pattern) {
		if (!isDirectory(dstPath)) {
			fr = FR_INVALID_PARAMETER;
			goto cleanup;
		}
		fr = f_opendir(&dir, srcDir);
		if (fr != FR_OK) goto cleanup;

		fr = f_findfirst(&dir, fno, srcDir, pattern);
		while (fr == FR_OK && fno->fname[0] != '\0') {
			size_t srcPathLen = strlen(srcDir) + strlen(fno->fname) + 1;
			size_t dstPathLen = strlen(dstPath) + strlen(fno->fname) + 2; // +2 for '/' and null terminator
			fullSrcPath = umm_malloc(srcPathLen);
			fullDstPath = umm_malloc(dstPathLen);

			if (fullSrcPath && fullDstPath) {
				snprintf(fullSrcPath, srcPathLen, "%s%s", srcDir, fno->fname);
				snprintf(fullDstPath, dstPathLen, "%s%s%s", dstPath, (dstPath[strlen(dstPath) - 1] == '/' ? "" : "/"), fno->fname);
				copy_file(fullSrcPath, fullDstPath, verbose);
			} else {
				fr = (FRESULT)MOS_OUT_OF_MEMORY;
			}

			if (fullSrcPath) umm_free(fullSrcPath);
			if (fullDstPath) umm_free(fullDstPath);
			fullSrcPath = NULL;
			fullDstPath = NULL;

			if (fr != FR_OK) break;
			fr = f_findnext(&dir, fno);
		}

		f_closedir(&dir);
	} else {
		size_t fullDstPathLen = strlen(dstPath) + strlen(srcPath) + 2; // +2 for potential '/' and null terminator
		fullDstPath = umm_malloc(fullDstPathLen);
		if (!fullDstPath) {
			fr = FR_INT_ERR;
			goto cleanup;
		}
		srcFilename = strrchr(srcPath, '/');
		srcFilename = (srcFilename != NULL) ? srcFilename + 1 : srcPath;
		if (isDirectory(dstPath)) {
			snprintf(fullDstPath, fullDstPathLen, "%s%s%s", dstPath, (dstPath[strlen(dstPath) - 1] == '/' ? "" : "/"), srcFilename);
		} else {
			fullDstPath[0] = 0;
			strbuf_append(fullDstPath, fullDstPathLen, dstPath, fullDstPathLen);
		}

		fr = copy_file(srcPath, fullDstPath, verbose);
	}

cleanup:
	umm_free(fno);
	if (srcDir) umm_free(srcDir);
	if (pattern) umm_free(pattern);
	if (fullSrcPath) umm_free(fullSrcPath);
	if (fullDstPath) umm_free(fullDstPath);
	return fr;
}

// Make a directory
// Parameters:
// - filename: Path of file to delete
// Returns:
// - FatFS return code
//
uint24_t mos_MKDIR(char *filename)
{
	FRESULT fr;

	fr = f_mkdir(filename);
	return fr;
}

// Load and run a batch file of MOS commands.
// Parameters:
// - filename: The batch file to execute
// - buffer: Storage for each line to be loaded into and executed from (recommend 256 bytes)
// - size: Size of buffer (in bytes)
// Returns:
// - FatFS return code (of the last command)
//
uint24_t mos_EXEC(char *filename, char *buffer, uint24_t size)
{
	FRESULT fr;
	FIL fil;
	int line = 0;

	fr = f_open(&fil, filename, FA_READ);
	if (fr == FR_OK) {
		while (!f_eof(&fil)) {
			line++;
			f_gets(buffer, size, &fil);
			fr = mos_exec(buffer, true);
			if (fr != FR_OK) {
				printf("\r\nError executing %s at line %d\r\n", filename, line);
				break;
			}
		}
	}
	f_close(&fil);
	return fr;
}

// Open a file
// Parameters:
// - filename: Path of file to open
// - mode: File open mode (r, r/w, w, etc) - see FatFS documentation for more details
// Returns:
// - File handle, or 0 if the file cannot be opened
//
uint24_t mos_FOPEN(char *filename, uint8_t mode)
{
	FRESULT fr;
	int i;

	for (i = 0; i < MOS_maxOpenFiles; i++) {
		if (mosFileObjects[i] == NULL) {
			FIL *f = umm_malloc(sizeof(FIL));
			if (!f) return MOS_OUT_OF_MEMORY;

			fr = f_open(f, filename, mode);
			if (fr == FR_OK) {
				mosFileObjects[i] = f;
				return i + 1;
			} else {
				umm_free(f);
				return 0;
			}
		}
	}
	return 0;
}

// Close file(s)
// Parameters:
// - fh: File handle, or 0 to close all open files
// Returns:
// - File handle passed in function args
//
uint24_t mos_FCLOSE(uint8_t fh)
{
	FRESULT fr;
	int i;

	if (fh > 0 && fh <= MOS_maxOpenFiles) {
		i = fh - 1;
		if (mosFileObjects[i]) {
			fr = f_close(mosFileObjects[i]);
			umm_free(mosFileObjects[i]);
			mosFileObjects[i] = NULL;
		}
	} else {
		for (i = 0; i < MOS_maxOpenFiles; i++) {
			if (mosFileObjects[i]) {
				fr = f_close(mosFileObjects[i]);
				umm_free(mosFileObjects[i]);
				mosFileObjects[i] = NULL;
			}
		}
	}
	return fh;
}

// Read a byte from a file
// Parameters:
// - fh: File handle
// Returns:
// - Byte read in lower 8 bits
// - EOF in upper 8 bits (1 = EOF)
//
uint24_t mos_FGETC(uint8_t fh)
{
	FRESULT fr;
	FIL *fo;
	UINT br;
	char c;

	fo = (FIL *)mos_GETFIL(fh);
	if (fo > 0) {
		fr = f_read(fo, &c, 1, &br);
		if (fr == FR_OK) {
			return c | (fat_EOF(fo) << 8);
		}
	}
	return 0;
}

// Write a byte to a file
// Parameters:
// - fh: File handle
// - c: Byte to write
//
void mos_FPUTC(uint8_t fh, char c)
{
	FIL *fo = (FIL *)mos_GETFIL(fh);

	if (fo > 0) {
		f_putc(c, fo);
	}
}

// Read a block of data into a buffer
// Parameters:
// - fh: File handle
// - buffer: Address to write the data into
// - btr: Number of bytes to read
// Returns:
// - Number of bytes read
//
uint24_t mos_FREAD(uint8_t fh, uint24_t buffer, uint24_t btr)
{
	FRESULT fr;
	FIL *fo = (FIL *)mos_GETFIL(fh);
	UINT br = 0;

	if (fo > 0) {
		fr = f_read(fo, (void *)buffer, btr, &br);
		if (fr == FR_OK) {
			return br;
		}
	}
	return 0;
}

// Write a block of data from a buffer
// Parameters:
// - fh: File handle
// - buffer: Address to read the data from
// - btw: Number of bytes to write
// Returns:
// - Number of bytes written
//
uint24_t mos_FWRITE(uint8_t fh, uint24_t buffer, uint24_t btw)
{
	FRESULT fr;
	FIL *fo = (FIL *)mos_GETFIL(fh);
	UINT bw = 0;

	if (fo > 0) {
		fr = f_write(fo, (const void *)buffer, btw, &bw);
		if (fr == FR_OK) {
			return bw;
		}
	}
	return 0;
}

// Move the read/write pointer in a file
// Parameters:
// - offset: Position of the pointer relative to the start of the file
// Returns:
// - FRESULT
//
uint8_t mos_FLSEEK(uint8_t fh, uint32_t offset)
{
	FIL *fo = (FIL *)mos_GETFIL(fh);

	if (fo > 0) {
		return f_lseek(fo, offset);
	}
	return FR_INVALID_OBJECT;
}

// Alternative FLSEEK function that uses a pointer to the 32-bit offset value
// Parameters:
// - fh: File handle
// - offset: Pointer to the position of the pointer relative to the start of the file
// Returns:
// - FRESULT
//
uint8_t mos_FLSEEKP(uint8_t fh, uint32_t *offset)
{
	return mos_FLSEEK(fh, *offset);
}

// Check whether file is at EOF (end of file)
// Parameters:
// - fh: File handle
// Returns:
// - 1 if EOF, otherwise 0
//
uint8_t mos_FEOF(uint8_t fh)
{
	FIL *fo = (FIL *)mos_GETFIL(fh);

	if (fo > 0) {
		return fat_EOF(fo);
	}
	return 0;
}

// Copy an error string to RAM
// Parameters:
// - errno: The error number
// - address: Address of the buffer to copy the error code to
// - size: Size of buffer
//
void mos_GETERROR(uint8_t errno, uint24_t address, uint24_t size)
{
	strncpy((char *)address, mos_errors[errno], size - 1);
}

// OSCLI
// Parameters
// - cmd: Address of the command entered
// Returns:
// - MOS error code
//
uint24_t mos_OSCLI(char *cmd)
{
	uint24_t fr;
	fr = mos_exec(cmd, false);
	return fr;
}

// Get the RTC
// Parameters:
// - address: Pointer to buffer to store time in
// Returns:
// - size of string
//
uint8_t mos_GETRTC(char buffer[static 64])
{
	vdp_time_t t;

	rtc_update();
	rtc_unpack(&rtc, &t);
	rtc_formatDateTime(buffer, &t);

	return strlen(buffer);
}

void mos_UNPACKRTC(uint24_t address, uint8_t flags)
{
	if (flags & 1) {
		rtc_update();
	}
	if (address != 0) {
		rtc_unpack(&rtc, (vdp_time_t *)address);
	}
	if (flags & 2) {
		rtc_update();
	}
}

// Set the RTC
// Parameters:
// - address: Pointer to buffer that contains the time data
// Returns:
// - size of string
//
void mos_SETRTC(uint24_t address)
{
	uint8_t *p = (uint8_t *)address;

	putch(23); // Set the ESP32 time
	putch(0);
	putch(VDP_rtc);
	putch(1);  // 1: Set time (6 byte buffer mode)
	//
	putch(*p++); // Year
	putch(*p++); // Month
	putch(*p++); // Day
	putch(*p++); // Hour
	putch(*p++); // Minute
	putch(*p);   // Second
}

// Set an interrupt vector
// Parameters:
// - vector: The interrupt vector to set
// - address: Address of the interrupt handler
// Returns:
// - address: Address of the previous interrupt handler
//
uint24_t mos_SETINTVECTOR(uint8_t vector, uint24_t address)
{
	void (*handler)(void) = (void *)address;
	return (uint24_t)set_vector(vector, handler);
}

// Get a FIL struct from a filehandle
// Parameters:
// - fh: The filehandle (indexed from 1)
// Returns:
// - address of the file structure, or 0 if invalid fh
//
uint24_t mos_GETFIL(uint8_t fh)
{
	if (fh > 0 && fh <= MOS_maxOpenFiles) {
		return (uint24_t)mosFileObjects[fh - 1];
	}
	return 0;
}

// Check whether file is at EOF (end of file)
// Parameters:
// - fp: Pointer to file structure
// Returns:
// - 1 if EOF, otherwise 0
//
uint8_t fat_EOF(FIL *fp)
{
	if (f_eof(fp) != 0) {
		return 1;
	}
	return 0;
}

// (Re-)mount the MicroSD card
// Parameters:
// - None
// Returns:
// - fatfs error code
//
int mos_mount(void)
{
	int ret = f_mount(&fs, "", 1); // Mount the SD card
	if (ret == FR_OK) {
		update_cwd();
	}
	return ret;
}

extern void hxload_vdp(void);

int mos_cmdSIDELOAD(char *p)
{
	printf("Waiting for VDP data...\r\n");
	hxload_vdp();
	printf("Done\r\n");
	return 0;
}

int mos_cmdFBMODE(char *p)
{
	char *value_str;
	if (fb_driverversion() == 0) {
		printf("EZ80 GPIO video driver not found\r\n");
		return 0;
	}
	// Get mode as argument
	if (!mos_parseString(NULL, &value_str)) {
		printf("Current mode: %d\r\n", (int8_t)fb_mode);
		printf("Available modes:\r\n");

		for (int mode = 0;; mode++) {
			struct fbmodeinfo_t *minfo = fb_lookupmode(mode);
			if (minfo == NULL) break;
			printf("Mode %d: %dx%d", mode, minfo->width, minfo->height);
			if (minfo->flags & FBMODE_FLAG_15KHZ) printf(" 15KHz");
			if (minfo->flags & FBMODE_FLAG_31KHZ) printf(" VGA");
			if (minfo->flags & FBMODE_FLAG_50HZ) printf(" 50Hz");
			if (minfo->flags & FBMODE_FLAG_60HZ) printf(" 60Hz");
			if (minfo->flags & FBMODE_FLAG_SLOW) printf(" (SLOW)");
			printf("\r\n");
		}
		printf("Mode -1: Disable GPIO video\n");

		return 0;
	}

	int mode = strtol(value_str, NULL, 10);
	int ret = mos_FBMODE(mode);

	if (ret == MOS_INVALID_PARAMETER) {
		printf("Invalid mode\r\n");
		return 0;
	} else if (ret == MOS_NOT_IMPLEMENTED) {
		printf("EZ80 GPIO video driver not found\r\n");
		return 0;
	} else {
		return ret;
	}
}

static void *fb_scanline_offsets = NULL;

uint24_t mos_FBMODE(int req_mode)
{
	if (fb_driverversion() == 0) {
		return MOS_NOT_IMPLEMENTED;
	}

	if (fb_scanline_offsets) {
		umm_free(fb_scanline_offsets);
		fb_scanline_offsets = NULL;
	}

	if (req_mode == -1) {
		stop_fbterm();
		console_enable_vdp();
		return 0;
	}

	int set_mode = req_mode & 0x100 ? fb_mode : req_mode;

	struct fbmodeinfo_t *minfo = fb_lookupmode(set_mode);

	if (minfo == 0) {
		return MOS_INVALID_PARAMETER;
	}

	void *fb_base = (void *)((int)__MOS_systemAddress - SPL_STACK_SIZE - minfo->width * minfo->height);

	fb_scanline_offsets = umm_malloc(sizeof(void *) * minfo->height * minfo->scan_multiplier);

	return start_fbterm(set_mode, fb_base, fb_scanline_offsets);
}
