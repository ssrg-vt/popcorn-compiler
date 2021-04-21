#!/bin/bash

make clean
make

sshpass -p "popcorn" scp 359_botsspar_aarch64 popcorn@192.168.2.100:
sshpass -p "popcorn" scp 359_botsspar_x86-64 popcorn@192.168.2.100:
sshpass -p "popcorn" scp 359_botsspar_x86-64 popcorn@192.168.2.100:359_botsspar

sshpass -p "popcorn" scp 359_botsspar_aarch64 popcorn@192.168.2.101:
sshpass -p "popcorn" scp 359_botsspar_aarch64 popcorn@192.168.2.101:359_botsspar
sshpass -p "popcorn" scp 359_botsspar_x86-64 popcorn@192.168.2.101:
