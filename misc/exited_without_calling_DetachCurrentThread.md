Native thread exited without calling DetachCurrentThread
======

## 现象

根据 Android JNI 文档:

```
Threads attached through JNI must call DetachCurrentThread() before they exit.
```

在 Android M 及以下系统, 以下代码
```cpp
std::thread t([](){
    JNIEnv* env;
    jvm->AttachCurrentThread(&env, NULL);
});
```

可以触发一个 `Abort` 导致应用闪退:

```
I/AEE/AED: Abort message: 'art/runtime/thread.cc:1245] Native thread exited without calling DetachCurrentThread: Thread[12,tid=10705,Native,Thread*=0x7f8bad7800,peer=0x12e650a0,"Thread-744"]'
```

但是, 在 Android N 之后, 就这段代码就不再触发 `Abort` 了.

## 流程

`runtime/thread.cc` 中:

负责这个检查的是 `Thread::ThreadExitCallback`, 这个函数在 `Thread::Startup` 时被注册为 `Thread::pthread_key_self_` 的析构函数.

线程退出时, 若 `Thread::pthread_key_self_` 不为空则调用 `Thread::ThreadExitCallback` 检查状态.

上述逻辑经过这么多年的迭代, 都几乎没有改变.

### Android M

在 `runtime/thread.cc` 的 `Thread::Init` 中:
```cpp
CHECK_PTHREAD_CALL(pthread_setspecific, (Thread::pthread_key_self_, this), "attach self");
```
`Thread::pthread_key_self_` 被设置成当前 `Thread` 的地址.

### Android N
在 `runtime/thread.cc` 的 `Thread::Init` 中:
```cpp
#ifdef __ANDROID__
  __get_tls()[TLS_SLOT_ART_THREAD_SELF] = this;
#else
  CHECK_PTHREAD_CALL(pthread_setspecific, (Thread::pthread_key_self_, this), "attach self");
#endif
```
实际生效的宏是 `__ANDROID__`. `Thread::pthread_key_self_` 没有被赋值, 也就不会在退出时被检查到, 尽管 `Thread::Startup` 的逻辑还在.

## 总结
`M`以后, `Android` 换了一种更快的方式存放 `this`, 同时绕开了这个检查, 不知道是有意还是无意.

