	.assume adl=1
	jp start
	.align 0x40
	db "MOS",0,1

	; Raw SDcard reading demo
	; Reads and dumps boot sector
start:
		push iy

		ld hl,unlock_code
		ld a,0x70 ; sd_api_getunlockcode
		rst.lil 8
		; no return value to check

		ld hl,block_addr
		ld de,buffer
		ld bc,1 ; number of blocks to read	
		ld a,0x72 ; sd_api_readblocks
		rst.lil 8
		or a
		jp z,@read_ok

		push af
		ld hl,error_msg
		ld bc,0
		xor a
		rst.lil 0x18
		pop af
		call print_hexbyte
		call crlf
		jp exit
		
	@read_ok:
		ld hl,msg_bootsec
		ld bc,0
		xor a
		rst.lil 0x18

		ld bc,512
		ld hl,buffer

		@@:
			ld a,(hl)
			inc hl
			call print_hexbyte
			dec bc
			ld a,b
			or c
			jr nz,@b
		call crlf

exit:
		pop iy
		ld hl,0
		ret

crlf:
		ld a,13
		rst.lil 0x10
		ld a,10
		rst.lil 0x10
		ret

print_hexbyte:
		push af
		srl a
		srl a
		srl a
		srl a
		; print high nibble
		call @print_hexnibble
		pop af
		and $f
		; print low nibble
	@print_hexnibble:
		add a, 48
		cp 58
		jr c, @f
		add a, 7
	@@:	rst.lil $10
		ret

error_msg: asciz "sd_api_readblocks API error "
msg_bootsec: asciz "SDCard sector zero:\r\n"

; struct SD_safe_access {
block_addr: db 0,0,0,0
unlock_code: db 0,0,0
; }

buffer: ds 512
