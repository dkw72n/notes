[rycstar/libcbinder](https://github.com/rycstar/libcbinder)

纯 C 实现的 binder 通信库, 没有依赖, 协议版本未知.

[mer-hybris/libgbinder](https://github.com/mer-hybris/libgbinder)

glib 风格的 binder 库, 支持使用配置文件适配不同的版本.

[dxwu/BinderFilter](https://github.com/dxwu/BinderFilter)

内核态的 binder 防火墙. 仓库中有 binder 调用流程及序列化相关文档.

[Balancor/BinderHelper](https://github.com/Balancor/BinderHelper)

使用 ndk 编译 libbinder

[lcodecorex/KeepAlive](https://github.com/lcodecorex/KeepAlive)

native 层 binder 与 ams 直接通信, 监控被杀后马上拉起实现保活.

仓库中有 AOSP 的部分头文件, 可直接 link 预编译的 `libbinder.so` `libutils.so` `libcutils.so`

[zhaodm/android-binder-standalone](https://github.com/zhaodm/android-binder-standalone)

binder 独立化尝试, 很久没有维护.

[hiking90/binder-linux](https://github.com/hiking90/binder-linux)

在 linux 上使用的 binder 库, 使用 AOSP 代码 + 少量 patch 进行构建.

其中 AOSP 代码是通过 shell 脚本而非 repo 获取.

[Hamz-a/frida-android-libbinder](https://github.com/Hamz-a/frida-android-libbinder)

打印 binder 通信的 frida 脚本, 目前只打印了 `BINDER_WRITE_READ` 







