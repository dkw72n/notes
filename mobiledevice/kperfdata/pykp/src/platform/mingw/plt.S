__plt_malloc:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rcx
  movq %rdi, %rcx
  subq $32, %rsp
  callq malloc
  addq $32, %rsp
  popq %rcx
  popq %rbp
  retq

__plt_free:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rcx
  movq %rdi, %rcx
  subq $32, %rsp
  callq free
  addq $32, %rsp
  popq %rcx
  popq %rbp
  retq

__plt_calloc:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rcx
  pushq %rdx
  movq %rdi, %rcx
  movq %rsi, %rdx
  subq $32, %rsp
  callq calloc
  addq $32, %rsp
  popq %rdx
  popq %rcx
  popq %rbp
  retq

__plt_strncpy:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rcx
  pushq %rdx
  pushq %r8
  movq %rdx, %r8
  movq %rdi, %rcx
  movq %rsi, %rdx
  subq $32, %rsp
  callq strncpy
  addq $32, %rsp
  popq %r8
  popq %rdx
  popq %rcx
  popq %rbp
  retq
  
_kpdecode_cursor_setchunk:
  pushq %rbp
  movq  %rsp, %rbp
  pushq %rdi
  pushq %rsi
  pushq %rdx
  movq %rcx, %rdi
  movq %rdx, %rsi
  movq %r8, %rdx
  callq __kpdecode_cursor_setchunk
  pop %rdx
  pop %rsi
  pop %rdi
  popq  %rbp
  retq
  
_kpdecode_cursor_clearchunk:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rdi
  movq %rcx, %rdi
  callq __kpdecode_cursor_clearchunk
  popq %rdi
  popq %rbp
  retq
  
_kpdecode_cursor_free:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rdi
  movq %rcx, %rdi
  callq __kpdecode_cursor_free
  popq %rdi
  popq %rbp
  retq
  
_kpdecode_cursor_set_option:
  pushq %rbp
  movq %rsp, %rbp

  pushq %rdi
  pushq %rsi
  pushq %rdx
  movq %rcx, %rdi
  movq %rdx, %rsi
  movq %r8, %rdx
  callq __kpdecode_cursor_set_option
  popq %rdx
  popq %rsi
  popq %rdi

  popq %rbp
  retq

_kpdecode_record_free:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rdi
  movq %rcx, %rdi
  callq __kpdecode_record_free
  popq %rdi
  popq %rbp
  retq
  
_kpdecode_cursor_next_record:
  pushq %rbp
  movq  %rsp, %rbp
  pushq %rdi
  pushq %rsi
  movq %rcx, %rdi
  movq %rdx, %rsi
  callq __kpdecode_cursor_next_record
  popq %rsi
  popq %rdi
  popq %rbp
  retq