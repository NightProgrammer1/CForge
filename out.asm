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
    mov dword [rbp - 4], 10
    mov dword [rbp - 8], 0
    mov r11d, dword [rbp - 4]
    cmp r11d, 5
    jne if_false_1
    if_true_0:
    mov dword [rbp - 8], 1
    jmp if_end_2
    if_false_1:
    mov r11d, dword [rbp - 4]
    cmp r11d, 10
    jne if_false_4
    if_true_3:
    mov dword [rbp - 8], 2
    jmp if_end_5
    if_false_4:
    mov dword [rbp - 8], 3
    if_end_5:
    if_end_2:
    mov eax, dword [rbp - 8]
    sub eax, 2
    mov rsp, rbp
    pop rbp
    ret
