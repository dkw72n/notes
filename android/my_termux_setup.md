## /data/data/com.termux/files/home
```
~ $ pwd
/data/data/com.termux/files/home
~ $ cat .bash_init
#!/system/bin/sh
export PREFIX='/data/data/com.termux/files/usr'
export HOME='/data/data/com.termux/files/home'
export LD_LIBRARY_PATH='/data/data/com.termux/files/usr/lib'
export PATH="/data/data/com.termux/files/usr/bin:/data/data/com.termux/files/usr/bin/applets:$PATH"
export LANG='en_US.UTF-8'
export SHELL='/data/data/com.termux/files/usr/bin/bash'
cd "$HOME"
exec "$SHELL" -l
#exec "$SHELL" -l
```

## 从 adb 启动

```
adb shell -t run-as com.termux /system/bin/sh /data/data/com.termux/files/home/.bash_init
```

## dropbear 配置

### 密钥目录
```
mkdir ~/.ssh
touch ~/.ssh/authorized_keys
chmod 700 ~/.ssh
chmod 600 ~/.ssh/authorized_keys
```

### 启动

```
dropbear -s -p 1337
```

### 持久化

```
echo `pidof dropbear` > /sys/fs/cgroup/cpuacct/uid_0/tasks
```


