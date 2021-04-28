__plt_malloc:
  jmp malloc@plt

__plt_free:
  jmp free@plt

__plt_calloc:
  jmp calloc@plt

__plt_strncpy:
  jmp strncpy@plt
 
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

