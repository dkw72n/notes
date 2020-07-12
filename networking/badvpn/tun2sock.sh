#!/bin/bash
set -e
set -x

sudo ip tuntap add dev tun0 mode tun user $USER
sudo ip addr add 10.101.110.2/30 dev tun0
sudo ip link set tun0 up
./tun2socks-linux-amd64 -tunAddr 10.101.110.2 -tunGw 10.101.110.1 -tunMask 255.255.255.252 -tunName tun0 -proxyServer 127.0.0.1:8964

