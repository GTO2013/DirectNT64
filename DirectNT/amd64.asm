.code

;64 Bit Code from: https://www-user.tu-chemnitz.de/~heha/viewzip.cgi/hs/giveio64.zip/

extern KdDebuggerNotPresent:qword	;Pointer auf Byte

;void EachProcessorDpc(KDPC*Dpc, PVOID Context, PVOID Arg1, PVOID Arg2)
EachProcessorDpc proc
	push	r9			;Arg2 = Zeiger auf Affinitätsmaske
	 mov	rcx,r8			;Arg1 = Argument
	 call	rdx			;Context = Prozedurzeiger
	pop	rcx
	movzx	eax,byte ptr gs:[184h]	;KeGetCurrentProcessorNumber() für AMD64
	lock btr dword ptr[rcx],eax	;Für diesen Prozessor als erledigt markieren
	ret
EachProcessorDpc endp

;Zeiger in GDT für TSS beschaffen ->RDX
GetTssDesc proc private
	sub	rsp,16
	sgdt	[rsp+6]
	str	rdx		;Windows: 0x40
	add	rdx,[rsp+8]
	add	rsp,16
	ret
GetTssDesc endp

;GDT-Eintrag für aktuellen TSS auslesen, Offset->RAX, Länge->ECX
;Typischer Aufbau: 00700067 05008B3F FFFF8000 00000000
;Anscheinend(!) reserviert Windows (Vista) bereits 8K Platz, da sind Nullen drin.
;Daher würde es genügen, einfach das Limit in der GDT (den GDTs) zu erhöhen.
;Was zu testen wäre.
GetTSS proc private
	call	GetTssDesc
	mov	eax,[rdx+8]
	shl	rax,16
	mov	ah,[rdx+7]
	mov	al,[rdx+4]
	shl	rax,16
	mov	ax,[rdx+2]
	movzx	ecx,word ptr[rdx]	;Länge (für ein TSS genügt das)
	inc	ecx
	ret
GetTSS endp

;GDT-Eintrag für aktuellen TSS ändern, Offset=RAX, Länge=ECX
;Ruft PatchGuard auf den Plan! Dieser generiert Bluescreen 0x109 (arg4 = 3 = GDT hacked)
SetTSS proc private
	call	GetTssDesc
	pushf
	 cli
	 dec	ecx
	 mov	[rdx],cx
	 mov	[rdx+2],ax
	 shr	rax,16
	 mov	[rdx+4],al
	 mov	[rdx+7],ah
	 shr	rax,16
	 mov	[rdx+8],eax
	 str	ax
	 and	byte ptr[rdx+5],not 2	;Busy-Bit in GDT löschen (sonst kracht's beim nächsten Befehl)
	 ltr	ax			;Lade CPU-internen Cache des Task-Registers
	 mov	rax,qword ptr[KdDebuggerNotPresent]
	 cmp	byte ptr[rax],0
	 jz	@f			;Springe wenn Debugger vorhanden
	 mov	byte ptr[rdx+1],0	;Das geht wirklich! PatchGuard austricksen
@@:	popf
	ret
SetTSS endp

;void SetIOPermissionMap(int OnOff);
;Setzt TSS-Limit auf 8K oder 0 für den aktuellen Prozessor
SetIOPermissionMap proc
	test	ecx,ecx
	jnz	@@on
	call	GetTSS
	mov	ch,0
	jmp	SetTSS

@@on:	call	GetTSS
	mov	ch,20h
	jmp	SetTSS
SetIOPermissionMap endp

end
