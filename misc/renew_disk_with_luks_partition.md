
## 新硬盘

参考 https://askubuntu.com/a/1184272

使用 unetbootin 制作一个 clonezilla U 盘

新硬盘接硬盘盒插上

启动进入 clonezilla shell

`sudo cryptsetup luksOpen /dev/sdb3 xxx` 解密分区

退出 shell，进入向导模式

选 disk to disk 拷贝模式，跟着向导一步步走

遇到让选择是否重建分区表，选是

结束后可以选择回到 shell

用 `sudo cryptsetup luksOpen /dev/sdc3 yyy` 验证新的分区能否正常解密

新硬盘装上重启，应该能进系统

新系统中，需要对逻辑卷进行扩容

先对物理卷进行扩容：`pvresize /dev/mapper/sda3_crypt`

然后扩容逻辑卷，使用剩余的全部空间 `sudo lvextend -l +100%FREE /dev/mapper/ubuntu--vg-root`

这时 `pvs` `lvs` 都显示最新大小了，但 `df -h` 还是显示之前的大小

最后执行 `sudo resize2fs /dev/mapper/ubuntu--vg-root`, `df -h` 就能取得正确的大小了

## 旧盘处理

先把所有uuid（UUID/PARTUUID）通通改一遍，包括但不限于
`cryptsetup luksUUID ...`
`tune2fs --uuid random ...`
`pvchange --uuid`

修复 grub

先解密 `cryptsetup luksOpen /dev/sdb3 usbhdd`

挂载 root 分区，这里用的是 lvm 的分区，不是 luks 的
`mount /dev/detached-vg/root odsk`

bind 各种虚拟文件系统，为 chroot 做准备
```
 mount --bind /dev odsk/dev
 mount --bind /sys odsk/sys
 mount --bind /proc odsk/proc
```

挂载 boot 分区，它是这系列操作的目标
`mount /dev/sdb2 odsk/boot`

chroot 进去
`chroot odsk`

执行 `update-grub`

到这里，旧的硬盘应该就可以作为 usbhdd 启动了
