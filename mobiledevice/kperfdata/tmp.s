_add_pmc_data32:
    6d75:       pushq   %rbp
    6d76:       movq    %rsp, %rbp
    6d79:       movq    8(%rdi), %rax
    6d7d:       shlq    $32, %rax
    6d81:       movl    16(%rdi), %ecx
    6d84:       orq     %rax, %rcx
    6d87:       movq    24(%rdi), %rax
    6d8b:       shlq    $32, %rax
    6d8f:       movl    32(%rdi), %edx
    6d92:       orq     %rax, %rdx
    6d95:       movslq  3000(%rsi), %rax
    6d9c:       movq    %rcx, 2176(%rsi,%rax,8)
    6da4:       movq    %rdx, 2184(%rsi,%rax,8)
    6dac:       leal    2(%rax), %eax
    6daf:       movl    %eax, 3000(%rsi)
    6db5:       movl    2440(%rsi), %ecx
    6dbb:       cmpl    %ecx, %eax
    6dbd:       jae     8 <_add_pmc_data32+0x52>
    6dbf:       movl    %eax, 2168(%rsi)
    6dc5:       jmp     16 <_add_pmc_data32+0x62>
    6dc7:       movl    %ecx, 2168(%rsi)
    6dcd:       movl    $0, 3000(%rsi)
    6dd7:       popq    %rbp
    6dd8:       retq

