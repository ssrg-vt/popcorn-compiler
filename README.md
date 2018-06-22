# Heterogeneous HermitCore toolchain

For now all of this only works with x86, ARM integration is TODO.

## Dependencies

- Recommneded distribution: debian 9
- Dependencies:

 1. Debian packages
```
apt update
apt install -y build-essential nasm texinfo zip zlib1g-dev gcc-aarch64-linux-gnu g++-aarch64-linux-gnu python python3 flex bison wget bsdmainutils subversion
```

 2. Cmake

We need Cmake version > 3.7. If the version from the repositories (```apt 
install cmake```) is too old, get it there: https://cmake.org/download/

 3. `/usr/include/asm/`

If the folder is not present on your system, create it as follows:
```
sudo ln -s /usr/include/asm-generic /usr/include/asm
```

## Installation
```
./install_compiler.py
```

- You can use the `--install-path` option to specify a custom installation path
- You can the `--help` option to list all the possible options

## Compiling application
- Use `util/hermit/Makefile` in your source directory. Type `make` to compile,
`make test` to run. If your system is not configured to run kvm as a non-root
user, you probably need to run `sudo make test`.

## Modifying toolchain components
- Here is how the installation folder looks like:

```
installation_dir/
\_ x86_64-host/      - Sources & binaries for the toolchain (executed on the host)
 \_ bin/             - Toolchain binaries (clang, binutils, etc.)
 |_  include/        - Includes for the host (LLVM & Libelf)
 |_ lib/             - Libraries for the host (LLVM & libelf)
 |_ share/           - Tools for the host
 |_ src/             - Toolchain sources
  \_ binutils        - Binutils including gold, ar, etc.
  |_ HermitCore      - HermitCore kernel + libomp
  |_ llvm            - LLVM + clang
  |_ newlib          - newlib for hermitcore
  |_ pte             - pthread embedded for hermitcore
 ...
|_ x86_64-hermit/   - Include and libraries for the x86_64 guest
 ...
```
- To modify one of the tools, go to 
 `installation_dir/x86_64-host/src/<tool name>`, do your modification. Next 
 you need to recompile and reinstall the tool.
   - For LLVM, hermitcore, newlib, binutils: there is a `build` folder within
   the source folder, `make` then `make install` will recompile and reinstall
   - For pte, directly in the source folder you cna use `make` and 
   `make install`



