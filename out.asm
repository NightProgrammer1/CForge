global _start
_start:
    call main
    mov rdi, rax
    mov rax, 60
    syscall
isOdd:
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
    sete al
    movzx rax, al
    push rax
    pop rax
    test rax, rax
    jz ifend0
    mov rax, 0
    push rax
    pop rax
    mov rsp, rbp
    pop rbp
    ret
    ifend0:
    mov rax, QWORD [rbp - 8]
    push rax
    mov rax, 1
    push rax
    pop rdi
    pop rax
    sub rax, rdi
    push rax
    pop rdi
    call isEven
    push rax
    pop rax
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret
isEven:
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
    sete al
    movzx rax, al
    push rax
    pop rax
    test rax, rax
    jz ifend1
    mov rax, 1
    push rax
    pop rax
    mov rsp, rbp
    pop rbp
    ret
    ifend1:
    mov rax, QWORD [rbp - 8]
    push rax
    mov rax, 1
    push rax
    pop rdi
    pop rax
    sub rax, rdi
    push rax
    pop rdi
    call isOdd
    push rax
    pop rax
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret
main:
    push rbp
    mov rbp, rsp
    mov rax, 7
    push rax
    pop rdi
    call isEven
    push rax
    pop rax
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret
