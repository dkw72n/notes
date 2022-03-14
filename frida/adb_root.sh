#!/system/bin/sh
# see: https://web.archive.org/web/20220222093559/https://liwugang.github.io/2021/07/11/magisk_enable_adbr_root.html
resetprop ro.debuggable 1
resetprop service.adb.root 1
magiskpolicy --live 'allow adbd adbd process setcurrent'
magiskpolicy --live 'allow adbd su process dyntransition'
magiskpolicy --live 'permissive { su }'
kill -9 `ps -A | grep adbd | awk '{print $2}' `
