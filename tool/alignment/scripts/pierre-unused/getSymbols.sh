#!/bin/bash

NM_AARCH64=../../toolchain/install_arm64/bin/aarch64-unknown-linux-gnu-nm
NM_X86=nm

find . -type f -name "*_aarch.o" -print0 |
while IFS= read -r -d '' pathname; do
        $NM_AARCH64 --numeric-sort "$pathname" >> nm_symbols_aarch64.txt
done

find . -type f -name "*_x86.o" -print0 |
while IFS= read -r -d '' pathname; do
        $NM_X86 --numeric-sort "$pathname" >> nm_symbols_x86.txt
done

