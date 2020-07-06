ifconfig tun1 172.18.0.2/24

ip route del default
ip route add default via 172.18.0.1

