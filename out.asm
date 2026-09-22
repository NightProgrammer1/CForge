global _start
_start:
    call main
    mov rdi, rax
    mov rax, 60
    syscall
main:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    lea rax, [rbp - 16]
    mov rcx, 0
    imul rcx, 4
    add rax, rcx
    mov dword [rax], 100
    lea rax, [rbp - 16]
    mov rcx, 1
    imul rcx, 4
    add rax, rcx
    mov dword [rax], 200
    lea rax, [rbp - 16]
    mov rcx, 2
    imul rcx, 4
    add rax, rcx
    mov dword [rax], 300
    lea rax, [rbp - 16]
    mov rcx, 0
    imul rcx, 4
    add rax, rcx
    mov qword [rbp - 24], rax
    mov rax, 2
    imul rax, 4
    add rax, qword [rbp - 24]
    mov qword [rbp - 24], rax
    mov r11, qword [rbp - 24]
    mov dword [r11], 400
    lea rax, [rbp - 16]
    mov rcx, 0
    imul rcx, 4
    add rax, rcx
    mov eax, dword [rax]
    lea rcx, [rbp - 16]
    mov rdx, 1
    imul rdx, 4
    add rcx, rdx
    mov ecx, dword [rcx]
    add eax, ecx
    lea rcx, [rbp - 16]
    mov rdx, 2
    imul rdx, 4
    add rcx, rdx
    mov ecx, dword [rcx]
    add eax, ecx
    mov rsp, rbp
    pop rbp
    ret
