		.assume adl = 1	
		.global _on_crash
		.global _record_stack_highwatermark
		.global _stack_highwatermark
		.data
_stack_highwatermark:
		.d24 __stack
		.text
_record_stack_highwatermark:
		push hl
		push de
		ld hl,0
		add hl,sp
		ld de,0xb0000
		or a
		sbc hl,de
		jr c,1f		; ignore stacks moved by user below 0xb0000
		add hl,de
		ld de,(_stack_highwatermark)
		or a
		sbc hl,de
		jr nc,1f	; not new 'highest'
		add hl,de
		ld (_stack_highwatermark),hl
	1:
		pop de
		pop hl
		ret
_on_crash:
		di
		push iy
		push ix
		push hl
		push de
		push bc
		push af

		ld ix,0
		add ix,sp

		; push SPL
		pea ix-18
		; push SPS
		ld hl,0
		add.s hl,sp
		push hl
		; mb
		ld hl,0
		ld a,mb
		ld l,a
		push hl
		; PC before rst 0x38
		ld hl,(ix+18)
		push hl

		ld hl,panic_msg
		push hl
		call _printf
		pop af
		pop af
		pop af
		pop af
		pop af

		ei 		; enable interrupts so user can make keypresses
	2:	xor a 		; wait for 'r'esume
		rst.lil 08h
		cp 'r'
		jr z, 3f
		cp 'R'
		jr z, 3f
		cp 't'
		jr z, 4f
		cp 'T'
		jr z, 4f
		; esc: reboot
		cp 27
		jp z, 0
		jr 2b

		; restore everything and return to userspace
	3:	call _kbuf_clear
		pop af
		pop bc
		pop de
		pop hl
		pop ix
		pop iy
		; ret, not ret.lil, because we assume accidental entry to
		; rst38 from ADL code. so rst.lil was not used, just rst
		; Also allows rst38 handler to be used for stepping through
		; code, with rst 0x38 as a breakpoint
		ret

	4:	; 'x': Exit to mos
		call _kbuf_clear
		ld sp,__stack
		jp _mainloop

panic_msg:
		.ascii "\x11\x81\x11\x10"
		.ascii "<< RST $38 PANIC: "
		.ascii "PC:%06x MB:%02x SPS:%04x SPL:%06x "
		.ascii "AF:%06x BC:%06x DE:%06x HL:%06x "
		.ascii "IX:%06x IY:%06x >>"
		.ascii "\x11\x80\x11\x0f"
		.ascii "\r\n[r]esume, [t]erminate, [esc] reboot\r\n"
		.db 0
