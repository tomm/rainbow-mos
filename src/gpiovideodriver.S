#include "config.h"
#ifdef FEAT_FRAMEBUFFER
		.assume adl=1
		.altmacro	; enable local keyword in macros
		.global _gpiovideoapi_init

mos_api_pollkeyboardevent: .equ 0x62
		.bss
; Public video_setup struct
video_setup:
	; incremented at end of image data scanout
	frame_counter:	.ds 1
	fb_ptr:		.ds 3 ; &[width*height]
	fb_scanline_offsets: .ds 3; &[480*3 bytes for VGA modes]
	pre_image_callback: .ds 3	; fn ptr. called one line before image scanout begins. last chance to swap buffers etc
_current_mode:	.ds 3		; ptr into _modes
_timer1_int_vector:
		.ds 3
_section_line_number:
		.ds 1
uart0_buf_pos:	.ds 3		; ptr into uart0_rx_buf
uart0_rx_buf:	.ds 32

		.text
_gpiovideoapi_init:
		ld a,0x61 ; mos_api_setresetvector (rainbow mos 2.5+)
		ld e,0x20 ; set 0x20 reset vector
		ld hl,videoapi_entry
		rst.lil 8
		ret

		.balign 0x100
videoapi_entry:
		push hl
		; a *= 3
		ld l,a
		add a,a
		add a,l
		ld hl,jumptable
		add a,l
		ld l,a		; only works due to .align 0x100 and jumptable being located close to tsr_begin
		ld hl,(hl)
		jp (hl)
jumptable:
		.d24 api_getversion		; a=0
		.d24 api_setmode			; a=1
		.d24 api_getvideosetup		; a=2
		.d24 api_getmodeinfo		; a=3
		.d24 api_videostop		; a=4
		.d24 api_fillscanlineoffsetarray ; a=5
		.d24 api_lookupmode		; a=6
		.d24 api_getaudioctrl		; a=7
		.d24 api_pollkeyboardevent	; a=8

; Input: a=0
; Return version in `a` (will be non-zero)
api_getversion:
		pop hl
		ld a,3
		ret.lil

; Input: a=1, mode number in `l`.
api_setmode:
		call video_init
		pop hl
		ld a,l		; video mode number
		call video_set_mode
		ret.lil

; Input: a=2
; Return pointer to `video_setup` structure in IY (read/write)
; struct {
;   uint8_t frame_counter; /* incremented after each frame */
;   uint24_t fb_ptr; /* pointer to framebuffer data */
;   uint24_t fb_scanline_offsets; /* pointer to array of 480 uint24_t, each
;                                    an offset applied to fb_ptr to find the
;                                    pixel data of a given scanline */
;   uint24_t pre_image_callback; /* fn pointer, called shortly before image scanout */
; }
api_getvideosetup:
		pop hl
		ld iy,video_setup
		ret.lil

; Input: a=3
; Return pointer to info of current mode in IY (read only)
; struct {
;    uint24_t scanline_isr;
;    uint24_t clockcycles_per_scanline_div_4;
;    uint24_t width;
;    uint24_t height;
;    uint24_t scan_multiplier;
;    uint8_t flags;
; }
api_getmodeinfo:
		pop hl
		ld iy,(_current_mode)
		ret.lil

; Input: a=4
; End GPIO video scanout
api_videostop:
		pop hl
		call video_stop
		ret.lil

; Input: a=5
;   ix: fb_scanline_offsets array address
;   de: line stride (ie virtual framebuffer width in pixels)
;   bc: number of lines
;   iy: scan multiplier (ie 2=doublescan)
; Output:
;   array at [ix+0]..[ix+bc*iyl*3] will be filled with 24-bit scanline offsets
;
; See example-virtualfb.asm
api_fillscanlineoffsetarray:
		pop hl
		call fill_scanline_offset_array
		ret.lil

; Input: a=6
;	hl = mode number
; Output:
;	hl = mode info struct ptr (same structure as api_getmodeinfo returns)
api_lookupmode:
		pop hl
		ld a,l
		call lookup_mode
		ret.lil

; Input: a=7
; Output:
;	hl = audio ring buffer address (256 byte size, 256 byte alignment)
;	a = audio ring buffer play position
;
; Samples written to the audio ring buffer are packets
; of 8 samples per byte, played lowest-order bit first.
;
; Do not write to the 'play position' (output register `a`):
; this would cause audio corruption. Buffer is full when you
; have filled up to one byte before the 'play position'.
;
; Audio playback is destructive, clearing the buffer as it goes.
api_getaudioctrl:
		pop hl
		ld hl,audio_buf
		ld a,(audio_readpos)
		ret.lil

; Poll for next event in keyboard buffer.
; (Same as Rainbow MOS mos_api_pollkeyboardevent)
;
; Input: a=8
;        de = Address of 4-byte buffer to write keyboard event to
; Return:
;   A=0 IF no event
;   A=1 IF event
;   (DE+0) - Event ASCII value
;   (DE+1) - Event keymods (alt, ctrl, etc)
;   (DE+2) - Event FabGL vkey
;   (DE+3) - 0=Key Up, 1=Key Down
api_pollkeyboardevent:
		pop hl
		; Use rainbow MOS api
		ld a,mos_api_pollkeyboardevent
		rst.lil 8
		ret.lil

;
; Video driver proper
;
UART0_REG_RBR:		equ	0xc0	; receive buffer
UART0_REG_IER:		equ	0xc1	; interrupt enable register
UART0_REG_LSR:		equ	0xc5	; line status
UART_LSR_RDY:		equ	0x01	; data ready

PB_DR: .equ 0x9a
PB_DDR: .equ 0x9b
PB_ALT1: .equ 0x9c
PB_ALT2: .equ 0x9d

PC_DR: .equ 0x9e
PC_DDR: .equ 0x9f
PC_ALT1: .equ 0xa0
PC_ALT2: .equ 0xa1

PD_DR: .equ 0xa2
PD_DDR: .equ 0xa3
PD_ALT1: .equ 0xa4
PD_ALT2: .equ 0xa5

TMR1_CTL: .equ 0x83
TMR1_DR_L: .equ 0x84
TMR1_DR_H: .equ 0x85

PRT_INT_ENABLE: .equ 0b01000000
PRT_CLK_DIV_4: .equ 0b0000
PRT_CLK_DIV_16: .equ 0b0100
PRT_CLK_DIV_64: .equ 0b1000
PRT_CLK_DIV_256: .equ 0b1100
PRT_MODE_CONTINUOUS: .equ 0b10000
PRT_ENABLE: .equ 0b1
PRT_START: .equ 0b10

	macro REP_NOP num
		;.fillbyte 0
		.ds \num
	endm

	; for minimum n of 2
	macro REP_NOP_10x n
		push bc		; 4
		ld b,[\n-1]	; 2
	1:	nop
		nop
		nop
		nop
		nop
		nop
		djnz 1b	; 4*n
		nop		; part of djnz
		nop		;   "     "
		pop bc		; 4
	endm

MODE_FLAG_SLOW: .equ 1
MODE_FLAG_15KHZ: .equ 2
MODE_FLAG_31KHZ: .equ 4
MODE_FLAG_50HZ: .equ 8
MODE_FLAG_60HZ: .equ 16

modestruct_len: .equ 16
_modes:
		; struct Mode {
		; 	void (*initial_scanline_handler_fn)();
		;	uint24_t scanline_duration;  /* In CPU clocks divided by 4 */
		;	uint24_t width;
		;	uint24_t height;
		;	uint24_t scan_multiplier;    /* eg: 2 for doublescan */
		; }
		; Fast modes scanning every second line (~33% CPU free)
		.d24 scan_vga_31khz_480p_60hz_grille, 147, 156, 120, 2
		.db MODE_FLAG_31KHZ | MODE_FLAG_60HZ

		.d24 scan_vga_31khz_480p_60hz_grille, 147, 156, 240, 1
		.db MODE_FLAG_31KHZ | MODE_FLAG_60HZ

		.d24 scan_vga_31khz_480p_60hz, 147, 156, 120, 4
		.db MODE_FLAG_SLOW | MODE_FLAG_31KHZ | MODE_FLAG_60HZ

		.d24 scan_vga_31khz_480p_60hz, 147, 156, 160, 3
		.db MODE_FLAG_SLOW | MODE_FLAG_31KHZ | MODE_FLAG_60HZ

		.d24 scan_vga_31khz_480p_60hz, 147, 156, 240, 2
		.db MODE_FLAG_SLOW | MODE_FLAG_31KHZ | MODE_FLAG_60HZ

		.d24 scan_vga_31khz_480p_60hz, 147, 156, 480, 1
		.db MODE_FLAG_SLOW | MODE_FLAG_31KHZ | MODE_FLAG_60HZ

		.d24 scan_rgb_15khz_240p_60hz_grille, 294, 320, 120, 1
		.db MODE_FLAG_15KHZ | MODE_FLAG_60HZ

		.d24 scan_rgb_15khz_240p_60hz, 294, 320, 120, 2
		.db MODE_FLAG_15KHZ | MODE_FLAG_SLOW | MODE_FLAG_60HZ

		.d24 scan_rgb_15khz_240p_60hz, 294, 320, 240, 1
		.db MODE_FLAG_15KHZ | MODE_FLAG_SLOW | MODE_FLAG_60HZ

		.d24 scan_rgb_15khz_288p_60hz_grille, 294, 320, 144, 1
		.db MODE_FLAG_15KHZ | MODE_FLAG_50HZ

		.d24 scan_rgb_15khz_288p_50hz, 294, 320, 144, 2
		.db MODE_FLAG_15KHZ | MODE_FLAG_SLOW | MODE_FLAG_50HZ

		.d24 scan_rgb_15khz_288p_50hz, 294, 320, 288, 1
		.db MODE_FLAG_15KHZ | MODE_FLAG_SLOW | MODE_FLAG_50HZ
_modes_end:
num_modes: .equ [_modes_end-_modes]/modestruct_len

reti_callback:	reti.lil

video_stop:
		; Turn off GPIO video scanout
		xor a
		out0 (TMR1_CTL),a
		; Re-enable uart0 rx interrupt
		ld a,(UART0_REG_IER)
		set 0,a
		out0 (UART0_REG_IER),a
		; Enable vblank interrupt
		in0 a,(PB_ALT2)
		set 1,a
		out0 (PB_ALT2),a
		in0 a,(PB_ALT1)
		set 1,a
		out0 (PB_ALT1),a
		ret

video_init:
		; turn off uart0 rx interrupt. we have to poll for uart0 rx
		in0 a,(UART0_REG_IER)
		res 0,a
		out0 (UART0_REG_IER),a
		
		; turn off vblank interupt
		in0 a,(PB_ALT2)
		res 1,a
		out0 (PB_ALT2),a
		in0 a,(PB_ALT1)
		res 1,a
		out0 (PB_ALT1),a

		; set port c for output
		xor a
		out0 (PC_DDR),a
		out0 (PC_ALT1),a
		out0 (PC_ALT2),a
		out0 (PC_DR),a

		; set port d pin 5,6,7 (audio,vsync,hsync) to output
		in0 a,(PD_DDR)
		and 0b00011111
		out0 (PD_DDR),a

		in0 a,(PD_ALT1)
		and 0b00011111
		out0 (PD_ALT1),a

		in0 a,(PD_ALT2)
		and 0b00011111
		out0 (PD_ALT2),a

		; clear timer1 first before messing with interrupt handlers
		xor a
		out0 (TMR1_CTL),a

		; find address of ram timer1 interrupt vector
		; - so we can manipulate it directly and quickly
		ld hl,0
		ld a,(0x10c)
		ld l,a
		ld a,(0x10d)
		ld h,a
		inc hl ; skip over call (0xc3)
		ld hl,(hl) ; load address of RAM jump target
		ld a,0xc3
		ld (hl),a ; write a call opcode
		inc hl ; skip over CALL opcode
		ld (_timer1_int_vector),hl

		; clear uart0 buf
		ld hl,uart0_rx_buf
		ld (uart0_buf_pos),hl

		; ready for next audio sample
		ld a,1
		ld (audio_bitpos),a

		; Set pre_image_callback to something safe
		ld hl,reti_callback
		ld (pre_image_callback),hl

		ret

; In: a = mode number
; Out: hl = mode struct ptr
lookup_mode:
		; mode number in 'a'
		cp num_modes
		jr nc,.Linvalid_mode
		ld l,a
		ld h,modestruct_len
		mlt hl
		ld de,_modes
		add hl,de
		ret
	.Linvalid_mode:
		ld hl,0
		ret

video_set_mode:	; mode in `a`
		push ix
		push iy
		call lookup_mode
		push hl
		pop iy

		; save current mode pointer
		ld (_current_mode),hl

		; set timer1 interrupt handler to start of frame (vsync)
		ld bc,(hl)	; _modes[0] value: scanline handler
		ld hl,(_timer1_int_vector)
		ld (hl),bc
		xor a
		ld (_section_line_number),a

		; set up timer1
		ld hl,(iy+3)	; load _modes[1] value (scanline length in CPU clocks / 4)
		out0 (TMR1_DR_L),l
		out0 (TMR1_DR_H),h
		ld a, [PRT_ENABLE | PRT_CLK_DIV_4 | PRT_START | PRT_INT_ENABLE | PRT_MODE_CONTINUOUS]
		out0 (TMR1_CTL),a

		; set scanline framebuffer offset data (this implements scanline doubling/quadrupling if mode needs)
		ld ix,(fb_scanline_offsets)
		ld de,(iy+6) ; mode.width
		ld bc,(iy+9) ; mode.height
		ld iy,(iy+12) ; scan multiplier
		call fill_scanline_offset_array

		ei
	
		pop iy
		pop ix
		ret

; Params:
;   ix: fb_scanline_offsets array address
;   de: line stride (ie virtual framebuffer width in pixels)
;   bc: number of pixel rows
;   iy: scan multiplier (ie 2=doublescan)
;
; Array in ix will need to be [bc*iyl*3] bytes long.
;   eg for 156x240 mode with doublescan (480 scanned out lines)
;      there wil lbe de=156, bc=240, iyl=2
; Output:
;   array at [ix+0]..[ix+bc*iyl*3] will be filled with 24-bit scanline offsets
fill_scanline_offset_array:
		ld hl,0
	.Lloop:	; framebuffer scanline offset only increments every 4 frames, to implement quadscan
		ld a,iyl
	1:
		ld (ix+0),hl
		lea ix,ix+3
		dec a
		jr nz,1b
		add hl,de
		
		dec bc
		ld a,b
		or c
		jr nz,.Lloop
		ret

;
; common.asm
;
; fudge_factors determined experimentally (with dejitter_experiment.asm)
macro DEJITTER_PRT fudge_factor
	; sample interrupt timing jitter from PRT register (16 cycles)
	; can read every 3 cycles, while PRT decrements every 4.
	ld bc,TMR1_DR_L	; 4
	in a,(bc)	; 3
	in h,(bc)	; 3
	in l,(bc)	; 3
	in b,(bc)	; 3

	cpl
	sub a,h
	sub a,l
	sub a,b

	; convert to number of cycles lagged (by the instruction executing when interrupt was due)
	add a,\fudge_factor
	; now cycles lagged (+4) is in `a`
	; Why +4? we want 16 nops to ensure unexpected values don't
	; jump us somewhere weird (see `and 0xf`), but don't want to
	; execute any more nops than needed. expected lag cycles
	; should be 0-9, so +4 takes us further towards the end
	; of the nop region wasting fewer cycles. why not +5? +6?
	; yeah maybe could be

	; ensure it's in range 0-15 (to be robust against very delayed timer interrupts -
	; likely caused by 'di' in user code). monitor won't tolerate this, but 
	; at least don't crash the driver.
	and 0xf

	; Negate lag by computed jump into nops
	ld hl,1f
	add a,l
	ld l,a
	jp (hl)

	.balign 16
	1:
	nop
	nop
	nop
	nop

	nop  ; <- where interrupting a 1 cycle instruction will jump to
	nop  ; <- 2 cycle instruction
	nop  ; <-  ... etc ...
	nop

	nop
	nop
	nop
	nop

	nop
	nop
	nop
	nop
endm

macro DEJITTER_31KHZ_PRT
	DEJITTER_PRT 0x23
endm

macro DEJITTER_15KHZ_PRT
	DEJITTER_PRT 0x6f
endm

;
; uart0.asm
;
; pointer arith requires the ordering of these fields
; XXX note: placed at fixed location at end of 8k SRAM
audio_bitpos:	.equ 0xbfefe
audio_readpos:	.equ 0xbfeff
audio_buf:	.equ 0xbff00

; input: gpio-d value in `b`. pin-5 is audio, rest must be preserved
; output: updated gpio-d value in `a`
macro AUDIO_1BIT_SAMPLE_36_CYC
local .same_packet, .play_sample
	; 22/12 for not-taken/taken path
	ld hl,audio_bitpos	; 4
	dec (hl)		; 4
	jr nz,.same_packet	; 2/4 (not taken/taken)

	; Move to next sample packet (of 8 sample bits)
	ld (hl),8		; 3
	inc hl			; 1. hl := audio_readpos
	ld a,(hl)		; 2
	inc a			; 1
	ld (hl),a		; 2
	jr .play_sample		; 3

	; still processing packet of 8 sample bits
.same_packet:
	REP_NOP 7
	inc hl			; 1. hl := audio_readpos
	ld a,(hl)		; 2
.play_sample:
	; 14
	inc hl			; 1. hl := audio_buf
	ld l,a			; 1
	xor a			; 1
	srl (hl)		; 5. 1-bit sample to be played in carry flag
	rra			; 1
	rra			; 1
	rra			; 1. 1-bit sample now in bit 5
	res 5,b			; 2
	or b			; 1. apply to gpio-d value
endm

; Poll uart0 or update audio output
; Expects GPIO-D value in `b`, and returns it in `a` (with bit 5 as audio)
macro UART0_RX_POLL_OR_AUDIO_46_CYC
local .empty, .end
		IN0		A,(UART0_REG_LSR)	; 4 cyc. Get the line status register
		AND 		UART_LSR_RDY		; 2 cyc. Check for characters in buffer
		JR		Z,.empty		; 2 cyc (4 if taken)
		IN0		A,(UART0_REG_RBR)	; 4 cyc. Read the character from the UART receive buffer
		ld hl,(uart0_buf_pos)			; 7 cyc
		ld (hl),a				; 2 cyc
		inc hl					; 1 cyc
		ld (uart0_buf_pos),hl			; 7 cyc
		REP_NOP 13
		ld a,b		; 1 cyc. a is expected to contain preserved GPIO D value
		jr .end					; 3 cyc
	.empty:
		; 10 cyc from above
		AUDIO_1BIT_SAMPLE_36_CYC
	.end:
endm

	.include "gpiovideodriver_31khz.inc"
	.include "gpiovideodriver_15khz.inc"
#endif /* FEAT_FRAMEBUFFER */
