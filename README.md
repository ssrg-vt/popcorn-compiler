- Recommneded distribution: debian 9
- Dependencies:

 1. Debian packages
```
apt update
apt install -y build-essential nasm texinfo zip zlib1g-dev gcc-aarch64-linux-gnu g++-aarch64-linux-gnu python python3 flex bison wget bdsmainutils
```

 2. Cmake

We need Cmake version > 3.7. If the version from the repositories (```apt 
install cmake```) is too old, get it there: https://cmake.org/download/

 3. `/usr/include/asm/`

If the folder is not present on your system, create it as follows:
```
sudo ln -s /usr/include/asm-generic /usr/include/asm
```
