		.assume adl=1

		.global _hxload_vdp

; hxload_vdp received records from the VDP
; each record has
; start - byte - X:Address/Data record, 0:end transmission
; HLU   - byte - high byte address
; H     - byte - medium byte address
; L     - byte - low byte address
; N     - byte - number of databytes to read
; 0     - N-1 - bytes of data
; 
; Last record sent has N == 0, but will send address bytes, no data bytes
_hxload_vdp:
	push	de
	push	bc
	push	ix						; safeguard main ixu

	; Put VDP into hexload mode
	ld a,23
	call UART0_serial_PUTCH
	ld a,28
	call UART0_serial_PUTCH

	ld		a, 1
	ld		(firstwrite),a			; firstwrite = true	

blockloop:
	ld		d,0						; reset checksum
	call	getkey					; ask for start byte
	;push	af
	call UART0_serial_PUTCH
	;pop		af
	or		a
	jr		z, rbdone				; end of transmission received

	add		a,d
	ld		d,a
	call	getkey					; ask for byte HLU
	;push	af
	call UART0_serial_PUTCH
	;pop		af
	ld		(hexload_address+2),a	; store
	add		a,d
	ld		d,a
	call	getkey					; ask for byte H
	;push	af
	call UART0_serial_PUTCH
	;pop		af
	ld		(hexload_address+1),a	; store
	add		a,d
	ld		d,a
	call	getkey					; ask for byte L
	;push	af
	call UART0_serial_PUTCH
	;pop		af
	ld		(hexload_address),a		; store
	add		a,d
	ld		d,a
	
	call	getkey					; ask for number of bytes to receive
	;push	af
	call UART0_serial_PUTCH
	;pop		af
	ld		b,a						; loop counter
	add		a,d
	ld		d,a

	ld		hl, hexload_address		; load address of pointer
	ld		hl, (hl)				; load the (assembled) pointer from memory
	ld		a,(firstwrite)			; is this the first address we write to?
	cp		a,1
	jr		nz, 1f					; if not, skip storing the first address
	xor		a,a						; firstwrite = false
	ld		(firstwrite),a
	ld		(_startaddress),hl		; store first address
1:
	call	getkey		; receive each byte in a
	ld		(hl),a					; store byte in memory
	add		a,d
	ld		d,a
	inc		hl						; next address
	djnz	1b						; next byte
	ld		a,d
	neg								; compute 2s complement from the total checksum						
	call UART0_serial_PUTCH
	ld		(_endaddress), hl		; store end address
	jp		blockloop
	
rbdone:
	pop ix
	pop bc
	pop de
	ret

; Use new rainbow mos api
getkey:
		push de
	1:
		ld a,0x62		; mos_api_pollkeyboardevent
		ld de,eventbuf
		rst.lil 8

		and a
		jr z,1b		; no event yet...

		ld a,(e_isdown)	; only want key-down events
		and a
		jr z,1b

		ld a,(e_ascii)

		pop de
		ret

			.bss
eventbuf:
e_ascii:	.ds 1
e_kmod:		.ds 1
e_vkey:		.ds 1
e_isdown:	.ds 1
hexload_address:		DS		3	; 24bit address
hexload_error:		DS		1	; error counter
firstwrite:			DS		1	; boolean
_defaultAddress:		DS		3	; default address of Agon platform
_startaddress:		DS		3	; first address written to
_endaddress:			DS		3	; last address written to
_datarecords:		DS		3	; number of data records read
_defaultAddressUsed: DS		1	; boolean
;_datawritten		DS		1	; boolean
_linearmode:			DS		1	; boolean
