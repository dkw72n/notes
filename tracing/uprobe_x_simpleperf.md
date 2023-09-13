uprobe x simpleperf
====


添加 uprobe 事件
```
echo 'p /system/lib64/libGLESv2.so:0x13918' > /sys/kernel/tracing/uprobe_events
```

在 simpleperf 中枚举 uprobe 事件
```
# simpleperf list | grep uprobes
uprobes:p_libGLESv2_0x13918
```

采集事件
```
# simpleperf record -e "uprobes:p_libGLESv2_0x13918" -g -a
^C
simpleperf W dso.cpp:384] /data/app/com.xxx.yyy-YiShvcUIZfanB3Sesd84QA==/lib/arm64/libUE4.so doesn't contain symbol table
simpleperf I cmd_record.cpp:635] Samples recorded: 172. Samples lost: 0.
```

查看命中
```
# simpleperf dump | grep callchain -A20
...
--
  callchain:
    glShaderSource (/system/lib64/libGLESv2.so[+13918])
    libUE4.so[+6549ff8] (/data/app/com.xxx.yyy-YiShvcUIZfanB3Sesd84QA==/lib/arm64/libUE4.so[+6549ff8])
    libUE4.so[+6544bd0] (/data/app/com.xxx.yyy-YiShvcUIZfanB3Sesd84QA==/lib/arm64/libUE4.so[+6544bd0])
    libUE4.so[+65468e4] (/data/app/com.xxx.yyy-YiShvcUIZfanB3Sesd84QA==/lib/arm64/libUE4.so[+65468e4])
    libUE4.so[+6575460] (/data/app/com.xxx.yyy-YiShvcUIZfanB3Sesd84QA==/lib/arm64/libUE4.so[+6575460])
    libUE4.so[+65743c8] (/data/app/com.xxx.yyy-YiShvcUIZfanB3Sesd84QA==/lib/arm64/libUE4.so[+65743c8])
    libUE4.so[+33c3d4c] (/data/app/com.xxx.yyy-YiShvcUIZfanB3Sesd84QA==/lib/arm64/libUE4.so[+33c3d4c])
    libUE4.so[+33bcb64] (/data/app/com.xxx.yyy-YiShvcUIZfanB3Sesd84QA==/lib/arm64/libUE4.so[+33bcb64])
    libUE4.so[+6af96d0] (/data/app/com.xxx.yyy-YiShvcUIZfanB3Sesd84QA==/lib/arm64/libUE4.so[+6af96d0])
    libUE4.so[+6bab1a4] (/data/app/com.xxx.yyy-YiShvcUIZfanB3Sesd84QA==/lib/arm64/libUE4.so[+6bab1a4])
    libUE4.so[+6bae80c] (/data/app/com.xxx.yyy-YiShvcUIZfanB3Sesd84QA==/lib/arm64/libUE4.so[+6bae80c])
    libUE4.so[+5ffe574] (/data/app/com.xxx.yyy-YiShvcUIZfanB3Sesd84QA==/lib/arm64/libUE4.so[+5ffe574])
    libUE4.so[+5ffe1c0] (/data/app/com.xxx.yyy-YiShvcUIZfanB3Sesd84QA==/lib/arm64/libUE4.so[+5ffe1c0])
    libUE4.so[+648cd00] (/data/app/com.xxx.yyy-YiShvcUIZfanB3Sesd84QA==/lib/arm64/libUE4.so[+648cd00])
    libUE4.so[+6033694] (/data/app/com.xxx.yyy-YiShvcUIZfanB3Sesd84QA==/lib/arm64/libUE4.so[+6033694])
    libUE4.so[+5ffc790] (/data/app/com.xxx.yyy-YiShvcUIZfanB3Sesd84QA==/lib/arm64/libUE4.so[+5ffc790])
    __pthread_start(void*) (/apex/com.android.runtime/lib64/bionic/libc.so[+d6cb0])
    __start_thread (/apex/com.android.runtime/lib64/bionic/libc.so[+74eac])
```

删除 uprobe 事件
```
echo '-:p_libGLESv2_0x13918' >> /d/tracing/uprobe_events
```

