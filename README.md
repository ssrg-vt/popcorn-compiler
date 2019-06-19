# HEXO - Heterogeneous EXecution Offloading

General information about HEXO can be found in the related HPDC'19
[paper](https://www.ssrg.ece.vt.edu/papers/hpdc19.pdf) and
[poster] (http://popcornlinux.org/images/publications/hexo-poster.pdf).

## Dependencies

- Recommended machines: for x86, any relatively recent Intel processor should be
  supported. For arm64, our system has only been tested on the
  [librecomputer potato](https://libre.computer/products/boards/aml-s905x-cc/)
  board.
- Recomended distribution: debian 9
- Software dependencies - on the host (x86 machine):

 1. Debian packages
```
apt update
apt install -y build-essential nasm texinfo zip zlib1g-dev gcc-aarch64-linux-gnu \
 g++-aarch64-linux-gnu python python3 flex bison wget bsdmainutils subversion git
```

 2. Cmake

We need Cmake version > 3.7. If the version from the repositories (```apt 
install cmake```) is too old, get it there: https://cmake.org/download/

 3. `/usr/include/asm/`

If the folder is not present on your system, create it as follows:
```
sudo ln -s /usr/include/asm-generic /usr/include/asm
```

- Software dependencies - on the arm64 board: the only dependency is that the
board needs to run a kernel with KVM enabled.

## Installation
First clone this repo and checkout the `hermit-master` branch:
```
git clone https://github.com/ssrg-vt/popcorn-compiler.git
cd popcorn-compiler
```
Then simply launch the installation script:
```
./install_compiler.py
```

- You can use the `--install-path` option to specify a custom installation path
- You can the `--help` option to list all the possible options

## Compiling applications

### Migration points
The application needs to be instrumented with migration points. They can be
inserted by editing the code:
```C
/* This header must be inserted : */
#include <hermit/migration.h>

/* ... */

/* Insert this line to create a migration point: */
popcorn_check_migrate();

```

Alternatively the migration point insertion can be fully automated by adding
the `-finstrument-functions` compiler flag, which will then insert a call to
`popcorn_check_migrate()` at the beginning and end of each function.

In order to trigger migration, a flag first needs to be raised then upon
reaching the next migration point the application will be checkpointed for
migration to the board (if it is running on the server) or to the server (if it
is running on the board). To raise that flag, a signal `SIGUSR1` must be sent
to the hypervisor running on the host.

### Application compilation and test
We strongly advise to use our template `Makefile`, `util/hermit/Makefile`, that
can be copy-pasted or symlinked in your source directory. Running make will
compile the unikernel for both architectures. This makefile is supposed to be
run on the host and never on the board.

You may need to edit the `POPHERMIT` variable in that makefile if you selected
a custom install folder during installation. Other interesting variables to
edit in this file are:

- `MEM`: memory given to the VM (on each architecture)
- `VERBOSE`: set to `1` to enable kernel logging, useful for debugging
- `CPUS`: set the number of vCPUs, for now we support only 1
- `ARGS`: application command line arguments
- `MIGTEST=x`: automatically send a `SIGUSR1` migration signal to the
  hypervisor after `x` seconds of execution. Useful for development.
- `RESUME`: indicate that the `make test-xxx` target should reload a checkpoint
  rather than be an initial execution
- `PORT`: port to open for post-copy server after migration signal is received,
  also used on the client side to connect to the server
- `SERVER`: the ip of the post-copy server when resuming on the client
- `FULL_CHKPT_SAVE`: set to 1 for saving a full checkpoint and exiting upon
  migration (checkpoint/restart mode), as opposed to post-copy migration (0)
- `FULL_CHKPT_RESTORE`: set to 1 for restoring a full checkpoint when
  resuming (checkpoint/restart mode), as opposed to post-copy resuming (0)
- `ARM_TARGET_IP`: IP of the board, used for various makefile targets
- `ARM_TARGET_HOME`: working directory on the board


Interesting targets for this template Makefile include:

- `make test-x86`: start execution on the x86 machine
- `make test-arm`: start execution on the board
- `make transfer-full-checkpoint-to-arm`: transfer a checkpoint
  (checkpoint/restore mode) from the x86 machine to the board
- `make transfer-full-checkpoint-from-arm`: transfer a checkpoint
  (checkpoint/restore mode) from the board to the x86 machine
- `make transfer-checkpoint-to-arm`: same as above but for post-copy-mode
- `make transfer-checkpoint-from-arm`: same as above but for post-copy-mode

Each variable can be edited in the makefile but also set from the command line,
for example:
```
RESUME=1 make test-arm
```

### An example: migrating NPB IS

Make sure that the toolchain is installed, that the board and the x86 machine
are connected on the same network, and that the board's kernel has KVM enabled
(there should be a `/dev/KVM` file). Note that all the commands in this guide
will run on the server: the template makefile executes remote commands  on the
board through ssh. Thus, you need to have ssh access to a user on the board. We
advise you setup passwordless connection with `ssh-copy-id` between the user on
the x86 machine and the one on the board. Finally, it is also important that the
user on the board can access `/dev/kvm`.

Edit the template makefile and set the following variables:
- `POPHERMIT` should be set to the toolchain's install folder (leave the
  default value if you did not specify anything particular when launching the
  install script)
- `ARM_TARGET_IP` should be set to the board IP
- `SERVER` should be set to the x96 machine IP


Then go to NPB IS folder and compile the binaries after having set the dataset
size for IS (we will use B) with a symlink:
```
cd apps/npb-is
ln -sf npbparams-B.h npbparams.h
make
```
#### Running in C/R mode
Once the binaries are compiled, let's run HEXO in checkpoint/restart mode first.
Make sure that in the makefile both of these variables are set to 1:
- `FULL_CHKPT_SAVE`
- `FULL_CHKPT_RESTORE`

Then we start the execution of IS and checkpoint after 5 seconds (you may want
to set a different value here accordign to the speed of your machine):
```
MIGTEST=5 make test-x86
HERMIT_ISLE=uhyve HERMIT_MEM=2G HERMIT_CPUS=1 \
	HERMIT_VERBOSE=0 HERMIT_MIGTEST=5 \
	HERMIT_MIGRATE_RESUME=0 HERMIT_DEBUG=0 \
	HERMIT_NODE_ID=0 ST_AARCH64_BIN=prog_aarch64_aligned \
	ST_X86_64_BIN=prog_x86-64_aligned \
	HERMIT_MIGRATE_PORT=5050 HERMIT_MIGRATE_SERVER=192.168.1.2 \
	HERMIT_FULL_CHKPT_SAVE=1 \
	HERMIT_FULL_CHKPT_RESTORE=1 \
	/home/pierre/hermit-popcorn/x86_64-host//bin/proxy prog_x86-64_aligned 


 NAS Parallel Benchmarks (NPB3.3-SER) - IS Benchmark

 Size:  33554432  (class B)
 Iterations:   10

   iteration
        1
        2
        3
        4
        5
Uhyve: exiting
```

Next let's transfer the checkpoint to the board:
```
make transfer-full-checkpoint-to-arm
rsync --no-whole-file *.bin stack.bin.* tls.bin.* \
	potato:/home/pierre
```

Finally we can resume and finish the execution on the board:
```
RESUME=1 make test-arm
rsync --no-whole-file /home/pierre/hermit-popcorn/aarch64-hermit//bin/proxy prog_aarch64_aligned prog_x86-64_aligned potato:/home/pierre/
ssh potato HERMIT_ISLE=uhyve HERMIT_MEM=2G HERMIT_CPUS=1 \
	HERMIT_VERBOSE=0 HERMIT_MIGTEST=0 \
	HERMIT_MIGRATE_RESUME=1 HERMIT_DEBUG=0 \
	HERMIT_NODE_ID=1 ST_AARCH64_BIN=prog_aarch64_aligned \
	ST_X86_64_BIN=prog_x86-64_aligned \
	HERMIT_MIGRATE_PORT=5050 HERMIT_MIGRATE_SERVER=192.168.1.2 \
	HERMIT_FULL_CHKPT_SAVE=1 \
	HERMIT_FULL_CHKPT_RESTORE=1 \
	/home/pierre/proxy /home/pierre/prog_aarch64_aligned 
        6
        7
        8
        9
        10
# results output here
```
#### Running in post-copy mode
To try post-copy transfer, update the makefile to set the two following
variables to 0:
- `FULL_CHKPT_SAVE`
- `FULL_CHKPT_RESTORE`

Then issue these commands:
```
MIGTEST=5 make test-x86
# you need a new terminal window here
make transfer-checkpoint-to-arm
RESUME=1 make test-arm
```

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
various reasons there are generally not named `master`. The list of stable
branches is as follows:

- **popcorn-compiler**: `hermit-master` ([link](https://github.com/ssrg-vt/popcorn-compiler/tree/hermit-master))
- **llvm**: `hermit-popcorn-master` ([link](https://github.com/ssrg-vt/llvm/tree/hermit-popcorn-master))
- **clang**: `hermit-popcorn-master` ([link](https://github.com/ssrg-vt/clang/tree/hermit-popcorn-master))
- **HermitCore**: `hermit-popcorn-master` ([link](https://github.com/ssrg-vt/HermitCore/tree/hermit-popcorn-master))
- **newlib**: `hermit-popcorn-master` ([link](https://github.com/ssrg-vt/newlib/tree/hermit-popcorn-master))
- **binutils**: `hermit-popcorn-master` ([link](https://github.com/ssrg-vt/binutils/tree/hermit-popcorn-master))
- **pte**: `hermit-popcorn-master` ([link](https://github.com/ssrg-vt/pthread-embedded/tree/hermit-popcorn-master))

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
   the source folder, `make` then `make install` will recompile and reinstall.
   Note that for both HermitCore and Newlib there is two build folders, one for
   aarch64 and one for x86-64
   - For pte, directly in the source folder you can use `make` and 
   `make install`



