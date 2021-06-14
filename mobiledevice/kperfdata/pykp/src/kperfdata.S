.text
.global _kpdecode_cursor_setchunk
.global _kpdecode_cursor_free
.global _kpdecode_record_free
.global _kpdecode_cursor_set_option
.global _kpdecode_cursor_clearchunk
.global _kpdecode_cursor_create
.global _kpdecode_cursor_next_record
.global _get_rbp
.balign 16

#include "plt.S"

__kpdecode_cursor_setchunk:
    pushq   %rbp
    movq    %rsp, %rbp
    movl    $4294967294, %eax
    cmpq    $0, 24(%rdi)
    jne     .L0
    movq    %rsi, 24(%rdi)
    movq    $0, 40(%rdi)
    movq    %rdx, 32(%rdi)
    movq    %rdx, 48(%rdi)
    movq    %rsi, 80(%rdi)
    xorl    %eax, %eax
.L0:
    popq    %rbp
    retq

__kpdecode_cursor_clearchunk:
    pushq   %rbp
    movq    %rsp, %rbp
    movq    24(%rdi), %rax
    testq   %rax, %rax
    je      .L1
    leaq    24(%rdi), %rcx
    xorl    %edx, %edx
    movq    %rdx, 96(%rdi)
    movq    %rdx, 88(%rdi)
    movq    %rdx, 80(%rdi)
    movq    %rdx, 24(%rcx)
    movq    %rdx, 16(%rcx)
    movq    %rdx, 8(%rcx)
    movq    %rdx, (%rcx)
    movb    $1, 168(%rdi)
.L1:
    popq    %rbp
    retq

_kpdecode_cursor_create:
    pushq   %rbp
    movq    %rsp, %rbp
    movl    $1, %edi
    movl    $1248, %esi
    popq    %rbp
    jmp     __plt_calloc

__kpdecode_cursor_free:
    pushq   %rbp
    movq    %rsp, %rbp
    popq    %rbp
    jmp     __plt_free

_kpdecode_cursor_flush:
    pushq   %rbp
    movq    %rsp, %rbp
    popq    %rbp
    retq

_kpdecode_cursor_get_stats:
    pushq   %rbp
    movq    %rsp, %rbp
    movq    $-1, %rax
    cmpl    $1, %esi
    je      gs2f
    testl   %esi, %esi
    jne     gs4a
    cmpl    $0, 64(%rdi)
    je      gs4a
    movq    176(%rdi), %rax
    testq   %rax, %rax
    je      gs3e
    movq    3008(%rax), %rax
    jmp     gs4a
gs2f:
    cmpl    $0, 64(%rdi)
    je      gs4a
    movslq  196(%rdi), %rax
    jmp     gs4a
gs3e:
    movslq  192(%rdi), %rax
    imulq   8(%rdi), %rax
gs4a:
    popq    %rbp
    retq

__kpdecode_cursor_set_option:
    pushq   %rbp
    movq    %rsp, %rbp
    movq    $-1, %rax
    testl   %esi, %esi
    jne     so20
    movzbl  1244(%rdi), %eax
    testq   %rdx, %rdx
    setne   1244(%rdi)
so20:
    popq    %rbp
    retq

__kpdecode_record_free:
    pushq   %rbp
    movq    %rsp, %rbp
    pushq   %rbx
    pushq   %rax
    movq    %rdi, %rbx
    movq    2616(%rdi), %rdi
    testq   %rdi, %rdi
    je      krf1a
    callq   __plt_free
krf1a:    
    movq    %rbx, %rdi
    addq    $8, %rsp
    popq    %rbx
    popq    %rbp
    jmp     __plt_free

__kpdecode_cursor_next_kevent:
    movq 24(%rdi), %rcx
    testq %rcx, %rcx
    je __kpdecode_cursor_next_kevent$152
    pushq %rbp
    movq %rsp, %rbp
    pushq %r14
    pushq %rbx
    movl (%rdi), %eax
    testl %eax, %eax
    jne __kpdecode_cursor_next_kevent$eb
    movq 32(%rdi), %r11
    xorl %eax, %eax
    cmpq $288, %r11
    jb __kpdecode_cursor_next_kevent$eb
    movl (%rcx), %esi
    cmpl $1437204992, %esi
    je __kpdecode_cursor_next_kevent$55
    cmpl $1437204737, %esi
    jne __kpdecode_cursor_next_kevent$eb
    movl $2, (%rdi)
    movl $24, %r8d
    jmp __kpdecode_cursor_next_kevent$7d
__kpdecode_cursor_next_kevent$55:
    movl 20(%rcx), %eax
    andl $1, %eax
    leal 1(%rax), %esi
    movl %esi, (%rdi)
    movl $288, %r8d
    testl %eax, %eax
    jne __kpdecode_cursor_next_kevent$7d
    movl $1, %eax
    movl $28, %r9d
    movl $32, %r14d
    jmp __kpdecode_cursor_next_kevent$8e
__kpdecode_cursor_next_kevent$7d:
    movl $2, %eax
    movl $32, %r9d
    movl $64, %r14d
__kpdecode_cursor_next_kevent$8e:
    leaq 24(%rdi), %r10
    movq %r14, 8(%rdi)
    movq %r9, 16(%rdi)
    movslq 4(%rcx), %rdx
    imulq %r9, %rdx
    leaq (%rdx,%r8), %rsi
    leaq 4096(%rdx,%r8), %rbx
    andq $-4096, %rbx
    testl $4092, %esi
    cmoveq %rsi, %rbx
    movl $1, 64(%rdi)
    movq %r10, 72(%rdi)
    addq %rbx, %r14
    addq %rcx, %rbx
    xorl %esi, %esi
    cmpq %r14, %r11
    cmovaeq %rbx, %rsi
    movq %rsi, 80(%rdi)
    addq %r8, %rcx
    movq %rcx, 88(%rdi)
    addq %rdx, %rcx
    movq %rcx, 96(%rdi)
__kpdecode_cursor_next_kevent$eb:
    cmpl $0, 64(%rdi)
    popq %rbx
    popq %r14
    popq %rbp
    je __kpdecode_cursor_next_kevent$152
    cmpb $0, 168(%rdi)
    jne __kpdecode_cursor_next_kevent$13f
    movq 88(%rdi), %rcx
    testq %rcx, %rcx
    je __kpdecode_cursor_next_kevent$13f
    movq 96(%rdi), %rdx
    cmpq %rdx, %rcx
    jae __kpdecode_cursor_next_kevent$138
    xorl %esi, %esi
    cmpl $1, %eax
    setne %sil
    leaq 4(,%rsi,4), %rsi
__kpdecode_cursor_next_kevent$121:
    cmpl $0, (%rcx,%rsi)
    jne __kpdecode_cursor_next_kevent$1c4
    addq 16(%rdi), %rcx
    movq %rcx, 88(%rdi)
    cmpq %rcx, %rdx
    ja __kpdecode_cursor_next_kevent$121
__kpdecode_cursor_next_kevent$138:
    movb $1, 168(%rdi)
__kpdecode_cursor_next_kevent$13f:
    movq 80(%rdi), %rcx
    testq %rcx, %rcx
    je __kpdecode_cursor_next_kevent$152
    cmpl $2, %eax
    jne __kpdecode_cursor_next_kevent$155
    movq %rcx, %rax
    jmp __kpdecode_cursor_next_kevent$1a3
__kpdecode_cursor_next_kevent$152:
    xorl %eax, %eax
    retq
__kpdecode_cursor_next_kevent$155:
    movq (%rcx), %rdx
    movabsq $72057594037927935, %rax
    andq %rdx, %rax
    movq %rax, 104(%rdi)
    pmovzxdq 8(%rcx), %xmm0
    movdqu %xmm0, 112(%rdi)
    pmovzxdq 16(%rcx), %xmm0
    leaq 104(%rdi), %rax
    movdqu %xmm0, 128(%rdi)
    movl 24(%rcx), %esi
    movq %rsi, 144(%rdi)
    movl 28(%rcx), %esi
    movl %esi, 152(%rdi)
    shrq $56, %rdx
    movl %edx, 156(%rdi)
__kpdecode_cursor_next_kevent$1a3:
    addq 8(%rdi), %rcx
    movq %rcx, 80(%rdi)
    movq 72(%rdi), %rdx
    movq (%rdx), %rsi
    addq 24(%rdx), %rsi
    cmpq %rcx, %rsi
    ja __kpdecode_cursor_next_kevent$1c3
    movq $0, 80(%rdi)
__kpdecode_cursor_next_kevent$1c3:
    retq
__kpdecode_cursor_next_kevent$1c4:
    leaq 112(%rdi), %rdx
    movq $0, 104(%rdi)
    movq $117506056, 152(%rdi)
    cmpl $1, %eax
    jne __kpdecode_cursor_next_kevent$1f9
    movl 24(%rcx), %eax
    movl %eax, 16(%rdx)
    movq 8(%rcx), %rax
    movq 16(%rcx), %rsi
    movq %rsi, 8(%rdx)
    movq %rax, (%rdx)
    movl (%rcx), %eax
    jmp __kpdecode_cursor_next_kevent$211
__kpdecode_cursor_next_kevent$1f9:
    movl 28(%rcx), %eax
    movl %eax, 16(%rdx)
    movq 12(%rcx), %rax
    movq 20(%rcx), %rsi
    movq %rsi, 8(%rdx)
    movq %rax, (%rdx)
    movq (%rcx), %rax
__kpdecode_cursor_next_kevent$211:
    movq %rax, 144(%rdi)
    movq 16(%rdi), %rax
    addq %rax, 88(%rdi)
    leaq 104(%rdi), %rax
    retq
    nop
    nop
    nop

__kpdecode_cursor_next_record:
    pushq %rbp
    movq %rsp, %rbp
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbx
    pushq %rax
    movq %rsi, -48(%rbp)
    movq %rdi, %rbx
_kpdecode_cursor_next_record$15:
    movq %rbx, %rdi
    callq _record_ready
    testb %al, %al
    jne _kpdecode_cursor_next_record$d2d
    movq %rbx, %rdi
    callq __kpdecode_cursor_next_kevent
    testq %rax, %rax
    je _kpdecode_cursor_next_record$d2d
    movq %rax, %r12
    movq %rbx, %r13
    movslq 192(%rbx), %rbx
    incq %rbx
    movl %ebx, 192(%r13)
    movl $3016, %edi
    callq __plt_malloc
    testq %rax, %rax
    je _kpdecode_cursor_next_record$d85
    movq %rax, %r15
    xorl %eax, %eax
    movl %eax, 2976(%r15)
    movabsq $-4294967296, %rcx
    movq %rcx, 3000(%r15)
    movl %eax, 24(%r15)
    movq %rax, 16(%r15)
    movq %rax, 8(%r15)
    movq %rax, (%r15)
    movq %rax, 2616(%r15)
    movl %eax, 2168(%r15)
    movq (%r12), %rax
    movq %rax, 8(%r15)
    imulq 8(%r13), %rbx
    movq %rbx, 3008(%r15)
    movl 52(%r12), %r14d
    cmpq $32, %r14
    jb _kpdecode_cursor_next_record$14a
    movl 48(%r12), %eax
    movl %eax, 48(%r15)
    movq 8(%r12), %rax
    movq %rax, 56(%r15)
    movq 16(%r12), %rax
    movq %rax, 64(%r15)
    movq 24(%r12), %rax
    movq %rax, 72(%r15)
    movq 32(%r12), %rax
    movq %rax, 80(%r15)
    movq 40(%r12), %rax
    movq %rax, 16(%r15)
    movl %r14d, 24(%r15)
    movabsq $-9223372036854775808, %rax
    leaq 23(%rax), %rax
    movq %rax, (%r15)
    movl $1, 2976(%r15)
    incl 196(%r13)
    movq $0, 2984(%r15)
    movq 184(%r13), %rax
    testq %rax, %rax
    movq %r13, %rbx
    je _kpdecode_cursor_next_record$13b
    movq %r15, 2984(%rax)
_kpdecode_cursor_next_record$13b:
    movq %r15, 184(%rbx)
    xorl %r13d, %r13d
    jmp _kpdecode_cursor_next_record$860
_kpdecode_cursor_next_record$14a:
    testq %rax, %rax
    movq %r13, %rbx
    je _kpdecode_cursor_next_record$180
    cmpl $630784000, 48(%r12)
    je _kpdecode_cursor_next_record$180
    movq 968(%rbx,%r14,8), %rax
    incq %rax
    movq %rax, 968(%rbx,%r14,8)
    cmpq 1224(%rbx), %rax
    jbe _kpdecode_cursor_next_record$180
    movq %rax, 1224(%rbx)
_kpdecode_cursor_next_record$180:
    cmpb $0, 1244(%rbx)
    je _kpdecode_cursor_next_record$1d1
    movq $23, (%r15)
    movl 48(%r12), %eax
    movl %eax, 48(%r15)
    movq 8(%r12), %rcx
    movq %rcx, 56(%r15)
    movq 16(%r12), %rcx
    movq %rcx, 64(%r15)
    movq 24(%r12), %rcx
    movq %rcx, 72(%r15)
    movq 32(%r12), %rcx
    movq %rcx, 80(%r15)
    movq 40(%r12), %rcx
    movq %rcx, 16(%r15)
    movl %r14d, 24(%r15)
    movl $23, %ecx
    jmp _kpdecode_cursor_next_record$1d8
_kpdecode_cursor_next_record$1d1:
    movl 48(%r12), %eax
    xorl %ecx, %ecx
_kpdecode_cursor_next_record$1d8:
    cmpl $117571592, %eax
    jne _kpdecode_cursor_next_record$287
    orq $65539, %rcx
    movq %rcx, (%r15)
    movl %r14d, 24(%r15)
    movq (%r12), %rax
    movq %rax, 8(%r15)
    movq 712(%rbx,%r14,8), %rcx
    movq %rcx, 2624(%r15)
    movl $1, 2976(%r15)
    movq %rax, 712(%rbx,%r14,8)
    movq 200(%rbx,%r14,8), %rax
    testq %rax, %rax
    je _kpdecode_cursor_next_record$24b
    movabsq $-9223372036854775808, %rcx
    orq %rcx, (%rax)
    movl $1, 2976(%rax)
    movq $0, 200(%rbx,%r14,8)
_kpdecode_cursor_next_record$24b:
    movq 456(%rbx,%r14,8), %rax
    xorl %r13d, %r13d
    testq %rax, %rax
    je _kpdecode_cursor_next_record$817
    movabsq $-9223372036854775808, %rcx
    orq %rcx, (%rax)
    movl $1, 2976(%rax)
    movq $0, 456(%rbx,%r14,8)
    jmp _kpdecode_cursor_next_record$817
_kpdecode_cursor_next_record$287:
    movq (%r12), %rdx
    movq %rdx, 712(%rbx,%r14,8)
    cmpl $620756993, %eax
    jne _kpdecode_cursor_next_record$2ee
    movl $2, %r13d
    cmpq $0, 200(%rbx,%r14,8)
    jne _kpdecode_cursor_next_record$817
    movq %r15, 200(%rbx,%r14,8)
    orq $8199, %rcx
    movq %rcx, (%r15)
    movl %r14d, 24(%r15)
    movl 16(%r12), %eax
    movl %eax, 2584(%r15)
    movl 24(%r12), %eax
    movl %eax, 2588(%r15)
    movq 40(%r12), %rax
    movq %rax, 16(%r15)
_kpdecode_cursor_next_record$2e6:
    xorl %r13d, %r13d
    jmp _kpdecode_cursor_next_record$817
_kpdecode_cursor_next_record$2ee:
    movl %eax, %edx
    andl $-4, %edx
    cmpl $117506048, %edx
    jne _kpdecode_cursor_next_record$353
    testb $1, %al
    jne _kpdecode_cursor_next_record$3bc
    movq 456(%rbx,%r14,8), %r13
    testq %r13, %r13
    je _kpdecode_cursor_next_record$4e1
    movq %r12, %rsi
    addq $8, %rsi
    movl $4, %edx
    movq %rbx, %rdi
    movq %r13, %rcx
    callq _batch_get_bytes
    testb $2, 48(%r12)
    je _kpdecode_cursor_next_record$803
    movl $1, 2976(%r13)
    movq $0, 456(%rbx,%r14,8)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$353:
    movq 200(%rbx,%r14,8), %r13
    testq %r13, %r13
    je _kpdecode_cursor_next_record$424
    cmpl $620888079, %eax
    jg _kpdecode_cursor_next_record$473
    cmpl $620822587, %eax
    jg _kpdecode_cursor_next_record$523
    cmpl $620822563, %eax
    jle _kpdecode_cursor_next_record$5d9
    cmpl $620822579, %eax
    jg _kpdecode_cursor_next_record$894
    cmpl $620822564, %eax
    je _kpdecode_cursor_next_record$9d9
    cmpl $620822572, %eax
    jne _kpdecode_cursor_next_record$803
    orb $-128, 2(%r13)
    movq 8(%r12), %rax
    movq %rax, 2576(%r13)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$3bc:
    orq $524288, %rcx
    movq %rcx, (%r15)
    movl %r14d, 24(%r15)
    movq %r15, %rdi
    addq $2640, %rdi
    movl $280, %esi
    callq __plt_free
    movl $2, %r13d
    cmpq $0, 456(%rbx,%r14,8)
    jne _kpdecode_cursor_next_record$817
    movq 16(%r12), %rax
    movq %rax, 2904(%r15)
    movl 8(%r12), %eax
    movl %eax, 2912(%r15)
    testb $2, 48(%r12)
    jne _kpdecode_cursor_next_record$5b2
    movq %r15, 456(%rbx,%r14,8)
    jmp _kpdecode_cursor_next_record$5bd
_kpdecode_cursor_next_record$424:
    cmpl $620953608, %eax
    jg _kpdecode_cursor_next_record$4ec
    cmpl $117506056, %eax
    je _kpdecode_cursor_next_record$706
    cmpl $620822548, %eax
    je _kpdecode_cursor_next_record$749
    cmpl $620953600, %eax
    jne _kpdecode_cursor_next_record$77b
    orq $16777216, %rcx
    movq %rcx, (%r15)
    movl %r14d, 24(%r15)
    movl $1, 2976(%r15)
    movl $620953600, %eax
    jmp _kpdecode_cursor_next_record$77b
_kpdecode_cursor_next_record$473:
    cmpl $621150243, %eax
    jg _kpdecode_cursor_next_record$559
    cmpl $621150219, %eax
    jle _kpdecode_cursor_next_record$60f
    cmpl $621150231, %eax
    jg _kpdecode_cursor_next_record$8cc
    cmpl $621150220, %eax
    je _kpdecode_cursor_next_record$a18
    cmpl $621150224, %eax
    jne _kpdecode_cursor_next_record$803
    orb $8, 1(%r13)
    movups 8(%r12), %xmm0
    movups 24(%r12), %xmm1
    movaps %xmm0, %xmm2
    unpckhpd %xmm1, %xmm2
    movlhps %xmm1, %xmm0
    shufps $136, %xmm2, %xmm0
    movups %xmm0, 2432(%r13)
    movl $0, 2168(%r13)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$4e1:
    movl $2, %r13d
    jmp _kpdecode_cursor_next_record$817
_kpdecode_cursor_next_record$4ec:
    cmpl $620953609, %eax
    je _kpdecode_cursor_next_record$505
    cmpl $621150209, %eax
    je _kpdecode_cursor_next_record$505
    cmpl $621346817, %eax
    jne _kpdecode_cursor_next_record$77b
_kpdecode_cursor_next_record$505:
    orq $256, %rcx
    movq %rcx, (%r15)
    movl %r14d, 24(%r15)
    movl $1, 2976(%r15)
    jmp _kpdecode_cursor_next_record$77b
_kpdecode_cursor_next_record$523:
    cmpl $620822599, %eax
    jle _kpdecode_cursor_next_record$672
    cmpl $620822607, %eax
    jg _kpdecode_cursor_next_record$8ec
    cmpl $620822600, %eax
    je _kpdecode_cursor_next_record$a2d
    cmpl $620822604, %eax
    jne _kpdecode_cursor_next_record$803
    movl $2, %edx
    jmp _kpdecode_cursor_next_record$5ff
_kpdecode_cursor_next_record$559:
    cmpl $621281291, %eax
    jle _kpdecode_cursor_next_record$6ab
    cmpl $621412359, %eax
    jg _kpdecode_cursor_next_record$920
    cmpl $621281292, %eax
    je _kpdecode_cursor_next_record$a67
    cmpl $621412356, %eax
    jne _kpdecode_cursor_next_record$803
    orb $16, 1(%r13)
    movl $0, 2448(%r13)
    movq 8(%r12), %rax
    cmpq $-1, %rax
    je _kpdecode_cursor_next_record$c80
    movq %rax, 2456(%r13)
    xorl %eax, %eax
    jmp _kpdecode_cursor_next_record$c90
_kpdecode_cursor_next_record$5b2:
    movl $1, 2976(%r15)
_kpdecode_cursor_next_record$5bd:
    addq $24, %r12
    movl $2, %edx
    movq %rbx, %rdi
    movq %r12, %rsi
    movq %r15, %rcx
    callq _batch_get_bytes
    jmp _kpdecode_cursor_next_record$2e6
_kpdecode_cursor_next_record$5d9:
    cmpl $620756994, %eax
    je _kpdecode_cursor_next_record$aa1
    cmpl $620822532, %eax
    je _kpdecode_cursor_next_record$abd
    cmpl $620822556, %eax
    jne _kpdecode_cursor_next_record$803
    movl $1, %edx
_kpdecode_cursor_next_record$5ff:
    movq %r12, %rdi
    movq %r13, %rsi
    callq _add_thread_info_sched_data
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$60f:
    cmpl $620888080, %eax
    je _kpdecode_cursor_next_record$aeb
    cmpl $620888084, %eax
    je _kpdecode_cursor_next_record$b11
    cmpl $620888088, %eax
    jne _kpdecode_cursor_next_record$803
    movq 16(%r12), %rax
    leaq -1(%rax), %rcx
    cmpq $999, %rcx
    ja _kpdecode_cursor_next_record$662
    orb $-128, (%r13)
    movl 8(%r12), %ecx
    movl %ecx, 104(%r13)
    cmpq $128, %rax
    movl $128, %ecx
    cmovael %ecx, %eax
    movl %eax, 108(%r13)
_kpdecode_cursor_next_record$662:
    movl $0, 2996(%r13)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$672:
    cmpl $620822588, %eax
    je _kpdecode_cursor_next_record$b59
    cmpl $620822592, %eax
    je _kpdecode_cursor_next_record$b93
    cmpl $620822596, %eax
    jne _kpdecode_cursor_next_record$803
    orb $4, 3(%r13)
    movups 8(%r12), %xmm0
    movups %xmm0, 2952(%r13)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$6ab:
    cmpl $621150244, %eax
    je _kpdecode_cursor_next_record$b9d
    cmpl $621281284, %eax
    je _kpdecode_cursor_next_record$ba7
    cmpl $621281288, %eax
    jne _kpdecode_cursor_next_record$803
    orb $32, 2(%r13)
    movq 8(%r12), %rax
    shlq $32, %rax
    movl 16(%r12), %ecx
    orq %rax, %rcx
    movq %rcx, 2520(%r13)
    movl 24(%r12), %eax
    movl %eax, 2528(%r13)
    movl 32(%r12), %eax
    movl %eax, 2532(%r13)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$706:
    orq $8, %rcx
    movq %rcx, (%r15)
    movq 40(%r12), %rax
    movq %rax, 16(%r15)
    movl %r14d, 24(%r15)
    movq %r15, %rdi
    addq $28, %rdi
    movq %r12, %rsi
    addq $8, %rsi
    movl $20, %edx
    callq __plt_strncpy
    movb $0, 47(%r15)
    movl $1, 2976(%r15)
    movl 48(%r12), %eax
    jmp _kpdecode_cursor_next_record$77b
_kpdecode_cursor_next_record$749:
    orq $16384, %rcx
    movq %rcx, (%r15)
    movq 8(%r12), %rax
    movq %rax, 2592(%r15)
    movl 16(%r12), %eax
    movl %eax, 2600(%r15)
    movl $1, 2976(%r15)
    movl $620822548, %eax
_kpdecode_cursor_next_record$77b:
    movl %eax, %ecx
    shrl $24, %ecx
    cmpb $33, %cl
    je _kpdecode_cursor_next_record$79b
    cmpb $1, %cl
    je _kpdecode_cursor_next_record$79b
    testb %cl, %cl
    jne _kpdecode_cursor_next_record$803
    movl %eax, %ecx
    shrl $2, %ecx
    andl $4194303, %ecx
    jmp _kpdecode_cursor_next_record$7b6
_kpdecode_cursor_next_record$79b:
    movl %eax, %ecx
    andl $16711680, %ecx
    cmpl $655360, %ecx
    jne _kpdecode_cursor_next_record$803
    movl %eax, %ecx
    shrl $2, %ecx
    andl $16383, %ecx
_kpdecode_cursor_next_record$7b6:
    orb $4, 2(%r15)
    movl 52(%r12), %edx
    movl %edx, 24(%r15)
    movl %ecx, 2632(%r15)
    testb $1, %al
    jne _kpdecode_cursor_next_record$7e0
    testb $2, %al
    jne _kpdecode_cursor_next_record$7ed
    movl $2, 2636(%r15)
    jmp _kpdecode_cursor_next_record$7f8
_kpdecode_cursor_next_record$7e0:
    movl $0, 2636(%r15)
    jmp _kpdecode_cursor_next_record$7f8
_kpdecode_cursor_next_record$7ed:
    movl $1, 2636(%r15)
_kpdecode_cursor_next_record$7f8:
    movl $1, 2976(%r15)
_kpdecode_cursor_next_record$803:
    xorl %r13d, %r13d
    cmpq $0, (%r15)
    je _kpdecode_cursor_next_record$817
    movl $1, 2976(%r15)
_kpdecode_cursor_next_record$817:
    movq (%r15), %rax
    testq %rax, %rax
    je _kpdecode_cursor_next_record$873
    cmpq $2047, 1224(%rbx)
    ja _kpdecode_cursor_next_record$835
    orq $131072, %rax
    movq %rax, (%r15)
_kpdecode_cursor_next_record$835:
    incl 196(%rbx)
    movq $0, 2984(%r15)
    movq 184(%rbx), %rax
    testq %rax, %rax
    je _kpdecode_cursor_next_record$859
    movq %r15, 2984(%rax)
_kpdecode_cursor_next_record$859:
    movq %r15, 184(%rbx)
_kpdecode_cursor_next_record$860:
    cmpq $0, 176(%rbx)
    jne _kpdecode_cursor_next_record$87b
    movq %r15, 176(%rbx)
    jmp _kpdecode_cursor_next_record$87b
_kpdecode_cursor_next_record$873:
    movq %r15, %rdi
    callq __kpdecode_record_free
_kpdecode_cursor_next_record$87b:
    cmpl $2, %r13d
    je _kpdecode_cursor_next_record$d8b
    cmpl $1, %r13d
    jne _kpdecode_cursor_next_record$15
    jmp _kpdecode_cursor_next_record$d2d
_kpdecode_cursor_next_record$894:
    cmpl $620822580, %eax
    je _kpdecode_cursor_next_record$be5
    cmpl $620822584, %eax
    jne _kpdecode_cursor_next_record$803
    orb $-128, 2(%r13)
    movq 8(%r12), %rax
    shlq $32, %rax
    movl 16(%r12), %ecx
    orq %rax, %rcx
    movq %rcx, 2576(%r13)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$8cc:
    cmpl $621150232, %eax
    je _kpdecode_cursor_next_record$c30
    cmpl $621150240, %eax
    jne _kpdecode_cursor_next_record$803
    orb $2, 1(%r13)
    jmp _kpdecode_cursor_next_record$a1d
_kpdecode_cursor_next_record$8ec:
    cmpl $620822608, %eax
    je _kpdecode_cursor_next_record$c45
    cmpl $620888076, %eax
    jne _kpdecode_cursor_next_record$803
    testb $64, (%r13)
    je _kpdecode_cursor_next_record$803
    leaq 1136(%r13), %rsi
    addq $2992, %r13
    jmp _kpdecode_cursor_next_record$b01
_kpdecode_cursor_next_record$920:
    cmpl $621412360, %eax
    je _kpdecode_cursor_next_record$c5a
    cmpl $630784000, %eax
    jne _kpdecode_cursor_next_record$803
    movq 2616(%r13), %rax
    testq %rax, %rax
    je _kpdecode_cursor_next_record$ca4
    movl 2608(%r13), %r14d
    movl 3004(%r13), %ecx
    addq $3004, %r13
    leaq 8(%r12), %r10
_kpdecode_cursor_next_record$960:
    leaq 24(%r12), %r8
    leaq 16(%r12), %r9
    addq $32, %r12
    xorl %esi, %esi
_kpdecode_cursor_next_record$970:
    cmpl %r14d, %ecx
    jge _kpdecode_cursor_next_record$803
    movq %rbx, %r11
    movl %esi, %ebx
    orl %ecx, %ebx
    je _kpdecode_cursor_next_record$9ca
    movl %esi, %ebx
    andl $2147483647, %ebx
    cmpl $3, %ebx
    ja _kpdecode_cursor_next_record$9bd
    movl %esi, %edi
    andl $2147483647, %edi
    leaq _kpdecode_cursor_next_record_jmptable(%rip), %rbx
    movq %rbx, %rdx
    movslq (%rbx,%rdi,4), %rbx
    addq %rdx, %rbx
    movq %r10, %rdi
    jmpq *%rbx
_next_record_case1:
    movq %r9, %rdi
    jmp _kpdecode_cursor_next_record$9ba
_next_record_case2:
    movq %r8, %rdi
    jmp _kpdecode_cursor_next_record$9ba
_next_record_case3:
    movq %r12, %rdi
_next_record_case0:
_kpdecode_cursor_next_record$9ba:
    movq (%rdi), %rdi
_next_record_case_default:
_kpdecode_cursor_next_record$9bd:
    movslq %ecx, %rbx
    movq %rdi, (%rax,%rbx,8)
    incl %ecx
    movl %ecx, (%r13)
_kpdecode_cursor_next_record$9ca:
    incl %esi
    cmpl $4, %esi
    movq %r11, %rbx
    jb _kpdecode_cursor_next_record$970
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$9d9:
    orb $64, 2(%r13)
    movq 8(%r12), %rax
    movzbl %al, %ecx
    movq %rcx, 2552(%r13)
    movq %rax, %rcx
    shrq $8, %rcx
    movw %cx, 2568(%r13)
    shrq $24, %rax
    movb %al, 2570(%r13)
    movq 16(%r12), %rax
    movq %rax, 2560(%r13)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$a18:
    orb $4, 1(%r13)
_kpdecode_cursor_next_record$a1d:
    movq %r12, %rdi
    movq %r13, %rsi
    callq _add_pmc_data
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$a2d:
    orb $4, 3(%r13)
    movq 8(%r12), %rax
    shlq $32, %rax
    movl 16(%r12), %ecx
    orq %rax, %rcx
    movq %rcx, 2952(%r13)
    movq 24(%r12), %rax
    shlq $32, %rax
    movl 32(%r12), %ecx
    orq %rax, %rcx
    movq %rcx, 2960(%r13)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$a67:
    orb $32, 2(%r13)
    movq 8(%r12), %rax
    shlq $32, %rax
    movl 16(%r12), %ecx
    orq %rax, %rcx
    movq %rcx, 2536(%r13)
    movq 24(%r12), %rax
    shlq $32, %rax
    movl 32(%r12), %ecx
    orq %rax, %rcx
    movq %rcx, 2544(%r13)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$aa1:
    movl $1, 2976(%r13)
    movq $0, 200(%rbx,%r14,8)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$abd:
    orb $32, (%r13)
    movl 8(%r12), %eax
    movl %eax, 88(%r13)
    movq 16(%r12), %rax
    movq %rax, 16(%r13)
    movq 24(%r12), %rax
    movq %rax, 96(%r13)
    movl 32(%r12), %eax
    movl %eax, 92(%r13)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$aeb:
    cmpb $0, (%r13)
    jns _kpdecode_cursor_next_record$803
    leaq 104(%r13), %rsi
    addq $2996, %r13
_kpdecode_cursor_next_record$b01:
    movq %r12, %rdi
    movq %r13, %rdx
    callq _add_stack_data
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$b11:
    movq 16(%r12), %rax
    leaq -1(%rax), %rcx
    cmpq $999, %rcx
    ja _kpdecode_cursor_next_record$b49
    orb $64, (%r13)
    movl 8(%r12), %ecx
    movl %ecx, 1136(%r13)
    cmpq $128, %rax
    movl $128, %ecx
    cmovael %ecx, %eax
    movl %eax, 1140(%r13)
_kpdecode_cursor_next_record$b49:
    movl $0, 2992(%r13)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$b59:
    orb $16, 2(%r13)
    movq 8(%r12), %rax
    shlq $32, %rax
    movl 16(%r12), %ecx
    orq %rax, %rcx
    movq %rcx, 2488(%r13)
    movq 24(%r12), %rax
    shlq $32, %rax
    movl 32(%r12), %ecx
    orq %rax, %rcx
    movq %rcx, 2496(%r13)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$b93:
    movl $1, %edx
    jmp _kpdecode_cursor_next_record$c4a
_kpdecode_cursor_next_record$b9d:
    orb $2, 1(%r13)
    jmp _kpdecode_cursor_next_record$c35
_kpdecode_cursor_next_record$ba7:
    orb $32, 2(%r13)
    movq 8(%r12), %rax
    movq %rax, 2520(%r13)
    movq 16(%r12), %rax
    movq %rax, %rcx
    shrq $32, %rcx
    movl %ecx, 2528(%r13)
    movl %eax, 2532(%r13)
    movups 24(%r12), %xmm0
    movups %xmm0, 2536(%r13)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$be5:
    orb $64, 2(%r13)
    movq 8(%r12), %rax
    movzbl %al, %ecx
    movq %rcx, 2552(%r13)
    movq %rax, %rcx
    shrq $8, %rcx
    movw %cx, 2568(%r13)
    shrq $24, %rax
    movb %al, 2570(%r13)
    movq 16(%r12), %rax
    shlq $32, %rax
    movl 24(%r12), %ecx
    orq %rax, %rcx
    movq %rcx, 2560(%r13)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$c30:
    orb $4, 1(%r13)
_kpdecode_cursor_next_record$c35:
    movq %r12, %rdi
    movq %r13, %rsi
    callq _add_pmc_data32
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$c45:
    movl $2, %edx
_kpdecode_cursor_next_record$c4a:
    movq %r12, %rdi
    movq %r13, %rsi
    callq _add_thread_info_sched_data2_32
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$c5a:
    orb $2, 3(%r13)
    movups 8(%r12), %xmm0
    movups %xmm0, 2920(%r13)
    movups 24(%r12), %xmm0
    movups %xmm0, 2936(%r13)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$c80:
    movl $1, 2448(%r13)
    movl $1, %eax
_kpdecode_cursor_next_record$c90:
    movq 16(%r12), %rcx
    cmpq $-1, %rcx
    je _kpdecode_cursor_next_record$cfd
    movq %rcx, 2464(%r13)
    jmp _kpdecode_cursor_next_record$d07
_kpdecode_cursor_next_record$ca4:
    cmpl $-1, 3004(%r13)
    jne _kpdecode_cursor_next_record$803
    movq 8(%r12), %r14
    movl %r14d, 2608(%r13)
    movslq %r14d, %rdi
    shlq $3, %rdi
    callq __plt_malloc
    movq %rax, 2616(%r13)
    movl $0, 3004(%r13)
    testq %rax, %rax
    je _kpdecode_cursor_next_record$803
    orb $-128, 1(%r13)
    leaq 8(%r12), %r10
    addq $3004, %r13
    xorl %ecx, %ecx
    jmp _kpdecode_cursor_next_record$960
_kpdecode_cursor_next_record$cfd:
    orl $2, %eax
    movl %eax, 2448(%r13)
_kpdecode_cursor_next_record$d07:
    movq 24(%r12), %rcx
    cmpq $-1, %rcx
    je _kpdecode_cursor_next_record$d1e
    movq %rcx, 2472(%r13)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$d1e:
    orl $4, %eax
    movl %eax, 2448(%r13)
    jmp _kpdecode_cursor_next_record$803
_kpdecode_cursor_next_record$d2d:
    movq %rbx, %rdi
    callq _record_ready
    movl $1, %r13d
    testb %al, %al
    je _kpdecode_cursor_next_record$d8b
    movq 176(%rbx), %rax
    movq -48(%rbp), %rcx
    movq %rax, (%rcx)
    decl 196(%rbx)
    movq 2984(%rax), %rcx
    movq %rcx, 176(%rbx)
    cmpq %rax, 184(%rbx)
    jne _kpdecode_cursor_next_record$d75
    movq $0, 184(%rbx)
_kpdecode_cursor_next_record$d75:
    movq $0, 2984(%rax)
    xorl %r13d, %r13d
    jmp _kpdecode_cursor_next_record$d8b
_kpdecode_cursor_next_record$d85:
    movl $2, %r13d
_kpdecode_cursor_next_record$d8b:
    movl %r13d, %eax
    addq $8, %rsp
    popq %rbx
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    popq %rbp
    retq
    nop
_kpdecode_cursor_next_record_jmptable:
    .long _next_record_case0 - _kpdecode_cursor_next_record_jmptable
    .long _next_record_case1 - _kpdecode_cursor_next_record_jmptable
    .long _next_record_case2 - _kpdecode_cursor_next_record_jmptable
    .long _next_record_case3 - _kpdecode_cursor_next_record_jmptable
    .byte 0x55
    .byte 0x1C, 0xFC, 0xFF, 0xFF, 0x0F, 0xFC, 0xFF, 0xFF, 0x14, 0xFC, 0xFF, 0xFF, 0x19, 0xFC, 0xFF, 0xFF    

_batch_get_bytes:
    pushq %rbp
    movq %rsp, %rbp
    pushq %r14
    pushq %rbx
    movl (%rdi), %eax
    xorl %edi, %edi
    cmpl $2, %eax
    sete %dil
    shlq $3, %rdi
    cmpl $1, %eax
    movl $4, %r11d
    cmovneq %rdi, %r11
    testq %rdx, %rdx
    je _batch_get_bytes$a7
    leaq 2640(%rcx), %r8
    xorl %r9d, %r9d
_batch_get_bytes$32:
    movq (%rsi,%r9,8), %rax
    movq %rax, -24(%rbp)
    movq 2896(%rcx), %r10
    testq %r11, %r11
    je _batch_get_bytes$77
    leaq (%r8,%r10), %r14
    leaq 1(%r10), %rbx
    xorl %edi, %edi
_batch_get_bytes$50:
    leaq (%rbx,%rdi), %rax
    cmpq $256, %rax
    jae _batch_get_bytes$8b
    movb -24(%rbp,%rdi), %al
    movb %al, (%r14,%rdi)
    testb %al, %al
    je _batch_get_bytes$a0
    incq %rdi
    cmpq %r11, %rdi
    jb _batch_get_bytes$50
    movq 2896(%rcx), %r10
_batch_get_bytes$77:
    addq %r11, %r10
    movq %r10, 2896(%rcx)
    incq %r9
    cmpq %rdx, %r9
    jb _batch_get_bytes$32
    jmp _batch_get_bytes$a7
_batch_get_bytes$8b:
    addq %rdi, 2896(%rcx)
    addq %rcx, %r10
    movb $0, 2640(%rdi,%r10)
    jmp _batch_get_bytes$a7
_batch_get_bytes$a0:
    addq %rdi, 2896(%rcx)
_batch_get_bytes$a7:
    popq %rbx
    popq %r14
    popq %rbp
    retq

_record_ready:
    pushq %rbp
    movq %rsp, %rbp
    cmpl $0, 64(%rdi)
    je _record_ready$60
    movq 176(%rdi), %rcx
    testq %rcx, %rcx
    je _record_ready$21
    movb $1, %al
    cmpl $0, 2976(%rcx)
    jne _record_ready$62
_record_ready$21:
    cmpl $10001, 196(%rdi)
    jl _record_ready$60
    orb $-128, 7(%rcx)
    movl $1, 2976(%rcx)
    movslq 24(%rcx), %rax
    xorl %ecx, %ecx
    movq %rcx, 200(%rdi,%rax,8)
    movq 176(%rdi), %rax
    movslq 24(%rax), %rax
    movq %rcx, 456(%rdi,%rax,8)
    movb $1, %al
    jmp _record_ready$62
_record_ready$60:
    xorl %eax, %eax
_record_ready$62:
    popq %rbp
    retq

_add_stack_data:
    pushq %rbp
    movq %rsp, %rbp
    movslq (%rdx), %rax
    cmpq $124, %rax
    jg _add_stack_data$36
    movq 8(%rdi), %rcx
    movq %rcx, 8(%rsi,%rax,8)
    movq 16(%rdi), %rcx
    movq %rcx, 16(%rsi,%rax,8)
    movq 24(%rdi), %rcx
    movq %rcx, 24(%rsi,%rax,8)
    movq 32(%rdi), %rcx
    movq %rcx, 32(%rsi,%rax,8)
    leal 4(%rax), %eax
    movl %eax, (%rdx)
_add_stack_data$36:
    popq %rbp
    retq

_add_pmc_data:
    pushq %rbp
    movq %rsp, %rbp
    cmpl $0, 2436(%rsi)
    je _add_pmc_data$49
    movq 8(%rdi), %rax
    movslq 3000(%rsi), %rcx
    movq %rax, 2176(%rsi,%rcx,8)
    movq 16(%rdi), %rax
    movq %rax, 2184(%rsi,%rcx,8)
    movq 24(%rdi), %rax
    movq %rax, 2192(%rsi,%rcx,8)
    movq 32(%rdi), %rax
    movq %rax, 2200(%rsi,%rcx,8)
    leal 4(%rcx), %eax
    jmp _add_pmc_data$88
_add_pmc_data$49:
    movslq 3000(%rsi), %rax
    movq 8(%rdi), %rcx
    testq %rax, %rax
    je _add_pmc_data$a1
    movq %rcx, 2176(%rsi,%rax,8)
    movq 16(%rdi), %rcx
    movq %rcx, 2184(%rsi,%rax,8)
    movq 24(%rdi), %rcx
    movq %rcx, 2192(%rsi,%rax,8)
    movq 32(%rdi), %rcx
    movq %rcx, 2200(%rsi,%rax,8)
    leal 4(%rax), %eax
_add_pmc_data$88:
    movl %eax, 3000(%rsi)
_add_pmc_data$8e:
    movl 2440(%rsi), %ecx
    cmpl %ecx, %eax
    cmovbl %eax, %ecx
    movl %ecx, 2168(%rsi)
    popq %rbp
    retq
_add_pmc_data$a1:
    movl %ecx, 2168(%rsi)
    movq 16(%rdi), %rax
    movq %rax, 2176(%rsi)
    movq 24(%rdi), %rax
    movq %rax, 2184(%rsi)
    movq 32(%rdi), %rax
    movq %rax, 2192(%rsi)
    movl $3, 3000(%rsi)
    movl $3, %eax
    jmp _add_pmc_data$8e

_add_thread_info_sched_data:
    pushq %rbp
    movq %rsp, %rbp
    movq (%rsi), %r8
    movq %r8, %rcx
    orq $1048576, %rcx
    movq %rcx, (%rsi)
    movups 8(%rdi), %xmm0
    movups %xmm0, 2488(%rsi)
    movq 24(%rdi), %rax
    movq %rax, %rcx
    shrq $48, %rcx
    movw %cx, 2508(%rsi)
    movq %rax, %rcx
    shrq $32, %rcx
    movw %cx, 2510(%rsi)
    movl %eax, %ecx
    shrl $24, %ecx
    movl %ecx, 2504(%rsi)
    shrl $6, %eax
    andl $7, %eax
    movl 2512(%rsi), %ecx
    andl $-8, %ecx
    orl %eax, %ecx
    movw %cx, 2512(%rsi)
    movzwl 24(%rdi), %eax
    andl $56, %eax
    andl $-57, %ecx
    orl %eax, %ecx
    movw %cx, 2512(%rsi)
    movl 24(%rdi), %eax
    andl $7, %eax
    shll $6, %eax
    andl $4294966847, %ecx
    orl %eax, %ecx
    movw %cx, 2512(%rsi)
    movq 32(%rdi), %rax
    shrq $52, %rax
    andl $3584, %eax
    andl $4294963711, %ecx
    orl %eax, %ecx
    movw %cx, 2512(%rsi)
    cmpl $2, %edx
    jl _add_thread_info_sched_data$108
    orq $135266304, %r8
    movq %r8, (%rsi)
    movq 32(%rdi), %rax
    shrq $58, %rax
    andl $7, %eax
    movl 2968(%rsi), %ecx
    andl $-8, %ecx
    orl %eax, %ecx
    movw %cx, 2968(%rsi)
    movq 32(%rdi), %rax
    shrq $52, %rax
    andl $56, %eax
    andl $-57, %ecx
    orl %eax, %ecx
    movw %cx, 2968(%rsi)
    movq 32(%rdi), %rax
    shrq $46, %rax
    andl $448, %eax
    andl $4294966847, %ecx
    orl %eax, %ecx
    movw %cx, 2968(%rsi)
_add_thread_info_sched_data$108:
    popq %rbp
    retq

_add_thread_info_sched_data2_32:
    pushq %rbp
    movq %rsp, %rbp
    movq (%rsi), %r8
    movq %r8, %rcx
    orq $1048576, %rcx
    movq %rcx, (%rsi)
    movq 8(%rdi), %rax
    movq %rax, %rcx
    shrq $16, %rcx
    movw %cx, 2508(%rsi)
    movw %ax, 2510(%rsi)
    movl 16(%rdi), %eax
    movl %eax, %ecx
    shrl $24, %ecx
    movl %ecx, 2504(%rsi)
    shrl $6, %eax
    andl $7, %eax
    movl 2512(%rsi), %ecx
    andl $-8, %ecx
    orl %eax, %ecx
    movw %cx, 2512(%rsi)
    movzwl 16(%rdi), %eax
    andl $56, %eax
    andl $-57, %ecx
    orl %eax, %ecx
    movw %cx, 2512(%rsi)
    movl 16(%rdi), %eax
    andl $7, %eax
    shll $6, %eax
    andl $4294966847, %ecx
    orl %eax, %ecx
    movw %cx, 2512(%rsi)
    movq 24(%rdi), %rax
    shrl $20, %eax
    andl $3584, %eax
    andl $4294963711, %ecx
    orl %eax, %ecx
    movw %cx, 2512(%rsi)
    cmpl $2, %edx
    jl _add_thread_info_sched_data2_32$f5
    orq $135266304, %r8
    movq %r8, (%rsi)
    movq 24(%rdi), %rax
    shrl $26, %eax
    andl $7, %eax
    movl 2968(%rsi), %ecx
    andl $-8, %ecx
    orl %eax, %ecx
    movw %cx, 2968(%rsi)
    movq 24(%rdi), %rax
    shrl $20, %eax
    andl $56, %eax
    andl $-57, %ecx
    orl %eax, %ecx
    movw %cx, 2968(%rsi)
    movq 24(%rdi), %rax
    shrl $14, %eax
    andl $448, %eax
    andl $4294966847, %ecx
    orl %eax, %ecx
    movw %cx, 2968(%rsi)
_add_thread_info_sched_data2_32$f5:
    popq %rbp
    retq


_add_pmc_data32:
    pushq %rbp
    movq %rsp, %rbp
    movq 8(%rdi), %rax
    shlq $32, %rax
    movl 16(%rdi), %ecx
    orq %rax, %rcx
    movq 24(%rdi), %rax
    shlq $32, %rax
    movl 32(%rdi), %edx
    orq %rax, %rdx
    movslq 3000(%rsi), %rax
    movq %rcx, 2176(%rsi,%rax,8)
    movq %rdx, 2184(%rsi,%rax,8)
    leal 2(%rax), %eax
    movl %eax, 3000(%rsi)
    movl 2440(%rsi), %ecx
    cmpl %ecx, %eax
    jae _add_pmc_data32$52
    movl %eax, 2168(%rsi)
    jmp _add_pmc_data32$62
_add_pmc_data32$52:
    movl %ecx, 2168(%rsi)
    movl $0, 3000(%rsi)
_add_pmc_data32$62:
    popq %rbp
    retq

_get_rbp:
    movq %rbp, %rax
    retq


