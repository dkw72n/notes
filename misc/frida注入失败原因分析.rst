前言
====

| 某项目组打了一个包，这个包上，\ ``frida-attach`` 会导致
  ``frida-server`` 和游戏进程闪退。
| 咨询了项目组，回答说没有接入其他安全防护方案，故只能从 ``frida``
  方向入手调查。

现象
----

``frida``\ 的所有工具，无论是\ ``frida``\ 、\ ``frida-trace``\ 还是
python 脚本，只要需要注入进程，注入瞬间，被注入进程就会闪退，同时
``frida-server`` 就会报 ``Segment Fault`` 然后闪退。

frida 编译
==========

为了调试此问题，需要一个带符号的 ``frida-server``,
这里简单记录一下编译过程，可直接跳过。

环境
----

.. code:: 

   ljj@ljj:~$ lsb_release -a
   No LSB modules are available.
   Distributor ID: Ubuntu
   Description:    Ubuntu 18.04.5 LTS
   Release:        18.04
   Codename:       bionic

获取代码
--------

.. code:: 

   git clone --recurse-submodules https://github.com/frida/frida.git
   cd frida

安装依赖
--------

编译相关依赖安装
~~~~~~~~~~~~~~~~

.. code:: 

   sudo apt update
   sudo apt-get install build-essential tree ninja-build gcc-multilib g++-multilib lib32stdc++-8-dev flex bison xz-utils ruby ruby-dev python3-requests python3-setuptools python3-dev python3-pip libc6-dev libc6-dev-i386 -y
    
   sudo gem install fpm -v 1.11.0 --no-document
   python3 -m pip install lief

ndk 安装
~~~~~~~~

``releng/setup-env.sh`` 有android ndk 版本的要求，当前要求是 22

.. code:: 

   wget https://dl.google.com/android/repository/android-ndk-r22b-linux-x86_64.zip
   unzip android-ndk-r22b-linux-x86_64.zip
   export ANDROID_NDK_ROOT='$PWD/android-ndk-r22b'

nodejs 安装
~~~~~~~~~~~

.. code:: 

   curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh | bash
   nvm install 11

构建
----

初始化
~~~~~~

.. code:: 

   FRIDA_HOST=android-arm64 ./releng/setup-env.sh

编译
~~~~

.. code:: 

   make core-android-arm64

输出
~~~~

输出目录在 ``build/frida-android-arm64/bin/``

.. code:: 

   ljj@ljj:/hdd/frida$ ls build/frida-android-arm64/bin/ -l
   total 95220
   -rwxr-xr-x 1 ljj ljj 45290216 Oct 27 05:03 frida-inject
   -rwxr-xr-x 1 ljj ljj  4820048 Oct 27 05:03 frida-portal
   -rwxr-xr-x 1 ljj ljj 45200048 Oct 27 05:03 frida-server
   -rwxr-xr-x 1 ljj ljj  2186352 Oct 27 05:09 gum-graft

输出目录是去符号的，我们需要的是带符号的版本，在
``build/tmp-android-arm64/frida-core/server/``

.. code:: 

   ljj@ljj:/hdd/frida$ ls build/tmp-android-arm64/frida-core/server/ -l
   total 113060
   -rwxrwxr-x 1 ljj ljj 45200048 Oct 27 05:03 frida-server
   -rwxrwxr-x 1 ljj ljj 70565160 Oct 27 05:03 frida-server-raw
   drwxrwxr-x 2 ljj ljj     4096 Oct 27 05:03 frida-server-raw.p

这个 ``frida-server-raw`` 就是我们需要的文件。

问题定位
========

重现问题
--------

由于可100%重现，调试相对简单。挂上 ``gdb``, 触发问题即可。

.. code:: 

   Thread 1 "frida-inject-ra" received signal SIGSEGV, Segmentation fault.
   _frida_linux_helper_backend_get_fifo_for_inject_instance (self=0xb400007ed69218f0, instance=0x0) at ../../../frida-core/src/linux/frida-helper-backend-glue.c:722

直接原因
--------

宕机时的语句

.. code:: 

   (gdb) x/10i $pc
   => 0x5557b65118 <_frida_linux_helper_backend_get_fifo_for_inject_instance>:     ldr     w0, [x1, #40]
      0x5557b6511c <_frida_linux_helper_backend_get_fifo_for_inject_instance+4>:   mov     w1, wzr
      0x5557b65120 <_frida_linux_helper_backend_get_fifo_for_inject_instance+8>:   b       0x5557bba1c4 <g_unix_input_stream_new>
   (gdb) info r
   x0             0xb400007ed69218f0  -5476376602116744976
   x1             0x0                 0

对应的代码是位于 ``frida-core/src/linux/frida-helper-backend-glue.c``

.. code:: 

   GInputStream *
   _frida_linux_helper_backend_get_fifo_for_inject_instance (FridaLinuxHelperBackend * self, void * instance)
   {
     return g_unix_input_stream_new (((FridaInjectInstance *) instance)->fifo, FALSE);
   }

``instance`` 为空，解引用访问 ``->fifo`` 时触发了 ``SIGSEGV``\ 。

.. _根原因分析rca）:

根原因分析（RCA）
-----------------

空值从何而来
~~~~~~~~~~~~

为了方便跟踪代码逻辑，把调用栈也打印一下

.. code:: 

   (gdb) bt
   #0  _frida_linux_helper_backend_get_fifo_for_inject_instance (self=0xb400007ed69218f0, instance=0x0) at ../../../frida-core/src/linux/frida-helper-backend-glue.c:722
   #1  0x0000005557b5cfd0 in frida_linux_helper_backend_establish_session_co (_data_=0xb400007ed6924970) at ../../../frida-core/src/linux/frida-helper-backend.vala:275
   #2  0x0000005557b5cebc in frida_linux_helper_backend_establish_session (self=<optimized out>, id=<optimized out>, pid=<optimized out>, _callback_=<optimized out>, _user_data_=<optimized out>)
       at ../../../frida-core/src/linux/frida-helper-backend.vala:2
   #3  0x0000005557b5cbac in frida_linux_helper_backend_real_inject_library_file_co (_data_=0xb400007ed6922800) at ../../../frida-core/src/linux/frida-helper-backend.vala:226
   #4  0x0000005557b5b814 in frida_linux_helper_backend_real_inject_library_file (base=<optimized out>, pid=<optimized out>, path_template=<optimized out>, entrypoint=<optimized out>, data=<optimized out>,
       temp_path=<optimized out>, id=1, cancellable=<optimized out>, _callback_=0x55555ff180 <frida_linux_helper_process_inject_library_file_ready>, _user_data_=0xb400007ed69248c0)
       at ../../../frida-core/src/linux/frida-helper-backend.vala:2
   #5  0x00000055555ff018 in frida_linux_helper_process_real_inject_library_file_co (_data_=0xb400007ed69248c0) at ../../../frida-core/src/linux/frida-helper-process.vala:99
   #6  0x0000005557bb1f30 in g_task_return_now (task=task@entry=0xb400007ed69208e0) at ../../../deps/glib/gio/gtask.c:1255
   #7  0x0000005557bb1664 in g_task_return (task=0xb400007ed69208e0, type=<optimized out>) at ../../../deps/glib/gio/gtask.c:1325

这个值是从 ``frida_linux_helper_backend_establish_session`` 传入的,
``frida_linux_helper_backend_establish_session`` 是 vala 编译成 c
自动生成的文件名，对应的源码在
``/frida-core/src/linux/frida-helper-backend.vala``:

.. code:: 

   		private async void establish_session (uint id, uint pid) throws Error {
   			var fifo = _get_fifo_for_inject_instance (inject_instances[id]);

   			...
   		}
   		public async void inject_library_file (/*省略参数*/) throws Error, IOError {
   			string path = path_template.expand (arch_name_from_pid (pid));
   			_do_inject (pid, path, entrypoint, data, temp_path, id);
   			yield establish_session (id, pid);
   		}

这里 ``establish_session`` 函数直接读取了 ``inject_instances[id]``
，未经判断就直接传给了 ``_get_fifo_for_inject_instance`` 导致宕机。

从旁边其他函数看，这个 ``inject_instances[id]`` 是不保证非空的：

.. code:: 

   		public async void recreate_injectee_thread (uint pid, uint id, Cancellable? cancellable) throws Error, IOError {
   			var instance = inject_instances[id];
   			if (instance == null)
   				throw new Error.INVALID_ARGUMENT ("Invalid ID");
   			...
   		}
   		public async void demonitor (uint id, Cancellable? cancellable) throws Error, IOError {
   			var instance = inject_instances[id];
   			if (instance == null)
   				throw new Error.INVALID_ARGUMENT ("Invalid ID");
   			...
   		}
   		...

接下来的问题是，这个 ``inject_instances[id]`` 是啥，为啥它会是空值？

为了回答这个问题，首先看 ``inject_instances[id]``\ 赋值的地方
``frida-core/src/linux/frida-helper-backend-glue.c``\ ：

.. code:: 

   void
   _frida_linux_helper_backend_do_inject (...)
   {
   	... // 注入操作

   	gee_abstract_map_set (GEE_ABSTRACT_MAP (self->inject_instances), GUINT_TO_POINTER (id), instance);
   	
   	... // 错误处理和资源释放
   }

这个函数是真正负责注入操作的，注入成功后，会得到一个 ``instance``
对象，在函数结束时，通过 ``gee_abstract_map_set`` 存到
``inject_instances`` 中。

``instance``\ 为空，说明注入失败了。

注入为何失败
~~~~~~~~~~~~

接下来需要分析注入为什么失败。

frida 注入流程的流程是：

1. 获取被注入进程的关键 API 地址

2. 根据获得的 API 地址生成 payload 字节码

3. 把 payload 字节码写入远端进程

4. 在远端进程中创建线程执行 payload

根据这几个步骤，一步步下断点分析。

首先是获取被注入进程的关键 API 地址的地方：

.. code:: 

   void
   _frida_linux_helper_backend_do_inject (...)
   {
    ....
    
     params.open_impl = frida_resolve_libc_function (pid, "open");
     params.close_impl = frida_resolve_libc_function (pid, "close");
     params.write_impl = frida_resolve_libc_function (pid, "write");
     params.syscall_impl = frida_resolve_libc_function (pid, "syscall");
     
    #if defined (HAVE_ANDROID)
     params.dlopen_impl = frida_resolve_android_dlopen (pid);
     params.dlclose_impl = frida_resolve_linker_address (pid, dlclose);
     params.dlsym_impl = frida_resolve_linker_address (pid, dlsym);
    #endif
    
     if (params.dlopen_impl == 0 || params.dlclose_impl == 0 || params.dlsym_impl == 0)
       goto no_libc;

     instance = frida_inject_instance_new (self, id, pid, temp_path);
     if (instance->executable_path == NULL)
       goto premature_termination;
     ...
   }

可以看出，\ ``frida`` 获取了 ``open``
、\ ``close``\ 、\ ``write``\ 、\ ``syscall``\ 、\ ``dlopen``\ 、\ ``dlclose``\ 、\ ``dlsym``
的地址。

在 ``frida_inject_instance_new`` 处下断点：

.. code:: 

   (gdb) b frida_inject_instance_new
   Breakpoint 2 at 0x5557b63da4: file ../../../frida-core/src/linux/frida-helper-backend-glue.c, line 893.
   (gdb) c
   Continuing.

   Thread 1 "frida-inject-ra" hit Breakpoint 2, frida_inject_instance_new (backend=0xb400007ed69438f0, id=1, pid=10563, temp_path=0xb400007dc6941c80 "/data/local/tmp/frida-67b6ca733cd46dc0d4d5c60c73577d08")
       at ../../../frida-core/src/linux/frida-helper-backend-glue.c:893
   893     in ../../../frida-core/src/linux/frida-helper-backend-glue.c
   (gdb) up
   #1  _frida_linux_helper_backend_do_inject (self=0xb400007ed69438f0, pid=10563, path=0xb400007df6943890 "/data/local/tmp/frida-67b6ca733cd46dc0d4d5c60c73577d08/frida-agent-64.so",
       entrypoint=0xb400007da69f6460 "frida_agent_main", data=0xb400007df6942180 "pipe:role=client,path=/data/local/tmp/frida-67b6ca733cd46dc0d4d5c60c73577d08/pipe-8a46c44032afeb607f21a5c9b0669038",
       temp_path=0xb400007dc6941c80 "/data/local/tmp/frida-67b6ca733cd46dc0d4d5c60c73577d08", id=1, error=error@entry=0xb400007ed6944888) at ../../../frida-core/src/linux/frida-helper-backend-glue.c:609
   609     in ../../../frida-core/src/linux/frida-helper-backend-glue.c
   (gdb) info locals
   saved_regs = {regs = {549620777360, 549620777600, 4294967296, 0, 0, 0, 545460846592, 12970367467297950784, 12970367467297959856, 12970367467297959932, 12970367467297959923, 12970367467297959929,
       12970367467297959923, 8589934592, 317827579903, 0, 549755808592, 366544616368, 32, 12970367470250775136, 0, 0, 549755808656, 549599913360, 12970367467297957936, 366544601084, 549755808640, 366544618064,
       12970367470250775136, 0, 366544604204}, sp = 366546694693, pc = 549755808704, pstate = 366544629896}
   offset = <optimized out>
   params = {pid = 10563, so_path = <optimized out>, entrypoint_name = <optimized out>, entrypoint_data = <optimized out>, fifo_path = 0x0, code = {offset = 0, size = 4096}, data = {offset = 4096,
       size = 4096}, guard = {offset = <optimized out>, size = 4096}, stack = {offset = <optimized out>, size = <optimized out>}, remote_address = 0, remote_size = <optimized out>, open_impl = 491345635300,
     close_impl = 491345608000, write_impl = 491345903696, syscall_impl = 491345577136, dlopen_impl = 510408110480, dlclose_impl = 510329282692, dlsym_impl = 510329282624}
   page_size = 4096
   instance = <optimized out>
   exited = <optimized out>

此时，可以看到 ``frida`` 获取的地址，选 open 的地址打印看看：

.. code:: 

   (gdb) p/x 491345635300
   $1 = 0x72667b67e4

对照被注入进程的地址空间

.. code:: 

   726675d000-726684c000 r--p 00000000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so

地址落在一段不可执行的内存上，也就是说，\ ``frida``
获取到一个错误的地址，这必然导致注入失败，由于地址不正确，最后恢复执行的时候，就导致了游戏闪退。

那是什么原因导致 ``frida`` 获取到错误的地址呢？

地址为何出错
~~~~~~~~~~~~

接下来查看地址解析逻辑，解析的代码也是在
``frida-core/src/linux/frida-helper-backend-glue.c`` , 函数是
``frida_resolve_library_function``\ 。

这里为了展示其逻辑，省略了一些错误处理和资源释放的代码：

.. code:: 

   static GumAddress
   frida_resolve_library_function (pid_t pid, const gchar * library_name, const gchar * function_name)
   {
     local_base = frida_find_library_base (getpid (), library_name, &local_library_path
     remote_base = frida_find_library_base (pid, local_library_path, &remote_library_path);

     canonical_library_name = g_path_get_basename (local_library_path);

     module = dlopen (canonical_library_name, RTLD_GLOBAL | RTLD_LAZY);
    
     local_address = dlsym (module, function_name);
    
     remote_address = remote_base + (GUM_ADDRESS (local_address) - local_base);

     return remote_address;
   }

具体的逻辑是

1. 分别取得当前进程和被注入进程的基地址

2. 取得当前进程的 api 地址

3. 根据当前进程的 api 与基地址的偏移，算出被注入进程的 api 地址

这个逻辑没问题，除非模块基地址出错。那模块的基地址是怎么获取呢？

代码还是同一个文件中，同样地为了展示逻辑，省略了无关代码：

.. code:: 

   static GumAddress
   frida_find_library_base (pid_t pid, const gchar * library_name, gchar ** library_path)
   {
     const guint line_size = 1024 + PATH_MAX;

     maps_path = g_strdup_printf ("/proc/%d/maps", pid);

     fp = fopen (maps_path, "r");
     while (result == 0 && fgets (line, line_size, fp) != NULL)
     {
       n = sscanf (line, "%" G_GINT64_MODIFIER "x-%*x %*s %*x %*s %*s %s", &start, path);
       if (strcmp (path, library_name) == 0)
       {
           result = start;
         	if (library_path != NULL)
           	*library_path = g_strdup (path);
       }
     }
     return result;
   }

具体逻辑是

1. 遍历 ``/proc/pid/maps``

2. 找到第一个匹配的的行，返回其起始地址

现在看看被注入进程的 maps：

.. code:: 

   OnePlus8T:/ # cat /proc/10563/maps | grep libc.so
   726675d000-726684c000 r--p 00000000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d0540000-76d057b000 r--p 00000000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d057b000-76d058b000 r-xp 0003b000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d058b000-76d058c000 rwxp 0004b000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d058c000-76d058e000 r-xp 0004c000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d058e000-76d058f000 rwxp 0004e000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d058f000-76d0590000 r-xp 0004f000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d0590000-76d0594000 rwxp 00050000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d0594000-76d059c000 r-xp 00054000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d059c000-76d059d000 rwxp 0005c000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d059d000-76d05da000 r-xp 0005d000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d05da000-76d05dd000 rwxp 0009a000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d05dd000-76d05ee000 r-xp 0009d000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d05ee000-76d05ef000 rwxp 000ae000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d05ef000-76d05f3000 r-xp 000af000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d05f3000-76d05f5000 rwxp 000b3000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d05f5000-76d05f6000 r-xp 000b5000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d05f6000-76d05fa000 r--p 000b6000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so
   76d05fa000-76d05fd000 rw-p 000b9000 07:c8 33                             /apex/com.android.runtime/lib64/bionic/libc.so

第一个匹配的，并非真实的基地址（第二行才是）。

至此，原因已大致查明。

总结
====

由于被注入进程的 ``maps`` 中出现的第一个\ ``libc.so``
并非其模块起始地址，导致\ ``frida`` 计算 ``api`` 地址出错。

错误的地址\ **导致了被注入进程闪退**\ ，同时还导致了
``injection_instances[id]`` 为空。

``frida`` 在注入结束后没有判空，就直接解引用，\ **导致 ``server``
闪退**\ 。

其实 RCA 还没结束，剩下的问题是这一段多出来的 ``libc.so``
是谁分配的？这块分析相对独立，后续有结果了再补充。
