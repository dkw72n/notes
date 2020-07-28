
route add 10.0.0.0/8 10.11.128.1 metric 5
route delete 0.0.0.0
route add 0.0.0.0/0 192.168.222.1 metric 5

pause

route delete 0.0.0.0
route add 0.0.0.0/0 10.11.128.1 metric 5
route delete 10.0.0.0