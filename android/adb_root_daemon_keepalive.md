adb root 后台进程保活
----

通常关闭adb调试后，从adb启动的后台进程都会被杀死。这是因为这些进程都归属与 adbd 创建的 cgroup，adbd没了，cgroup下所有进程就没了。

只要把进程从cgroup中移除，即可在adbd退出后保活。

这里启动了一个 sleep 进程：

```
.../root/renderdoc # ps -ef | grep sleep
root     26497 25935  0 05:44 pts/3    00:00:00 sleep 1000000
root     27925 18513  2 06:01 pts/0    00:00:00 grep sleep
```

查看其 cgroup
```
.../root/renderdoc # cat /proc/26497/cgroup
7:freezer:/
6:schedtune:/
5:cpuset:/
4:cpuacct:/uid_0/pid_23323
3:cpu:/
2:blkio:/
1:memory:/
0::/
```

注意到 `4:cpuacct:/uid_0/pid_23323`

```
.../root/renderdoc # ps -ef | grep 23323
shell    23323     1  0 05:29 ?        00:00:01 /system/bin/adbd --root_seclabel=u:r:su:s0
shell    25922 23323  0 05:43 pts/1    00:00:00 -/system/bin/sh
root     25929 23323  0 05:43 pts/3    00:00:00 /system/bin/sh /data/local/tools/adebimpl.sh
root     27863 18513  1 05:57 pts/0    00:00:00 grep 23323
```

正是 adbd。

当前环境的 cgroup 还没有 mount，需要先 mount
```
root@localhost:~# mount -t tmpfs cgroup_root /sys/fs/cgroup
root@localhost:~# ls /sys/fs/cgroup
root@localhost:~# mkdir /sys/fs/cgroup/cpuacct
root@localhost:~# mount -t cgroup cpuacct -o cpuacct /sys/fs/cgroup/cpuacct/
root@localhost:~# ls /sys/fs/cgroup/cpuacct/
cgroup.clone_children      uid_10063/                 uid_10103/                 uid_10159/                 uid_10191/                 uid_10225/                 uid_1058/
cgroup.procs               uid_10064/                 uid_10105/                 uid_10160/                 uid_10192/                 uid_10237/                 uid_1066/
cgroup.sane_behavior       uid_10065/                 uid_10107/                 uid_10166/                 uid_10193/                 uid_10244/                 uid_1067/
cpuacct.stat               uid_10066/                 uid_10108/                 uid_1017/                  uid_10194/                 uid_10245/                 uid_1068/
cpuacct.usage              uid_10068/                 uid_10110/                 uid_10170/                 uid_10195/                 uid_10246/                 uid_1069/
cpuacct.usage_all          uid_10072/                 uid_10111/                 uid_10171/                 uid_10196/                 uid_1027/                  uid_1072/
cpuacct.usage_percpu       uid_10074/                 uid_10113/                 uid_10172/                 uid_10199/                 uid_10283/                 uid_2000/
cpuacct.usage_percpu_sys   uid_10075/                 uid_10115/                 uid_10173/                 uid_1020/                  uid_10306/                 uid_2906/
cpuacct.usage_percpu_user  uid_10078/                 uid_10116/                 uid_10174/                 uid_10200/                 uid_10323/                 uid_9802/
cpuacct.usage_sys          uid_10083/                 uid_10121/                 uid_10175/                 uid_10201/                 uid_10329/                 uid_9810/
cpuacct.usage_user         uid_10085/                 uid_10122/                 uid_10177/                 uid_10204/                 uid_10345/                 uid_99000/
notify_on_release          uid_10088/                 uid_10124/                 uid_10178/                 uid_10205/                 uid_10346/                 uid_99001/
release_agent              uid_10089/                 uid_10125/                 uid_10179/                 uid_10207/                 uid_10347/                 uid_99003/
tasks                      uid_10090/                 uid_10129/                 uid_10180/                 uid_10209/                 uid_10348/                 uid_99004/
uid/                       uid_10091/                 uid_1013/                  uid_10181/                 uid_1021/                  uid_10349/                 uid_99005/
uid_0/                     uid_10094/                 uid_10135/                 uid_10184/                 uid_10211/                 uid_10350/                 uid_99006/
uid_1000/                  uid_10095/                 uid_10137/                 uid_10185/                 uid_10212/                 uid_1036/                  uid_99007/
```


现在把它的cgroup 切到根目录，这里用到的工具可以用 `apt install cgroup-tools` 安装
```
root@localhost:~# cgclassify -g cpuacct:/ 26497
```

现在再看看，已经修改成功了
```
root@localhost:~# cat /proc/26497/cgroup
7:freezer:/
6:schedtune:/
5:cpuset:/
4:cpuacct:/
3:cpu:/
2:blkio:/
1:memory:/
0::/
```

没有 cgroup-tools 直接 `echo $PID > /sys/fs/cgroup/cpuacct/uid_0/tasks` 也是可以的

关闭 adb 调试
```
root@localhost:~# /system/bin/cmd settings put global adb_enabled 0
root@localhost:~#
C:\Users\X>
```

连接直接断掉，说明已经关闭了，切到另一个shell， 看看sleep 是否存活
```
.../root/renderdoc # ps -ef | grep sleep
root     26497     1  0 05:44 ?        00:00:00 sleep 1000000
root     28074 18513  2 06:06 pts/0    00:00:00 grep sleep
```

还活着，挺好
