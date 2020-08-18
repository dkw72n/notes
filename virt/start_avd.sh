export LD_LIBRARY_PATH=${ANDROID_SDK_HOME}/emulator/lib64/qt/lib:${ANDROID_SDK_HOME}/emulator/lib64 
arch=x86_64
if test -n "${ARCH}"; then
  arch=${ARCH}
fi

QEMU=${ANDROID_SDK_HOME}/emulator/qemu/linux-x86_64/qemu-system-${arch}

#${ANDROID_SDK_HOME}/emulator/qemu/linux-x86_64/qemu-system-${arch} -netdelay none -netspeed full -no-snapshot -show-kernel -verbose -avd $1

${ANDROID_SDK_HOME}/emulator/qemu/linux-x86_64/qemu-system-${arch} -netdelay none -netspeed full -no-snapshot -no-boot-anim -show-kernel -verbose -avd $1 -qemu -append "trace_buf_size=128M trace_event=sched_process_exec,sched_process_exit,sched_process_fork"
#${QEMU} -qemu -h
