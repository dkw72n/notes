cdef extern from "kperfdata.h":
    cdef struct _TyF6:
        int eventCode
        long long arg0
        long long arg1
        long long arg2
        long long arg3
    
    cdef struct _TyF7:
        int pid
        int threadState
        long long dispatchQ
    
    cdef struct _TyF17:
        int actionId
        int triggerId

    cdef struct kpdecode_record:
        unsigned long long config
        long long time
        long long tid
        int cpu
        _TyF6 _field6
        _TyF7 _field7
        _TyF17 _field17
        pass
    int kpdecode_cursor_next_record(void* cursor, kpdecode_record** p_record);
    int kpdecode_cursor_setchunk(void*, void*, size_t);
    int kpdecode_cursor_set_option(void* cursor, int, long long);
    void kpdecode_cursor_clearchunk(void*);
    void* kpdecode_cursor_create();
    void kpdecode_cursor_free(void*);
    void kpdecode_record_free(void*);

cdef class KPDecoder:
    cdef void* _cursor

    def __cinit__(self):
        self._cursor = kpdecode_cursor_create()
        if self._cursor is not NULL:
            kpdecode_cursor_set_option(self._cursor, 0, 1)

    def __dealloc__(self):
        if self._cursor is not NULL:
            kpdecode_cursor_free(self._cursor)

    def decode(self, data):
        ret = []
        cdef kpdecode_record* record
        length = len(data)
        cdef char* buf = data;
        kpdecode_cursor_setchunk(self._cursor, <void*>buf, length)
        while kpdecode_cursor_next_record(self._cursor, &record) == 0:
            #ret.append((record._field6.eventCode, record.time, record._field6.arg1, record._field6.arg2, record._field6.arg3))
            info = {
                "time": record.time,
                "code": record._field6.eventCode,
                "arg0": record._field6.arg0,
                "arg1": record._field6.arg1,
                "arg2": record._field6.arg2,
                "arg3": record._field6.arg3,
                }
            if not (record.config & 1):
                info['time'] = 0
            if record.config & 2:
                info['cpu'] = record.cpu
            if record.config & 4:
                info['tid'] = record.tid
            if record.config & 0x20:
                info.update({
                    "pid": record._field7.pid,
                    "threadState": record._field7.threadState,
                    "dispatchQ": record._field7.dispatchQ
                })
            if record.config & 0xd:
                info.update({
                    'actionId': record._field17.actionId,
                    'triggerId': record._field17.triggerId
                    })
            ret.append(info)
            kpdecode_record_free(record)
        kpdecode_cursor_clearchunk(self._cursor)
        return ret
        pass

