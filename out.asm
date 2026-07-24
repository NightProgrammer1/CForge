global _start
_start:
    call main
    mov rdi, rax
    mov rax, 60
    syscall
abs:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov QWORD [rbp - 8], rdi
    mov rax, QWORD [rbp - 8]
    push rax
    mov rax, 0
    push rax
    pop rdi
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    push rax
    pop rax
    test rax, rax
    jz ifend0
    mov rax, QWORD [rbp - 8]
    push rax
    pop rax
    ifend0:
    mov rsp, rbp
    pop rbp
    ret
main:
    push rbp
    mov rbp, rsp
    mov rax, 2
    push rax
    pop rdi
    call abs
    push rax
    mov rax, 0
    push rax
    pop rax
    mov rsp, rbp
    pop rbp
    ret
