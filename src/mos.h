/*
 * Title:			AGON MOS - MOS code
 * Author:			Dean Belfield
 * Created:			10/07/2022
 */

#ifndef MOS_H
#define MOS_H

#include "../src_fatfs/ff.h"
#include "defines.h"

extern char cmd[256]; // Array for the command line handler

typedef struct {
	char *name;
	int (*func)(char *ptr);
	char *args;
	char *help;
} t_mosCommand;

/**
 * MOS-specific return codes
 * These extend the FatFS return codes FRESULT
 */
typedef enum {
	MOS_INVALID_COMMAND = 20, /* (20) Command could not be understood */
	MOS_INVALID_EXECUTABLE,	  /* (21) Executable file format not recognised */
	MOS_OUT_OF_MEMORY,	  /* (22) Generic out of memory error NB this is currently unused */
	MOS_NOT_IMPLEMENTED,	  /* (23) API call not implemented */
	MOS_OVERLAPPING_SYSTEM,	  /* (24) File load prevented to stop overlapping system memory */
	MOS_BAD_STRING,		  /* (25) Bad or incomplete string */
	MOS_INVALID_PARAMETER,	  /* (26) Invalid parameter */
} MOSRESULT;

void mos_error(MOSRESULT error);
uint8_t mos_getkey(void);
uint24_t mos_input(char *buffer, int bufferLength);
void mos_print_prompt(void);
const t_mosCommand *mos_getCommand(char *ptr);
char *mos_trim(char *s);
char *mos_strtok(char *s1, char *s2);
char *mos_strtok_r(char *s1, const char *s2, char **ptr);
int mos_exec(char *buffer, bool in_mos);
uint8_t mos_execMode(uint8_t *ptr);

int mos_mount(void);

bool mos_parseNumber(char *ptr, uint24_t *p_Value);
bool mos_parseString(char *ptr, char **p_Value);

int mos_cmdDIR(char *ptr);
int mos_cmdDISC(char *ptr);
int mos_cmdLOAD(char *ptr);
int mos_cmdSAVE(char *ptr);
int mos_cmdSIDELOAD(char *ptr);
int mos_cmdDEL(char *ptr);
int mos_cmdJMP(char *ptr);
int mos_cmdRUN(char *ptr);
int mos_cmdCD(char *ptr);
int mos_cmdREN(char *ptr);
int mos_cmdCOPY(char *ptr);
int mos_cmdMKDIR(char *ptr);
int mos_cmdSET(char *ptr);
int mos_cmdVDU(char *ptr);
int mos_cmdTIME(char *ptr);
int mos_cmdCREDITS(char *ptr);
int mos_cmdEXEC(char *ptr);
int mos_cmdTYPE(char *ptr);
int mos_cmdCLS(char *ptr);
int mos_cmdMOUNT(char *ptr);
int mos_cmdHELP(char *ptr);
int mos_cmdHOTKEY(char *ptr);
int mos_cmdMEM(char *ptr);
int mos_cmdPRINTF(char *ptr);
int mos_cmdECHO(char *ptr);
int mos_cmdFBMODE(char *ptr);
int mos_cmdMEMDUMP(char *ptr);

uint24_t mos_LOAD(char *filename, uint24_t address, uint24_t size);
uint24_t mos_SAVE(char *filename, uint24_t address, uint24_t size);
uint24_t mos_TYPE(char *filename);
uint24_t mos_CD(char *path);
uint24_t mos_DIR_API(char *path);
uint24_t mos_DIR(const char path[static 1], bool longListing);
uint24_t mos_DEL(char *filename);
uint24_t mos_REN_API(char *srcPath, char *dstPath);
uint24_t mos_REN(char *srcPath, char *dstPath, bool verbose);
uint24_t mos_COPY_API(char *srcPath, char *dstPath);
uint24_t mos_COPY(char *srcPath, char *dstPath, bool verbose);
uint24_t mos_MKDIR(char *filename);
uint24_t mos_EXEC(char *filename, char *buffer, uint24_t size);
uint24_t mos_FBMODE(int req_mode);

uint24_t mos_FOPEN(char *filename, uint8_t mode);
uint24_t mos_FCLOSE(uint8_t fh);
uint24_t mos_FGETC(uint8_t fh);
void mos_FPUTC(uint8_t fh, char c);
uint24_t mos_FREAD(uint8_t fh, uint24_t buffer, uint24_t btr);
uint24_t mos_FWRITE(uint8_t fh, uint24_t buffer, uint24_t btw);
uint8_t mos_FLSEEK(uint8_t fh, uint32_t offset);
uint8_t mos_FEOF(uint8_t fh);

void mos_GETERROR(uint8_t errno, uint24_t address, uint24_t size);
uint24_t mos_OSCLI(char *cmd);
uint8_t mos_GETRTC(char buffer[static 64]);
void mos_SETRTC(uint24_t address);
uint24_t mos_SETINTVECTOR(uint8_t vector, uint24_t address);
uint24_t mos_GETFIL(uint8_t fh);

extern TCHAR *cwd;
extern bool sdcardDelay;

uint8_t fat_EOF(FIL *fp);

#define HELP_CAT "Directory listing of the current directory\r\n"
#define HELP_CAT_ARGS "[-l] <path>"

#define HELP_CD "Change current directory\r\n"
#define HELP_CD_ARGS "<path>"

#define HELP_COPY "Create a copy of a file\r\n"
#define HELP_COPY_ARGS "<filename1> <filename2>"

#define HELP_CREDITS "Output credits and version numbers for\r\n" \
		     "third-party libraries used in the Agon firmware\r\n"

#define HELP_DELETE "Delete a file or folder (must be empty)\r\n"
#define HELP_DELETE_ARGS "[-f] <filename>"

#define HELP_EXEC "Run a batch file containing MOS commands\r\n"
#define HELP_EXEC_ARGS "<filename>"

#define HELP_JMP "Jump to the specified address in memory\r\n"
#define HELP_JMP_ARGS "<addr>"

#define HELP_LOAD "Load a file from the SD card to the specified address.\r\n" \
		  "If no `addr` parameter is passed it will"                   \
		  "default to &40000\r\n"
#define HELP_LOAD_ARGS "<filename> [<addr>]"

#define HELP_MEM "Output memory statistics\r\n"

#define HELP_MEMDUMP "Show contents of memory\r\n"
#define HELP_MEMDUMP_ARGS "<addr> <len>"

#define HELP_MKDIR "Create a new folder on the SD card\r\n"
#define HELP_MKDIR_ARGS "<filename>"

#define HELP_PRINTF "Print a string to the VDU, with common unix-style escapes\r\n"
#define HELP_PRINTF_ARGS "<string>"

#define HELP_ECHO "Like PRINTF, but terminates with a newline (\\r\\n)\r\n"
#define HELP_ECHO_ARGS "<string>"

#define HELP_RENAME "Rename a file in the same folder\r\n"
#define HELP_RENAME_ARGS "<filename1> <filename2>"

#define HELP_RUN "Call an executable binary loaded in memory.\r\n" \
		 "If no parameters are passed, then addr will "    \
		 "default to &40000.\r\n"
#define HELP_RUN_ARGS "[<addr>]"

#define HELP_SAVE "Save a block of memory to the SD card\r\n"
#define HELP_SAVE_ARGS "<filename> <addr> <size>"

#define HELP_SET "Set a system option\r\n\r\n"                 \
		 "Keyboard Layout\r\n"                         \
		 "SET KEYBOARD n: Set the keyboard layout\r\n" \
		 "    0: UK (default)\r\n"                     \
		 "    1: US\r\n"                               \
		 "    2: German\r\n"                           \
		 "    3: Italian\r\n"                          \
		 "    4: Spanish\r\n"                          \
		 "    5: French\r\n"                           \
		 "    6: Belgian\r\n"                          \
		 "    7: Norwegian\r\n"                        \
		 "    8: Japanese\r\n"                         \
		 "    9: US International\r\n"                 \
		 "   10: US International (alternative)\r\n"   \
		 "   11: Swiss (German)\r\n"                   \
		 "   12: Swiss (French)\r\n"                   \
		 "   13: Danish\r\n"                           \
		 "   14: Swedish\r\n"                          \
		 "   15: Portuguese\r\n"                       \
		 "   16: Brazilian Portugese\r\n"              \
		 "   17: Dvorak\r\n"                           \
		 "\r\n"                                        \
		 "Serial Console\r\n"                          \
		 "SET CONSOLE n: Serial console\r\n"           \
		 "    0: Console off (default)\r\n"            \
		 "    1: Console on\r\n"
#define HELP_SET_ARGS "<option> <value>"

#define HELP_TIME "Set and read the ESP32 real-time clock\r\n"
#define HELP_TIME_ARGS "[ <yyyy> <mm> <dd> <hh> <mm> <ss> ]"

#define HELP_VDU "Write a stream of characters to the VDP\r\n" \
		 "Character values are converted to bytes before sending\r\n"
#define HELP_VDU_ARGS "<char1> <char2> ... <charN>"

#define HELP_TYPE "Display the contents of a file on the screen\r\n"
#define HELP_TYPE_ARGS "<filename>"

#define HELP_FBMODE "Set EZ80 GPIO Video mode"
#define HELP_FBMODE_ARGS "<mode_number>"

#define HELP_HOTKEY "Store a command in one of 12 hotkey slots assigned to F1-F12\r\n\r\n" \
		    "Optionally, the command string can include \"%s\" as a marker\r\n"    \
		    "in which case the hotkey command will be built either side.\r\n\r\n"  \
		    "HOTKEY without any arguments will list the currently assigned\r\n"    \
		    "command strings.\r\n"

#define HELP_HOTKEY_ARGS "<key number> <command string>"

#define HELP_CLS "Clear the screen\r\n"

#define HELP_MOUNT "(Re-)mount the MicroSD card\r\n"

#define HELP_HELP "Display help on a single or all commands.\r\n"

#define HELP_HELP_ARGS "[ <command> | all ]"

#endif /* MOS_H */
