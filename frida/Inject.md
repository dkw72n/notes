Frida Gadget Injection
====

# Android

## app

### wait for debugger
```
./jdWp-lib-injector.sh $packagename /path/to/gadget.so
```

### runtime injection

see [non-app](#non-app)

### notes

__debuggable__ app required

on magisk-rooted machine:

```
./debuggablize.sh
```

or modify the `boot.img` for good.

## non-app

build `inject.c` for desired arch.

./inject APP_OR_PID /path/to/gadget.so ""

# Windows

```
py -3 windows_injector.py -h
```
