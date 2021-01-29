section .text

extern idt_raise_int_event

%macro raise_int 1
align 16
raise_int_%1:
    cld
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    mov rdi, %1
    call idt_raise_int_event
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq
%endmacro

%assign i 0
%rep 256
raise_int i
%assign i i+1
%endrep

section .data

%macro raise_int_getaddr 1
dq raise_int_%1
%endmacro

global int_thunks
int_thunks:
%assign i 0
%rep 256
raise_int_getaddr i
%assign i i+1
%endrep

section .rodata

syscall_count equ ((syscall_table.end - syscall_table) / 8)

align 16
syscall_table:
    extern syscall_debug_log
    dq syscall_debug_log
    extern syscall_mmap
    dq syscall_mmap
    extern syscall_openat
    dq syscall_openat
    extern syscall_read
    dq syscall_read
    extern syscall_write
    dq syscall_write
    extern syscall_seek
    dq syscall_seek
    extern syscall_close
    dq syscall_close
    extern syscall_set_fs_base
    dq syscall_set_fs_base
    extern syscall_ioctl
    dq syscall_ioctl
    extern syscall_getpid
    dq syscall_getpid
    extern syscall_chdir
    dq syscall_chdir
    extern syscall_mkdirat
    dq syscall_mkdirat
    extern syscall_socket
    dq syscall_socket
    extern syscall_bind
    dq syscall_bind
    extern syscall_fork
    dq syscall_fork
    extern syscall_execve
    dq syscall_execve
    extern syscall_faccessat
    dq syscall_faccessat
    extern syscall_fstatat
    dq syscall_fstatat
    extern syscall_fstat
    dq syscall_fstat
    extern syscall_getppid
    dq syscall_getppid
    extern syscall_fcntl
    dq syscall_fcntl
  .end:

section .rodata

lmao: db 0x0a, "syscall %d", 0x0a, 0
extern print

section .text

global syscall_entry
syscall_entry:
    cmp rax, syscall_count   ; is syscall_number too big?
    jae .too_big

    swapgs

    mov qword [gs:0016], rsp ; save the user stack
    mov rsp, qword [gs:0008] ; switch to the kernel space stack for the thread

    sti
    cld

    push 0x1b            ; ss
    push qword [gs:0016] ; rsp
    push r11             ; rflags
    push 0x23            ; cs
    push rcx             ; rip

    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

%if 0
    mov rdi, lmao
    mov rsi, rax
    push rax
    call print
    pop rax
%endif

    mov rdi, rsp

    call [syscall_table + rax * 8]

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    mov rdx, qword [gs:0024] ; return errno in rdx

    cli

    mov rsp, qword [gs:0016] ; restore the user stack

    swapgs

    o64 sysret

  .too_big:
    mov rax, -1
    mov rdx, 1051  ; return ENOSYS
    o64 sysret
