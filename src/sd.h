/*
 * Title:			AGON MOS - Low level SD card functionality
 * Author:			RJH
 * Modified By:		Dean Belfield
 * Created:			19/06/2022
 * Last Updated:	08/11/2023
 *
 * Modinfo:
 * 08/11/2023:		Removed redundant defines and function prototypes
 */

#ifndef SD_H
#define SD_H

#include "defines.h"

#define SD_SUCCESS 0
#define SD_ERROR 1
#define SD_LOCKED 2
#define SD_READY 0

uint8_t SD_readBlocks(uint32_t addr, uint8_t *buf, uint16_t count);
uint8_t SD_writeBlocks(uint32_t addr, const uint8_t *buf, uint16_t count);

uint8_t SD_init();

#endif /* SD_H */
