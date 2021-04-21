#!/bin/bash

make clean
make

sshpass -p "popcorn" scp 352_nab_aarch64 popcorn@192.168.2.100:
sshpass -p "popcorn" scp 352_nab_x86-64 popcorn@192.168.2.100:
sshpass -p "popcorn" scp 352_nab_x86-64 popcorn@192.168.2.100:352_nab

sshpass -p "popcorn" scp 352_nab_aarch64 popcorn@192.168.2.101:
sshpass -p "popcorn" scp 352_nab_aarch64 popcorn@192.168.2.101:
sshpass -p "popcorn" scp 352_nab_x86-64 popcorn@192.168.2.101:352_nab
