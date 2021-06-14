#ifndef _KPERFDATA_H_0227_1355_
#define _KPERFDATA_H_0227_1355_
typedef struct {
    unsigned long long _field1;
} CDStruct_69d7cc99;

typedef struct {
    unsigned long long _field1;
    unsigned long long _field2;
    unsigned long long _field3;
    unsigned long long _field4;
} CDStruct_33dcf794;

struct kpdecode_callstack {
    unsigned int _field1;
    unsigned int _field2;
    unsigned long long _field3[128];
};

struct kpdecode_pmc {
    int _field1;
    unsigned long long _field2[32];
};

struct kpdecode_record {
    unsigned long long config;
    unsigned long long time;
    unsigned long long tid;
    int cpu;
    struct {
        char _field1[20];
    } _field5;
    struct {
        unsigned int eventCode;
        unsigned long long arg0;
        unsigned long long arg1;
        unsigned long long arg2;
        unsigned long long arg3;
    } _field6;
    struct {
        int pid;
        int threadState;
        uint64_t dispatchQ;
    } _field7;
    struct kpdecode_callstack _field8;
    struct kpdecode_callstack _field9;
    struct kpdecode_pmc _field10;
    struct {
        unsigned int _field1;
        unsigned int _field2;
        unsigned int _field3;
        unsigned int _field4;
    } _field11;
    struct {
        unsigned int _field1;
        unsigned long long _field2;
        unsigned long long _field3;
        unsigned long long _field4;
        unsigned long long _field5;
    } _field12;
    struct {
        unsigned long long _field1;
        unsigned long long _field2;
        unsigned int _field3;
        short _field4;
        short _field5;
        unsigned int :3;
        unsigned int :3;
        unsigned int :3;
        unsigned int :3;
    } _field13;
    struct {
        unsigned long long _field1;
        int _field2;
        int _field3;
        unsigned long long _field4;
        unsigned long long _field5;
    } _field14;
    struct {
        unsigned long long _field1;
        unsigned long long _field2;
        short _field3;
        unsigned char _field4;
    } _field15;
    CDStruct_69d7cc99 _field16;
    struct {
        unsigned int actionId;
        unsigned int triggerId;
    } _field17;
    struct {
        unsigned long long _field1;
        int _field2;
    } _field18;
    struct {
        int _field1;
        unsigned long long *_field2;
    } _field19;
    CDStruct_69d7cc99 _field20;
    struct {
        unsigned int _field1;
        int _field2;
    } _field21;
    struct {
        char _field1[256];
        unsigned long long _field2;
        unsigned long long _field3;
        unsigned int _field4;
    } _field22;
    CDStruct_33dcf794 _field23;
    struct {
        unsigned long long _field1;
        unsigned long long _field2;
    } _field24;
    struct {
        unsigned int :3;
        unsigned int :3;
        unsigned int :3;
    } _field25;
};

extern int kpdecode_cursor_next_record(void* cursor, struct kpdecode_record** p_record) asm ("_kpdecode_cursor_next_record");
extern int kpdecode_cursor_setchunk(void*, void*, size_t) asm ("_kpdecode_cursor_setchunk");
extern int kpdecode_cursor_set_option(void* cursor, int, uint64_t) asm ("_kpdecode_cursor_set_option");
extern void kpdecode_cursor_clearchunk(void*) asm ("_kpdecode_cursor_clearchunk");
extern void* kpdecode_cursor_create() asm ("_kpdecode_cursor_create");
extern void kpdecode_cursor_free(void*) asm ("_kpdecode_cursor_free");
extern void kpdecode_record_free(void*) asm ("_kpdecode_record_free");
#endif
