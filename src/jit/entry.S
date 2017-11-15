
/**
 * Setup the 6502 context (install register values and status flags) and
 * jump to reomcpiled code.
 *
 * @param code      Pointer to recompiled native code
 * @param regs      Pointer to the structure containing the register values
 * @param stack     Pointer to the top of the stack
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
        push    edx                         // Override the original flags with
        popf                                // the constructed value

        /* Load A,X,Y registers */
        mov     dh, byte ptr [ecx]
        mov     bl, byte ptr [ecx + 1]
        mov     bh, byte ptr [ecx + 2]

        /* Load stack pointer into edi */
        mov     edi, dword ptr [ebp + 16]
        mov     edi, dword ptr [edi]

        /* Load cycle count into esi */
        mov     esi, dword ptr [ebp + 20]

        /* Load jump address and jump */
        mov     eax, dword ptr [ebp + 8]
        call    asm_entry
        jmp     asm_exit

asm_entry:
        pushf
        jmp     eax

asm_exit:
        /* Store A,X,Y, PC registers */
        mov     ecx, dword ptr [ebp + 12]   // Load \p regs into ecx
        mov     byte ptr [ecx], dh
        mov     byte ptr [ecx + 1], bl
        mov     byte ptr [ecx + 2], bh
        mov     word ptr [ecx + 6], ax

        /* Store status flags */
        pushf                               // Load x86 status flags into %edx
        pop     edx
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

        /* Update stack pointer */
        mov     eax, dword ptr [ebp + 16]
        mov     dword ptr [eax], edi

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
