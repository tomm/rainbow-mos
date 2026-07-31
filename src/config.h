/*
 * Title:			AGON MOS - MOS config
 * Author:			Dean Belfield
 * Created:			19/09/2022
 * Last Updated:	13/11/2022
 *
 * Modinfo:
 * 13/11/2022:		Added MOS_starLoadAddress
 */

#ifndef CONFIG_H
#define CONFIG_H

#define MOS_prompt '*'			// MOS prompt character
#define MOS_maxOpenFiles 8		// Maximum number of files that mos_FOPEN can open at the same time
#define MOS_defaultLoadAddress 0x040000 // Default load address for LOAD and RUN commands
#define MOS_starLoadAddress 0xB0000	// Address for loading on-SD star commands
#define MOS_externLastRAMaddress 0xBFFFF

#define FEAT_FRAMEBUFFER

#endif					/* CONFIG_H */
