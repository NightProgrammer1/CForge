global _start
_start:
    call main
    mov rdi, rax
    mov rax, 60
    syscall
add:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov QWORD [rbp - 8], rdi
    mov QWORD [rbp - 16], rsi
    mov QWORD [rbp - 24], rdx
    mov QWORD [rbp - 32], rcx
    mov QWORD [rbp - 40], r8
    mov rax, QWORD [rbp - 8]
    push rax
    mov rax, QWORD [rbp - 16]
    push rax
    pop rdi
    pop rax
    add rax, rdi
    push rax
    mov rax, QWORD [rbp - 24]
    push rax
    pop rdi
    pop rax
    add rax, rdi
    push rax
    mov rax, QWORD [rbp - 32]
    push rax
    pop rdi
    pop rax
    add rax, rdi
    push rax
    mov rax, QWORD [rbp - 40]
    push rax
    pop rdi
    pop rax
    add rax, rdi
    push rax
    mov QWORD [rbp - 48], rax
    mov rax, QWORD [rbp - 48]
    push rax
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret
main:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov rax, 1
    mov rdi, rax
    mov rax, 2
    mov rsi, rax
    mov rax, 3
    mov rdx, rax
    mov rax, 4
    mov rcx, rax
    mov rax, 5
    mov r8, rax
    call add
    mov QWORD [rbp - 8], rax
    mov rax, QWORD [rbp - 8]
    push rax
    mov rdi, rax
    mov rax, 60
    syscall
    mov rsp, rbp
    pop rbp
    ret
