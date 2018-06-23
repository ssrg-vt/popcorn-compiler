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

### Repositories and branches
The toolchain relies on multiple repositories that are cloned during the
installation:

- **popcorn-compiler** contains the installation script and the source of a few
  tools such as the stack transformation library and the alignment tool. For a
  regular user (i.e. non developper), this should be the only repository to
  interract with. The url is: https://github.com/ssrg-vt/popcorn-compiler.
- **llvm** and **clang** contain the compiler sources, the urls are:
  https://github.com/ssrg-vt/llvm and https://github.com/ssrg-vt/clang
- **HermitCore** contains the kernel sources. The url is:
  https://github.com/ssrg-vt/hermitcore
- **newlib** contains the C library. The url is:
  https://github.com/ssrg-vt/newlib
- **binutils** contains the binutils sources (we use ld.gold, elfedit, readelf,
  etc.). The url is: https://github.com/ssrg-vt/binutils
- **pte** contains the sources for the pthread embedded library (note that
  multi-threading is not supported by aarch64). The url is:
  https://github.com/ssrg-vt/pthread-embedded

Concerning branches, there is a stable branch for each repository. Because of
various reasons there are generally not named `master`. In addition, some
repositories have 2 stable branches: one for x86-64 and one for aarch64. The
list of stable branches is as follows:

- **popcorn-compiler**: `hermit-master`
- **llvm**: `pierre-hermit-popcorn` TODO change it
- **clang**: `pierre-hermit-popcorn` TODO change it
- **HermitCore**:
  - `llvm-stable-x86` (x86-64) TODO change
  - `llvm-stable-aarch64` (aarch64) TODO change
- **newlib**:
  - `llvm-stable` (x86-64) TODO change
  - `llvm-stable-aarch64` (aarch64) TODO change
- **binutils**: `hermit` TODO change?
- **pte**: `llvm-stable` TODO change

### Installation folder organization
- After installation, here is how the installation folder looks like:

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



