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
    mov dword [rbp - 4], 0
    mov dword [rbp - 8], 99
    mov dword [rbp - 12], 0
    mov r11d, dword [rbp - 4]
    test r11d, r11d
    jne if_end_2
    if_true_0:
    mov eax, dword [rbp - 12]
    add eax, 10
    mov dword [rbp - 12], eax
    jmp if_end_2
    if_end_2:
    mov r11d, dword [rbp - 8]
    test r11d, r11d
    jne if_end_5
    if_true_3:
    mov eax, dword [rbp - 12]
    add eax, 100
    mov dword [rbp - 12], eax
    jmp if_end_5
    if_end_5:
    mov eax, dword [rbp - 12]
    mov rsp, rbp
    pop rbp
    ret
