
/**
 * Setup the 6502 context (install register values and status flags) and
 * jump to reomcpiled code.
 *
 * @param code      Pointer to recompiled native code
 * @param regs      Pointer to the structure containing the register values
 * @param stack     Pointer to the start of the 6502 stack buffer
 * @param quantum   Expected (negative) number of cycles to emulate
 */
.text
.global asmEntry
.intel_syntax noprefix
asmEntry:
        push    ebp
        mov     ebp, esp
        push    ebx
        push    edi
        push    esi
//      sub     esp, 0

        /* Load status flags */
        mov     ecx, dword ptr [ebp + 12]   // Load \p regs into ecx
        pushf
        pop     edx
        and     dx, 0xf73e                  // Clear Carry, Zero, Sign, Overflow bits
        mov     bl, byte ptr [ecx + 3]      // Load 6502 status flags into %bl
        and     bl, 0x81                    // Keep bits Carry and Sign
        or      dl, bl                      // Write them to %edx
        mov     bx, 0
        mov     bl, byte ptr [ecx + 3]      // Get 6502 status flags
        and     bx, 0x42                    // Keep bits Zero and Overflow
        shl     bx, 5                       // Left shift by 5 to place them correctly
        or      dx, bx                      // Write them to %edx
        push    edx                         // Save constructed EFLAGS

        /* Load A,X,Y registers */
        mov     dh, byte ptr [ecx]
        mov     bl, byte ptr [ecx + 1]
        mov     bh, byte ptr [ecx + 2]

        /* Load SP register */
        mov     eax, dword ptr [ebp + 16]
        add     al, byte ptr [ecx + 4]
        mov     edi, eax

        /* Load cycle count into esi */
        mov     esi, dword ptr [ebp + 20]

        /* Load constructed EFLAGS */
        popf

        /* Load jump address and jump */
        mov     eax, dword ptr [ebp + 8]
        call    asm_entry
        jmp     asm_exit

asm_entry:
        /* Push EFLAGS on top of the stack */
        pushf
        jmp     eax

asm_exit:
        /* Save EFLAGS on top of the stack */
        pushf

        /* Store A,X,Y, PC registers */
        mov     ecx, dword ptr [ebp + 12]   // Load \p regs into ecx
        mov     byte ptr [ecx], dh
        mov     byte ptr [ecx + 1], bl
        mov     byte ptr [ecx + 2], bh
        mov     word ptr [ecx + 6], ax

        /* Update SP register */
        mov     eax, edi
        sub     eax, dword ptr [ebp + 16]
        mov     byte ptr [ecx + 4], al

        /* Store status flags */
        pop     edx                         // Retrieve EFLAGS
        push    edx                         // Keep a copy of the original x86 flags
        mov     bl, [ecx + 3]               // Get 6502 status flags
        and     bl, 0x3c                    // Clear Carry, Zero, Sign, Overflow bits
        and     dl, 0x81
        or      bl, dl
        pop     edx
        shr     dx, 5
        and     dl, 0x42
        or      bl, dl
        mov     [ecx + 3], bl

        /* Return cycle count in eax */
        mov     eax, esi

//      add     esp, 0
        pop     esi
        pop     edi
        pop     ebx
        mov     esp, ebp
        pop     ebp
        ret

/*
:    :
|  2 | [ebp + 20] (4th function argument)
|  2 | [ebp + 16] (3rd function argument)
|  5 | [ebp + 12] (2nd argument)
| 10 | [ebp + 8]  (1st argument)
| RA | [ebp + 4]  (return address)
| FP | [ebp]      (old ebp value)
|    | [ebp - 4]  (1st local variable)
:    :
:    :
|    | [ebp - X]  (esp - the current stack pointer. The use of push / pop is valid now)

_MyFunction3:
  push ebp
  mov ebp, esp
  sub esp, 12 ; sizeof(a) + sizeof(b) + sizeof(c)
  ;x = [ebp + 8], y = [ebp + 12], z = [ebp + 16]
  ;a = [ebp - 4] = [esp + 8], b = [ebp - 8] = [esp + 4], c = [ebp - 12] = [esp]
  mov esp, ebp
  pop ebp
  ret 12 ; sizeof(x) + sizeof(y) + sizeof(z)
*/
