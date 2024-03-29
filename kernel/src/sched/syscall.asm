%macro push_all 0
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rdi
    push rsi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro pop_all 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
%endmacro

global syscall_entry:function

extern syscall_handler

syscall_entry:
    swapgs

    mov byte [gs:0x20], 0x1 ; Set in_syscall to true
    mov [gs:0x18], rsp      ; Save current user stack
    mov rsp, [gs:0x10]      ; Move in the kernel stack

    push qword 0x23      ; User data selector
    push qword [gs:0x18] ; Saved user stack
    push r11             ; Saved CPU flags
    push qword 0x2b      ; User code selector
    push rcx             ; Current RIP

    push qword 0 ; Made up error code
    push qword 0 ; Made up ISR number

    push_all

    mov rdi, rsp
    mov rbp, 0

    call syscall_handler

    pop_all
    add rsp, 8 ; Compensate for not popping rax

    mov rsp, [gs:0x18]      ; Restore the user stack
    mov byte [gs:0x20], 0x0 ; Set in_syscall to false

    swapgs
    o64 sysret
