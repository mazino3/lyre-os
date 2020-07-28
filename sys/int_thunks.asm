; Interrupt thunks

section .bss
global int_event
align 16
int_event: resq 256

%macro raise_int 1
align 16
raise_int_%1:
    lock inc dword [int_event+%1*8]
    iretq
%endmacro

section .text

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

