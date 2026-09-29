#include "defines.h"
#include "sd.h"

extern uint8_t quickrand(void);

typedef struct {
	uint32_t address;
	uint24_t code;
} SD_safe_access;

//safety to throw an error if the length of code defined above is anything other than 3 bytes
typedef char assert_sd_safe_access_size[(sizeof(SD_safe_access) == 7) ? 1 : -1];

static uint24_t unlockCode = 0;

void SD_getUnlockCode(uint24_t *code)
{
	if (code == NULL) {
		return;
	}
	while (unlockCode == 0) {
		// Generate an unlock code
		uint8_t *codePtr = (uint8_t *)&unlockCode;
		codePtr[0] = ((uint8_t)quickrand()) ^ 0xDE;
		codePtr[1] = ((uint8_t)quickrand()) ^ 0xAD;
		codePtr[2] = ((uint8_t)quickrand()) ^ 0x5D;
		// unlockCode = rand() ^ 0xDEAD5D;
	}
	*code = unlockCode;
}

uint8_t SD_init_API(uint24_t *code)
{
	if ((code == NULL) || (*code != unlockCode)) {
		return SD_LOCKED;
	}
	return SD_init();
}

uint8_t SD_readBlocks_API(SD_safe_access *addr_w_code, uint8_t *buf, uint16_t count)
{
	// Check that the code provided matches unlockCode
	if (addr_w_code == NULL) {
		return SD_ERROR;
	}
	if ((unlockCode == 0) || (addr_w_code->code != unlockCode)) {
		return SD_LOCKED;
	}
	// Read the blocks from the SD card
	return SD_readBlocks(addr_w_code->address, buf, count);
}

uint8_t SD_writeBlocks_API(SD_safe_access *addr_w_code, uint8_t *buf, uint16_t count)
{
	// Check that the code provided matches unlockCode to prevent accidental SDcard writes
	if (addr_w_code == NULL) {
		return SD_ERROR;
	}
	if ((unlockCode == 0) || (addr_w_code->code != unlockCode)) {
		return SD_LOCKED;
	}
	return SD_writeBlocks(addr_w_code->address, buf, count);
}
