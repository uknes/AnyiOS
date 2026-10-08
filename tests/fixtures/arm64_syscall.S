.text
.globl _anyios_syscall_demo
.p2align 2
_anyios_syscall_demo:
    mov x0, #1
    mov x16, #4
    svc #0x80
    mov x0, #42
    ret
