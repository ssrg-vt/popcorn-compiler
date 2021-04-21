#!/bin/bash

make clean
make

sshpass -p "popcorn" scp 372_smithwa_aarch64 popcorn@192.168.2.100:
sshpass -p "popcorn" scp 372_smithwa_x86-64 popcorn@192.168.2.100:
sshpass -p "popcorn" scp 372_smithwa_x86-64 popcorn@192.168.2.100:372_smithwa

sshpass -p "popcorn" scp 372_smithwa_aarch64 popcorn@192.168.2.101:
sshpass -p "popcorn" scp 372_smithwa_aarch64 popcorn@192.168.2.101:
sshpass -p "popcorn" scp 372_smithwa_x86-64 popcorn@192.168.2.101:372_smithwa
