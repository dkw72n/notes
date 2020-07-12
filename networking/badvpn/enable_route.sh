#!/bin/bash
set -e
set -x
remote_ip=`sudo netstat -anltp | grep ss-local | grep -v '127.0.0.1' | awk '{print $5}' | uniq | tr ':' ' ' | awk '{print $1}'`
default_via=`ip route show | grep -oE "default via \S+" | awk '{print $3}'`
# echo remote_ip is ${remote_ip}
sudo ip route add $remote_ip/32 via $default_via || echo fine
sudo ip route add 114.114.114.114/32 via default_via || echo fine
sudo ip route add 8.8.8.8/32 via 10.101.110.1 || echo fine
sudo ip route add default via 10.101.110.1 metric 5

