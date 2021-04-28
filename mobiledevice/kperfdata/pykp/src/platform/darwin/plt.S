__plt_malloc:
  jmp _malloc

__plt_free:
  jmp _free

__plt_calloc:
  jmp _calloc

__plt_strncpy:
  jmp _strncpy
 
_kpdecode_cursor_setchunk:
  jmp __kpdecode_cursor_setchunk
 
_kpdecode_cursor_clearchunk:
  jmp __kpdecode_cursor_clearchunk
 
_kpdecode_cursor_free:
  jmp __kpdecode_cursor_free
 
_kpdecode_cursor_set_option:
  jmp __kpdecode_cursor_set_option

_kpdecode_record_free:
  jmp __kpdecode_record_free
 
_kpdecode_cursor_next_record:
  jmp __kpdecode_cursor_next_record

