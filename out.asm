global _start
_start:
    mov rax, 0
    push rax
    push QWORD [rsp + 0]
    while_start0:
    push QWORD [rsp + 8]
    push QWORD [rsp + 8]
    pop rdi
    pop rax
    add rax, rdi
    push rax
    pop rax
    test rax, rax
    jz while_end1
    jmp while_start0
    while_end1:
    push QWORD [rsp + 0]
    mov rax, 60
    pop rdi
    syscall
    mov rax, 60
    mov rdi, 0
    syscall