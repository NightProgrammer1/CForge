global _start
_start:
    call main
    mov rdi, rax
    mov rax, 60
    syscall
main:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov rax, 12
    push rax
    mov QWORD [rbp - 8], rax
    mov rax, 15
    push rax
    mov QWORD [rbp - 16], rax
    mov rax, 15
    push rax
    pop rax
    mov rsp, rbp
    pop rbp
    ret
