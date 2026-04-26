# fiber_switch_internal_linux.s
# Linux x64 System V AMD64 ABI (GAS)

# Keep constant with fibers.h FiberContext
.set FIBER_RBX,  0
.set FIBER_RBP,  8
.set FIBER_R12, 16
.set FIBER_R13, 24
.set FIBER_R14, 32
.set FIBER_R15, 40
.set FIBER_RDI, 48 
.set FIBER_RSP, 56
.set FIBER_RIP, 64

.text
.globl fiber_switch_internal

# void fiber_switch_internal(FiberContext* from, const FiberContext* to)
# rdi = from, rsi = to
fiber_switch_internal:
    # Save current non-volatile registers into 'from'
    mov  %rbx, FIBER_RBX(%rdi)
    mov  %rbp, FIBER_RBP(%rdi)
    mov  %r12, FIBER_R12(%rdi)
    mov  %r13, FIBER_R13(%rdi)
    mov  %r14, FIBER_R14(%rdi)
    mov  %r15, FIBER_R15(%rdi)
    # NOTE: rdi itself is not saved — it is caller-saved on Linux and only
    #       meaningful as a bootstrap argument, written once during fiber setup

    # return address
    mov  (%rsp), %rax
    mov  %rax, FIBER_RIP(%rdi)
    # stack pointer
    lea  8(%rsp), %rax          # save pointer value, not memory contents at rsp+8
    mov  %rax, FIBER_RSP(%rdi)

    # Restore non-volatile registers from 'to'
    mov  FIBER_RBX(%rsi), %rbx
    mov  FIBER_RBP(%rsi), %rbp
    mov  FIBER_R12(%rsi), %r12
    mov  FIBER_R13(%rsi), %r13
    mov  FIBER_R14(%rsi), %r14
    mov  FIBER_R15(%rsi), %r15
    mov  FIBER_RDI(%rsi), %rdi  # first arg on Linux goes in rdi

    mov  FIBER_RSP(%rsi), %rsp  # set stack pointer to the new fiber's stack
    mov  FIBER_RIP(%rsi), %rax  # jump to the new fiber's instruction pointer
    jmp  *%rax