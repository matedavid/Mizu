; fiber_switch_internal.asm
; Windows x64 MASM

OPTION CASEMAP:NONE

; Keep constant with fibers.h FiberContext
FIBER_RBX   EQU  0
FIBER_RBP   EQU  8
FIBER_R12   EQU 16
FIBER_R13   EQU 24
FIBER_R14   EQU 32
FIBER_R15   EQU 40
FIBER_RDI   EQU 48
FIBER_RSI   EQU 56
FIBER_RCX   EQU 64
FIBER_RSP   EQU 72
FIBER_RIP   EQU 80
FIBER_XMM6  EQU 96
FIBER_XMM7  EQU 112
FIBER_XMM8  EQU 128
FIBER_XMM9  EQU 144
FIBER_XMM10 EQU 160
FIBER_XMM11 EQU 176
FIBER_XMM12 EQU 192
FIBER_XMM13 EQU 208
FIBER_XMM14 EQU 224
FIBER_XMM15 EQU 240

.code

; void fiber_switch_internal(FiberContext& from, const FiberContext& to)
; rcx = from, rdx = to
fiber_switch_internal PROC PUBLIC
	; Save current non-volatile registers into 'from'
	mov		[rcx + FIBER_RBX], rbx
	mov		[rcx + FIBER_RBP], rbp
	mov		[rcx + FIBER_R12], r12
	mov		[rcx + FIBER_R13], r13
	mov		[rcx + FIBER_R14], r14
	mov     [rcx + FIBER_R15], r15
	mov     [rcx + FIBER_RDI], rdi
	mov     [rcx + FIBER_RSI], rsi

    movdqu  XMMWORD PTR [rcx + FIBER_XMM6],  xmm6
    movdqu  XMMWORD PTR [rcx + FIBER_XMM7],  xmm7
    movdqu  XMMWORD PTR [rcx + FIBER_XMM8],  xmm8
    movdqu  XMMWORD PTR [rcx + FIBER_XMM9],  xmm9
    movdqu  XMMWORD PTR [rcx + FIBER_XMM10], xmm10
    movdqu  XMMWORD PTR [rcx + FIBER_XMM11], xmm11
    movdqu  XMMWORD PTR [rcx + FIBER_XMM12], xmm12
    movdqu  XMMWORD PTR [rcx + FIBER_XMM13], xmm13
    movdqu  XMMWORD PTR [rcx + FIBER_XMM14], xmm14
    movdqu  XMMWORD PTR [rcx + FIBER_XMM15], xmm15

	; return address
	mov     rax, [rsp]
	mov     [rcx + FIBER_RIP], rax
	; stack pointer
    lea		rax, [rsp + 8]          ; save pointer value, not memory contents at rsp+8
	mov		[rcx + FIBER_RSP], rax

	; Restore non-volatile registers from 'to'
    mov     rbx, [rdx + FIBER_RBX]
    mov     rbp, [rdx + FIBER_RBP]
    mov     r12, [rdx + FIBER_R12]
    mov     r13, [rdx + FIBER_R13]
    mov     r14, [rdx + FIBER_R14]
    mov     r15, [rdx + FIBER_R15]
    mov     rdi, [rdx + FIBER_RDI]
    mov     rsi, [rdx + FIBER_RSI]
    mov     rcx, [rdx + FIBER_RCX]

	movdqu  xmm6,  XMMWORD PTR [rdx + FIBER_XMM6]
    movdqu  xmm7,  XMMWORD PTR [rdx + FIBER_XMM7]
    movdqu  xmm8,  XMMWORD PTR [rdx + FIBER_XMM8]
    movdqu  xmm9,  XMMWORD PTR [rdx + FIBER_XMM9]
    movdqu  xmm10, XMMWORD PTR [rdx + FIBER_XMM10]
    movdqu  xmm11, XMMWORD PTR [rdx + FIBER_XMM11]
    movdqu  xmm12, XMMWORD PTR [rdx + FIBER_XMM12]
    movdqu  xmm13, XMMWORD PTR [rdx + FIBER_XMM13]
    movdqu  xmm14, XMMWORD PTR [rdx + FIBER_XMM14]
    movdqu  xmm15, XMMWORD PTR [rdx + FIBER_XMM15]

    mov     rsp, [rdx + FIBER_RSP] ; set stack pointer to the new fiber's stack
    mov     rax, [rdx + FIBER_RIP] ; jump to the new fiber's instruction pointer
    jmp     rax

fiber_switch_internal ENDP

END