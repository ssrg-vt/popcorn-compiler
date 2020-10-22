#!/usr/bin/env python2

from __future__ import print_function

import argparse
import multiprocessing
import os, os.path
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib

#================================================
# GLOBALS
#================================================

# The machine on which we're compiling
host = platform.machine()

# Supported targets
#supported_targets = ['aarch64', 'x86_64']
supported_targets = ['x86_64']
# LLVM names for targets
llvm_targets = {
    'aarch64' : 'AArch64',
    'powerpc64' : 'PowerPC',
    'powerpc64le' : 'PowerPC',
    'x86_64' : 'X86'
}

# LLVM 3.7.1 SVN URL
llvm_url = 'http://llvm.org/svn/llvm-project/llvm/tags/RELEASE_371/final'
llvm_revision = '320332'
# Clang SVN URL
clang_url = 'http://llvm.org/svn/llvm-project/cfe/tags/RELEASE_371/final'
# Binutils 2.27 URL
binutils_url = 'http://ftp.gnu.org/gnu/binutils/binutils-2.27.tar.bz2'

#================================================
# ARGUMENT PARSING
#================================================
def setup_argument_parsing():
    global supported_targets

    parser = argparse.ArgumentParser(
                formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    config_opts = parser.add_argument_group('Configuration Options')
    config_opts.add_argument("--base-path",
                        help="Base path of popcorn-compiler repo",
                        default=os.getcwd(),
                        dest="base_path")
    config_opts.add_argument("--install-path",
                        help="Install path of Popcorn compiler",
                        default="/usr/local/popcorn",
                        dest="install_path")
    config_opts.add_argument("--threads",
                        help="Number of threads to build compiler with",
                        type=int,
                        default=multiprocessing.cpu_count(),
                        dest="threads")

    process_opts = parser.add_argument_group('Component Installation Options')
    process_opts.add_argument("--skip-prereq-check",
                        help="Skip checking for prerequisites (see README)",
                        action="store_true",
                        dest="skip_prereq_check")
    process_opts.add_argument("--install-all",
                        help="Install all toolchain components",
                        action="store_true",
                        dest="install_all")

    process_opts.add_argument("--install-llvm-clang",
                        help="Install LLVM and Clang",
                        action="store_true",
                        dest="llvm_clang_install")

    process_opts.add_argument("--install-binutils",
                        help="Install binutils",
                        action="store_true",
                        dest="binutils_install")

    process_opts.add_argument("--install-musl",
                        help="Install musl-libc",
                        action="store_true",
                        dest="musl_install")
    process_opts.add_argument("--install-libelf",
                        help="Install libelf",
                        action="store_true",
                        dest="libelf_install")
    process_opts.add_argument("--install-libopenpop",
                        help="Install libopenpop, Popcorn's OpenMP runtime",
                        action="store_true",
                        dest="libopenpop_install")
    process_opts.add_argument("--install-stack-transform",
                        help="Install stack transformation library",
                        action="store_true",
                        dest="stacktransform_install")
    process_opts.add_argument("--install-migration",
                        help="Install migration library",
                        action="store_true",
                        dest="migration_install")
    process_opts.add_argument("--install-stack-depth",
                        help="Install application call information library",
                        action="store_true",
                        dest="stackdepth_install")

    process_opts.add_argument("--install-tools",
                        help="Install compiler tools",
                        action="store_true",
                        dest="tools_install")

    process_opts.add_argument("--install-utils",
                        help="Install utility scripts",
                        action="store_true",
                        dest="utils_install")

    process_opts.add_argument("--install-namespace",
                        help="Install namespace tools (deprecated)",
                        action="store_true",
                        dest="namespace_install")

    selectable_targets = list(supported_targets)
    selectable_targets.extend(["all"])
    build_opts = parser.add_argument_group('Build options (per-step)')
    build_opts.add_argument("--targets",
                        help="Comma-separated list of targets to build " \
                             "(options: {})".format(selectable_targets),
                        default="all",
                        dest="targets")
    build_opts.add_argument("--debug-stack-transformation",
                        help="Enable debug output for stack transformation library",
                        action="store_true",
                        dest="debug_stack_transformation")
    build_opts.add_argument("--chameleon",
                        help="Build Popcorn compiler components for use with Chameleon",
                        action="store_true",
                        dest="chameleon")
    build_opts.add_argument("--libmigration-type",
                        help="Choose configuration for libmigration " + \
                             "(see INSTALL for more details)",
                        choices=['env_select', 'native', 'debug'],
                        dest="libmigration_type")
    build_opts.add_argument("--enable-libmigration-timing",
                        help="Turn on timing in migration library",
                        action="store_true",
                        dest="enable_libmigration_timing")

    return parser

def postprocess_args(args):
    global supported_targets
    global llvm_targets

    # Clean up paths
    args.base_path = os.path.abspath(args.base_path)
    args.install_path = os.path.abspath(args.install_path)

    # Sanity check targets requested & generate LLVM-equivalent names
    user_targets = args.targets.split(',')
    args.install_targets = []
    for target in user_targets:
        if target == "all":
            args.install_targets = supported_targets
            break
        else:
            if target not in supported_targets:
                print("Unsupported target '{}'!".format(target))
                sys.exit(1)
            args.install_targets.append(target)

    args.llvm_targets = ""
    for target in args.install_targets:
        args.llvm_targets += llvm_targets[target] + ";"
    args.llvm_targets = args.llvm_targets[:-1]

    # Turn on all components for installation if requested
    if args.install_all:
        args.llvm_clang_install = True
        args.binutils_install = True
        args.musl_install = True
        args.libelf_install = True
        args.libopenpop_install = False
        args.stacktransform_install = True
        args.migration_install = False
        args.stackdepth_install = True
        args.tools_install = True
        args.utils_install = True

def warn_stupid(args):
    if len(args.install_targets) < 2:
        print("WARNING: installing Popcorn compiler with < 2 architectures!")
    if platform.machine() != 'x86_64':
        print("WARNING: installing Popcorn compiler on '{}', " \
              "you may get errors!".format(platform.machine()))

#================================================
# PREREQUISITE CHECKING
#   Determines if all needed prerequisites are installed
#   See popcorn-compiler/README for more details
#================================================
def _check_for_prerequisite(prereq):
    try:
        out = subprocess.check_output([prereq, '--version'],
                                      stderr=subprocess.STDOUT)
    except Exception:
        print('{} not found!'.format(prereq))
        return None
    else:
        out = out.split('\n')[0]
        return out

def check_for_prerequisites(args):
    success = True

    print('Checking for prerequisites (see README for more info)...')
    gcc_prerequisites = ['x86_64-linux-gnu-g++']
    for target in args.install_targets:
        gcc_prerequisites.append('{}-linux-gnu-gcc'.format(target))
    other_prerequisites = ['flex', 'bison', 'svn', 'cmake', 'make', 'zip']

    for prereq in gcc_prerequisites:
        out = _check_for_prerequisite(prereq)
        if out:
            major, minor, micro = [int(v) for v in out.split()[3].split('.')]
            version = major * 10 + minor
            if not (version >= 48):
                print('{} 4.8 or higher required to continue'.format(prereq))
                success = False
        else:
            success = False

    for prereq in other_prerequisites:
        out = _check_for_prerequisite(prereq)
        if not out:
            success = False

    return success

#================================================
# BUILD API
#================================================

def run_cmd(name, cmd, ins=None, outs=None, use_shell=False):
    try:
        rv = subprocess.check_call(cmd, stdin=ins, stdout=outs,
                                   stderr=subprocess.STDOUT, shell=use_shell)
    except subprocess.CalledProcessError as e:
        print('Could not {} ({})!'.format(name, e))
        sys.exit(e.returncode)

def get_cmd_output(name, cmd, ins=None, use_shell=False):
    try:
        out = subprocess.check_output(cmd, stdin=ins, stderr=subprocess.STDOUT,
                                      shell=use_shell)
    except subprocess.CalledProcessError as e:
        print('Could not {} ({})!'.format(name, e))
        sys.exit(e.returncode)
    return out.decode('utf-8')

def install_clang_llvm(base_path, install_path, num_threads, llvm_targets):

    llvm_download_path = os.path.join(install_path, 'src', 'llvm')
    clang_download_path = os.path.join(llvm_download_path, 'tools', 'clang')

    patch_base = os.path.join(base_path, 'patches', 'llvm')
    llvm_patch_path = os.path.join(patch_base, 'llvm-3.7.1.patch')
    clang_patch_path = os.path.join(patch_base, 'clang-3.7.1.patch')

    cmake_flags = ['-DCMAKE_INSTALL_PREFIX={}'.format(install_path),
                   '-DLLVM_TARGETS_TO_BUILD={}'.format(llvm_targets),
                   '-DCMAKE_BUILD_TYPE=Debug',
                   '-DLLVM_ENABLE_RTTI=ON',
                   '-DBUILD_SHARED_LIBS=ON']

    #=====================================================
    # DOWNLOAD LLVM
    #=====================================================
    print('Downloading LLVM source...')
    args = ['svn', 'co', llvm_url, llvm_download_path, '-r', llvm_revision]
    run_cmd('download LLVM source', args)

    #=====================================================
    # DOWNLOAD CLANG
    #=====================================================
    print('Downloading Clang source...')
    args = ['svn', 'co', clang_url, clang_download_path, '-r', llvm_revision]
    run_cmd('download Clang source', args)

    #=====================================================
    # PATCH LLVM
    #=====================================================
    with open(llvm_patch_path, 'r') as patch_file:
        print('Patching LLVM...')
        args = ['patch', '-p0', '-d', llvm_download_path]
        run_cmd('patch LLVM', args, patch_file)

    #=====================================================
    # PATCH CLANG
    #=====================================================
    with open(clang_patch_path, 'r') as patch_file:
        print("Patching clang...")
        args = ['patch', '-p0', '-d', clang_download_path]
        run_cmd('patch Clang', args, patch_file)

    #=====================================================
    # BUILD AND INSTALL LLVM
    #=====================================================
    cur_dir = os.getcwd()
    os.chdir(llvm_download_path)
    os.mkdir('build')
    os.chdir('build')

    print('Running CMake...')
    args = ['cmake'] + cmake_flags + ['..']
    run_cmd('run CMake', args)


    print('Running Make...')
    args = ['make', '-j', str(num_threads)]
    run_cmd('run Make', args)
    args += ['install']
    run_cmd('install clang/LLVM', args)

    os.chdir(cur_dir)

def install_binutils(base_path, install_path, num_threads):

    binutils_install_path = os.path.join(install_path, 'src', 'binutils-2.27')

    patch_path = os.path.join(base_path, 'patches', 'binutils-gold',
                              'binutils-2.27-gold.patch')

    configure_flags = ['--prefix={}'.format(install_path),
                       '--enable-gold',
                       '--disable-ld',
                       '--disable-libquadmath',
                       '--disable-libquadmath-support',
                       '--disable-libstdcxx']

    #=====================================================
    # DOWNLOAD BINUTILS
    #=====================================================
    print('Downloading binutils source...')
    try:
        urllib.urlretrieve(binutils_url, 'binutils-2.27.tar.bz2')
        with tarfile.open('binutils-2.27.tar.bz2', 'r:bz2') as f:
            f.extractall(path=os.path.join(install_path, 'src'))
    except Exception as e:
        print('Could not download/extract binutils source ({})!'.format(e))
        sys.exit(1)

    #=====================================================
    # PATCH BINUTILS
    #=====================================================
    print("Patching binutils...")
    with open(patch_path, 'r') as patch_file:
        args = ['patch', '-p0', '-d', binutils_install_path]
        run_cmd('patch binutils', args, patch_file)

    #=====================================================
    # BUILD AND INSTALL BINUTILS
    #=====================================================
    cur_dir = os.getcwd()
    os.chdir(binutils_install_path)
    os.mkdir('build')
    os.chdir('build')

    print("Configuring binutils...")
    args = ['../configure'] + configure_flags
    run_cmd('configure binutils', args)

    print('Making binutils...')
    args = ['make', '-j', str(num_threads)]
    run_cmd('run Make', args)
    args += ['install']
    run_cmd('install binutils', args)

    os.chdir(cur_dir)

def install_musl(base_path, install_path, target, num_threads, chameleon):
    cur_dir = os.getcwd()

    #=====================================================
    # CONFIGURE & INSTALL MUSL
    #=====================================================
    target_install_path = os.path.join(install_path, target)
    os.chdir(os.path.join(base_path, 'lib', 'musl-1.1.18'))

    if os.path.isfile('Makefile'):
        run_cmd('clean musl', ['make', 'distclean'])

    cflags = 'CFLAGS="-target {}-linux-gnu -popcorn-libc {}"'.format(target,
        "-secure-popcorn -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer " \
        "-mno-red-zone -mllvm -blind-copy"
        if chameleon else "")

    print("Configuring musl ({})...".format(target))
    args = ' '.join(['./configure',
                     '--prefix={}'.format(target_install_path),
                     '--target={}-linux-gnu'.format(target),
                     '--enable-optimize',
                     '--enable-debug',
                     '--enable-warnings',
                     '--enable-wrapper=all',
                     '--disable-shared',
                     'CC={}/bin/clang'.format(install_path),
                     cflags])
    run_cmd('configure musl-libc ({})'.format(target), args, use_shell=True)

    print('Making musl ({})...'.format(target))
    args = ['make', '-j', str(num_threads)]
    run_cmd('make musl-libc ({})'.format(target), args)
    args += ['install']
    run_cmd('install musl-libc ({})'.format(target), args)

    os.chdir(cur_dir)

def install_libelf(base_path, install_path, target, num_threads):
    cur_dir = os.getcwd()

    #=====================================================
    # CONFIGURE & INSTALL LIBELF
    #=====================================================
    target_install_path = os.path.join(install_path, target)
    os.chdir(os.path.join(base_path, 'lib', 'libelf'))

    if os.path.isfile('Makefile'):
        run_cmd('clean libelf', ['make', 'distclean'])

    print("Configuring libelf ({})...".format(target))
    compiler = os.path.join(target_install_path, 'bin', 'musl-clang')
    args = ' '.join(['CC={}'.format(compiler),
                     'CFLAGS="-O3 -popcorn-alignment"',
                     'LDFLAGS="-static"',
                     './configure',
                     '--build={}-linux-gnu'.format(platform.machine()),
                     '--host={}-linux-gnu'.format(target),
                     '--prefix={}'.format(target_install_path),
                     '--enable-compat',
                     '--enable-elf64',
                     '--disable-shared',
                     '--enable-extended-format'])
    run_cmd('configure libelf ({})'.format(target), args, use_shell=True)

    print('Making libelf ({})...'.format(target))
    args = ['make', '-j', str(num_threads)]
    run_cmd('make libelf ({})'.format(target), args)
    args += ['install']
    run_cmd('install libelf ({})'.format(target), args)

    os.chdir(cur_dir)

def install_libopenpop(base_path, install_path, target, first_target, num_threads):
    global host

    cur_dir = os.getcwd()

    #=====================================================
    # CONFIGURE & INSTALL LIBOPENPOP
    #=====================================================
    target_install_path = os.path.join(install_path, target)
    os.chdir(os.path.join(base_path, 'lib', 'libopenpop'))
    if os.path.isfile('Makefile'):
        run_cmd('clean libopenpop', ['make', 'distclean'])

    print("Configuring libopenpop ({})...".format(target))

    # Get the libgcc file name
    # NOTE: this won't be needed if we force clang to emit 64-bit doubles when
    # compiling musl (currently, it needs soft-FP emulation for 128-bit long
    # doubles)
    args = ['{}-linux-gnu-gcc'.format(target), '-print-libgcc-file-name']
    libgcc = get_cmd_output('get libgcc file name ({})'.format(target), args)
    libgcc = libgcc.strip()

    # NOTE: for build repeatability, we have to use the *same* include path for
    # *all* targets.  This will be unnecessary with unified headers.
    include_dir = os.path.join(install_path, first_target, 'include')
    lib_dir = os.path.join(target_install_path, 'lib')

    args = ' '.join(['CC={}/bin/clang'.format(install_path),
                     'CFLAGS="-target {}-linux-gnu -O2 -g -Wall -fno-common ' \
                             '-nostdinc -isystem {} ' \
                             '-popcorn-metadata ' \
                             '-popcorn-target={}-linux-gnu"' \
                             .format(host, include_dir, target),
                     'LDFLAGS="-nostdlib -L{}"'.format(lib_dir),
                     'LIBS="{}/crt1.o -lc {}"'.format(lib_dir, libgcc),
                     './configure',
                     '--prefix={}'.format(target_install_path),
                     '--target={}-linux-gnu'.format(target),
                     '--host={}-linux-gnu'.format(target),
                     '--enable-static',
                     '--disable-shared'])
    run_cmd('configure libopenpop ({})'.format(target), args, use_shell=True)

    print('Making libopenpop ({})...'.format(target))
    args = ['make', '-j', str(num_threads)]
    run_cmd('make libopenpop ({})'.format(target), args)
    args += ['install']
    run_cmd('install libopenpop ({})'.format(target), args)

    os.chdir(cur_dir)

def install_stack_transformation(base_path, install_path, num_threads, st_debug,
                                 chameleon):
    cur_dir = os.getcwd()

    #=====================================================
    # CONFIGURE & INSTALL STACK TRANSFORMATION LIBRARY
    #=====================================================
    os.chdir(os.path.join(base_path, 'lib', 'stack_transformation'))
    run_cmd('clean libstack-transform', ['make', 'clean'])

    print('Making stack_transformation...')
    args = ['make', '-j', str(num_threads), 'POPCORN={}'.format(install_path)]
    if st_debug or chameleon:
        typeStr = 'type='
        if st_debug: typeStr += "debug,"
        if chameleon: typeStr += "chameleon,"
        args += [typeStr[:-1]]
    run_cmd('make libstack-transform', args)
    args += ['install']
    run_cmd('install libstack-transform', args)

    os.chdir(cur_dir)

def install_migration(base_path, install_path, num_threads, libmigration_type,
                      enable_libmigration_timing):
    cur_dir = os.getcwd()

    #=====================================================
    # CONFIGURE & INSTALL MIGRATION LIBRARY
    #=====================================================
    os.chdir(os.path.join(base_path, 'lib', 'migration'))
    run_cmd('clean libmigration', ['make', 'clean'])

    print('Making libmigration...')
    args = ['make', '-j', str(num_threads), 'POPCORN={}'.format(install_path)]
    if libmigration_type or enable_libmigration_timing:
        flags = 'type='
        if libmigration_type and enable_libmigration_timing:
            flags += libmigration_type + ',timing'
        elif libmigration_type:
            flags += libmigration_type
        elif enable_libmigration_timing:
            flags += 'timing'
        args += [flags]
    run_cmd('make libopenpop', args)
    args += ['install']
    run_cmd('install libopenpop', args)

    os.chdir(cur_dir)

def install_stackdepth(base_path, install_path, num_threads):
    cur_dir = os.getcwd()

    #=====================================================
    # INSTALL STACK DEPTH LIBRARY
    #=====================================================
    os.chdir(os.path.join(base_path, 'lib', 'stack_depth'))
    run_cmd('clean libstack-depth', ['make', 'clean'])

    print('Making stack depth library...')
    args = ['make', '-j', str(num_threads), 'POPCORN={}'.format(install_path)]
    run_cmd('make libstack-depth', args)
    args += ['install']
    run_cmd('install libstack-depth', args)

    os.chdir(cur_dir)

def install_tools(base_path, install_path, num_threads):
    cur_dir = os.getcwd()

    #=====================================================
    # INSTALL ALIGNMENT TOOL
    #=====================================================
    os.chdir(os.path.join(base_path, 'tool', 'alignment'))
    run_cmd('clean pyalign', ['make', 'clean'])

    print('Making pyalign...')
    args = ['make', '-j', str(num_threads), 'install',
            'POPCORN={}'.format(install_path)]
    run_cmd('make/install pyalign', args)

    os.chdir(cur_dir)

    #=====================================================
    # INSTALL STACK METADATA TOOL
    #=====================================================
    os.chdir(os.path.join(base_path, 'tool', 'stack_metadata'))
    run_cmd('clean stack metadata tool', ['make', 'clean'])

    print('Making stack metadata tool...')
    args = ['make', '-j', str(num_threads), 'POPCORN={}'.format(install_path)]
    run_cmd('make stack metadata tools', args)
    args += ['install']
    run_cmd('install stack metadata tools', args)

    #=====================================================
    # COPY METADATA HEADERS FOR OUTSIDE TOOLS
    #=====================================================
    dest = os.path.join(install_path, 'include')
    print('Copying headers defining metadata...')
    files = [ 'het_bin.h', 'rewrite_metadata.h', 'StackTransformTypes.def' ]
    for f in files:
        full = os.path.join(base_path, 'common', 'include', f)
        args = [ 'cp', full, dest ]
        run_cmd('copy metadata headers', args)

    #=====================================================
    # COPY STACK TRANSFORM API HEADERS FOR OUTSIDE TOOLS
    #=====================================================
    print('Copying headers for stack transformation API...')
    full = os.path.join(base_path, 'lib', 'stack_transformation', 'include',
                        'stack_transform.h')
    args = [ 'cp', full, dest ]
    run_cmd('copy stack transformation API headers', args)

    os.chdir(cur_dir)

def install_utils(base_path, install_path, num_threads):
    #=====================================================
    # MODIFY MAKEFILE TEMPLATE
    #=====================================================
    print("Updating util/Makefile.template to reflect install path...")

    tmp = install_path.replace('/', '\/')
    sed_cmd = "sed -i -e 's/^POPCORN := .*/POPCORN := {}/g' " \
              "./util/Makefile.template".format(tmp)
    run_cmd('update Makefile template', sed_cmd, use_shell=True)

    #=====================================================
    # COPY SCRIPTS
    #=====================================================
    print("Copying util/scripts to {}/bin...".format(install_path))
    for item in os.listdir('./util/scripts'):
        s = os.path.join('./util', 'scripts', item)
        d = os.path.join(os.path.join(install_path, 'bin'), item)
        if item != 'README':
            shutil.copy(s, d)

def build_namespace(base_path):
    cur_dir = os.getcwd()

    #=====================================================
    # MAKE NAMESPACE
    #=====================================================
    os.chdir(os.path.join(base_path, 'tool', 'namespace'))

    print("Building namespace tools...")
    run_cmd('make namespace tools', ['make'])

    os.chdir(cur_dir)

def main(args):

    if args.llvm_clang_install:
        install_clang_llvm(args.base_path, args.install_path, args.threads,
                           args.llvm_targets)

    if args.binutils_install:
        install_binutils(args.base_path, args.install_path, args.threads)

    for target in args.install_targets:
        if args.musl_install:
            install_musl(args.base_path, args.install_path, target,
                         args.threads, args.chameleon)

        if args.libelf_install:
            install_libelf(args.base_path, args.install_path, target,
                           args.threads)

    if args.stacktransform_install:
        install_stack_transformation(args.base_path, args.install_path,
                                     args.threads,
                                     args.debug_stack_transformation,
                                     args.chameleon)

    if args.migration_install:
        install_migration(args.base_path, args.install_path, args.threads,
                          args.libmigration_type,
                          args.enable_libmigration_timing)

    if args.libopenpop_install:
        for target in args.install_targets:
            install_libopenpop(args.base_path, args.install_path, target,
                               args.install_targets[0], args.threads)

    if args.stackdepth_install:
        install_stackdepth(args.base_path, args.install_path, args.threads)

    if args.tools_install:
        install_tools(args.base_path, args.install_path, args.threads)

    if args.utils_install:
        install_utils(args.base_path, args.install_path, args.threads)

    if args.namespace_install:
        build_namespace(args.base_path)

if __name__ == '__main__':
    parser = setup_argument_parsing()
    args = parser.parse_args()
    postprocess_args(args)
    warn_stupid(args)

    if not args.skip_prereq_check:
        success = check_for_prerequisites(args)
        if success != True:
            print('All prerequisites were not satisfied!')
            sys.exit(1)

    main(args)
