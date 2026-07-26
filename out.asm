global _start
_start:
    call main
    mov rdi, rax
    mov rax, 60
    syscall
test:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov QWORD [rbp - 8], rdi
    mov rax, 1
    push rax
    pop rax
    mov rsp, rbp
    pop rbp
    ret
main:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov rax, 10
    push rax
    mov QWORD [rbp - 8], rax
    mov rax, 2
    push rax
    pop rax
    mov rsp, rbp
    pop rbp
    ret
