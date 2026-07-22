global _start
_start:
    call main
    mov rdi, rax
    mov rax, 60
    syscall
func:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov rax, 30
    mov QWORD [rbp - 8], rax
    mov rax, 20
    mov QWORD [rbp - 16], rax
    mov rax, QWORD [rbp - 8]
    push rax
    mov rax, QWORD [rbp - 16]
    push rax
    pop rdi
    pop rax
    cmp rax, rdi
    setg al
    movzx rax, al
    push rax
    pop rax
    test rax, rax
    jz ifend0
    mov rax, 2
    mov rsp, rbp
    pop rbp
    ret
    ifend0:
    mov rsp, rbp
    pop rbp
    ret
main:
    push rbp
    mov rbp, rsp
    sub rsp, 8
    call func
    mov QWORD [rbp - 8], rax
    mov rax, QWORD [rbp - 8]
    push rax
    mov rdi, rax
    mov rax, 60
    syscall
    mov rsp, rbp
    pop rbp
    ret
