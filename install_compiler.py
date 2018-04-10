#!/usr/bin/env python2

from __future__ import print_function

import argparse
import multiprocessing
import os, os.path
import platform
import shutil
import subprocess
import sys

#================================================
# GLOBALS
#================================================

# Supported targets
supported_targets = ['aarch64', 'x86_64']
# LLVM names for targets
llvm_targets = {
    'aarch64' : 'AArch64',
    'x86_64' : 'X86'
}

llvm_direct_url = 'http://releases.llvm.org/3.7.1/llvm-3.7.1.src.tar.xz'
clang_direct_url = 'http://releases.llvm.org/3.7.1/cfe-3.7.1.src.tar.xz'

binutils_git_url = 'https://github.com/ssrg-vt/binutils.git'
binutils_git_branch = 'hermit'

hermit_git_url = 'https://github.com/ssrg-vt/HermitCore'
hermit_git_branch = 'llvm-stable-pierre' #TODO switch to llvm-stable when things are actually stable

newlib_git_url = 'https://github.com/ssrg-vt/newlib'
newlib_git_branch = 'llvm-stable'

#================================================
# LOG CLASS
#   Logs all output to outfile as well as stdout
#================================================
class Log(object):
    def __init__(self, name, mode):
        self.file = open(name, mode)
    def __del__(self):
        self.file.close()
    def write(self, data):
        self.file.write(data)

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
                        default="%s/hermit-popcorn/" % os.path.expanduser("~"),
                        dest="install_path")
    config_opts.add_argument("--threads",
                        help="Number of threads to build compiler with",
                        type=int,
                        default=multiprocessing.cpu_count(),
                        dest="threads")
    process_opts = parser.add_argument_group('Process Options (skip steps)')
    process_opts.add_argument("--skip-prereq-check",
                        help="Skip checking for prerequisites (see README)",
                        action="store_true",
                        dest="skip_prereq_check")
    process_opts.add_argument("--skip-llvm-clang-install",
                        help="Skip installation of LLVM and Clang",
                        action="store_true",
                        dest="skip_llvm_clang_install")
    process_opts.add_argument("--skip-binutils-install",
                        help="Skip installation of binutils",
                        action="store_true",
                        dest="skip_binutils_install")
    process_opts.add_argument("--skip-hermit-install",
                        help="Skip installation of the HermitCore kernel",
                        action="store_true",
                        dest="skip_hermit_install")
    process_opts.add_argument("--skip-newlib-install",
                        help="Skip installation of newlib",
                        action="store_true",
                        dest="skip_newlib_install")
    process_opts.add_argument("--skip-libraries-install",
                        help="Skip installation of libraries",
                        action="store_true",
                        dest="skip_libraries_install")
    process_opts.add_argument("--skip-tools-install",
                        help="Skip installation of tools",
                        action="store_true",
                        dest="skip_tools_install")
    process_opts.add_argument("--skip-utils-install",
                        help="Skip installation of util scripts",
                        action="store_true",
                        dest="skip_utils_install")
    process_opts.add_argument("--install-call-info-library",
                        help="Install application call information library",
                        action="store_true",
                        dest="install_call_info_library")
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
        try:
             out = subprocess.check_output([prereq, '-v'],
                                      stderr=subprocess.STDOUT)
        except Exception:
            print('{} not found!'.format(prereq))
            return None

    out = out.split('\n')[0]
    return out

def check_for_prerequisites(args):
    success = True

    print('Checking for prerequisites (see README for more info)...')
    gcc_prerequisites = ['x86_64-linux-gnu-g++']
    for target in args.install_targets:
        gcc_prerequisites.append('{}-linux-gnu-gcc'.format(target))
    other_prequisites = ['flex', 'bison', 'cmake', 'make', 'wget', 'nasm']

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

    for prereq in other_prequisites:
        out = _check_for_prerequisite(prereq)
        if not out:
            success = False

    return success

def install_clang_llvm(base_path, install_path, num_threads, llvm_targets):

    llvm_download_path = os.path.join(install_path, 'x86_64-host/src/llvm')
    clang_download_path = os.path.join(llvm_download_path, 'tools/clang')

    llvm_patch_path = os.path.join(base_path, 'patches/llvm/llvm-3.7.1.patch')
    clang_patch_path = os.path.join(base_path, 'patches/llvm/clang-3.7.1.patch')
    llvm_hermit_patch_path = os.path.join(base_path, 'patches/llvm/hermit-llvm-3.7.1.patch')
    clang_hermit_patch_path = os.path.join(base_path, 'patches/llvm/hermit-clang-3.7.1.patch')

    cmake_flags = ['-DCMAKE_INSTALL_PREFIX={}/x86_64-host'.format(install_path),
                   '-DLLVM_TARGETS_TO_BUILD={}'.format(llvm_targets),
                   '-DCMAKE_BUILD_TYPE=Debug',
                   '-DLLVM_ENABLE_RTTI=ON',
                   '-DBUILD_SHARED_LIBS=ON']

    with open(os.devnull, 'wb') as FNULL:

        #=====================================================
        # DOWNLOAD LLVM
        #=====================================================
        print('Downloading LLVM source...')

        try:
            rv = subprocess.check_call(['wget', llvm_direct_url, '-O',
                                        '/tmp/llvm.tar.xz'],
                                        # stdout=FNULL,
                                        stderr=subprocess.STDOUT)

        except Exception as e:
            print('Could not download LLVM source ({})!'.format(e))
            sys.exit(1)
        else:
            if rv != 0:
                print('LLVM source download failed.')
                sys.exit(1)

        print('Extracting LLVM source...')
        try:
            rv = subprocess.check_call(['tar', 'xf', '/tmp/llvm.tar.xz',
                                        '-C', '/tmp'])
            rv = subprocess.check_call(['rm', '-rf', llvm_download_path])
            rv = subprocess.check_call(['mkdir', '-p', install_path +
                '/x86_64-host/src'])
            rv = subprocess.check_call(['mv', '-f', '/tmp/llvm-3.7.1.src',
                llvm_download_path])

        except Exception as e:
            print('Cannot extract LLVM source: {}'.format(e))
            sys.exit(1)

        #=====================================================
        # DOWNLOAD CLANG
        #=====================================================
        print('Downloading Clang source...')
        try:
          rv = subprocess.check_call(['wget', clang_direct_url, '-O',
                                        '/tmp/clang.tar.xz'],
                                        #stdout=FNULL,
                                        stderr=subprocess.STDOUT) 
        except Exception as e:
            print('Could not download Clang source ({})!'.format(e))
            sys.exit(1)
        else:
            if rv != 0:
                print('Clang source download failed.')
                sys.exit(1)

        print('Extracting Clang source...')
        try:
            rv = subprocess.check_call(['tar', 'xf', '/tmp/clang.tar.xz',
                                        '-C', '/tmp'])
            rv = subprocess.check_call(['rm', '-rf', clang_download_path])
            rv = subprocess.check_call(['mv', '-f', '/tmp/cfe-3.7.1.src',
                clang_download_path])

        except Exception as e:
            print('Cannot extract LLVM source: {}'.format(e))
            sys.exit(1)

        # PATCH LLVM
        with open(llvm_patch_path, 'r') as patch_file:

            try:
                print("Patching LLVM...")
                rv = subprocess.check_call(['patch', '-p0', '-d',
                                            llvm_download_path],
                                            stdin=patch_file,
                                            #stdout=FNULL,
                                            stderr=subprocess.STDOUT)
            except Exception as e:
                print('Could not patch LLVM({})!'.format(e))
                sys.exit(1)
            else:
                if rv != 0:
                    print('LLVM patch failed.')
                    sys.exit(1)

        # PATCH HERMIT-LLVM
        with open(llvm_hermit_patch_path, 'r') as patch_file:

            try:
                print("Patching LLVM...")
                rv = subprocess.check_call(['patch', '-p0', '-d',
                                            llvm_download_path],
                                            stdin=patch_file,
                                            #stdout=FNULL,
                                            stderr=subprocess.STDOUT)
            except Exception as e:
                print('Could not patch hermit-LLVM({})!'.format(e))
                sys.exit(1)
            else:
                if rv != 0:
                    print('Hermit-LLVM patch failed.')
                    sys.exit(1)

        # PATCH CLANG
        with open(clang_patch_path, 'r') as patch_file:

            try:
                print("Patching clang...")
                rv = subprocess.check_call(['patch', '-p0', '-d',
                                            clang_download_path],
                                            stdin=patch_file,
                                            #stdout=FNULL,
                                            stderr=subprocess.STDOUT)
            except Exception as e:
                print('Could not patch clang({})!'.format(e))
                sys.exit(1)
            else:
                if rv != 0:
                    print('clang patch failed.')
                    sys.exit(1)

        # PATCH CLANG
        with open(clang_hermit_patch_path, 'r') as patch_file:

            try:
                print("Patching hermit-clang...")
                rv = subprocess.check_call(['patch', '-p0', '-d',
                                            clang_download_path],
                                            stdin=patch_file,
                                            #stdout=FNULL,
                                            stderr=subprocess.STDOUT)
            except Exception as e:
                print('Could not patch hermit-clang({})!'.format(e))
                sys.exit(1)
            else:
                if rv != 0:
                    print('Hermit-clang patch failed.')
                    sys.exit(1)

        # BUILD AND INSTALL LLVM
        cur_dir = os.getcwd()
        os.chdir(llvm_download_path)
        os.mkdir('build')
        os.chdir('build')
        try:
            print('Running CMake...')
            rv = subprocess.check_call(['cmake'] + cmake_flags + ['..'],
                                       ##stdout=FNULL,
                                       stderr=subprocess.STDOUT)
        except Exception as e:
            print('Could not run CMake ({})!'.format(e))
            sys.exit(1)
        else:
            if rv != 0:
                print('CMake failed.')
                sys.exit(1)

        try:
            print('Running Make...')
            rv = subprocess.check_call(['make', 'REQUIRES_RTTI=1',
                                        '-j', str(num_threads)])
            rv = subprocess.check_call(['make', 'install',
                                        '-j', str(num_threads)])
        except Exception as e:
            print('Could not run Make ({})!'.format(e))
            sys.exit(1)
        else:
            if rv != 0:
                print('Make failed.')
                sys.exit(1)

        os.chdir(cur_dir)

def install_libraries(base_path, install_path, targets, num_threads, st_debug,
                      libmigration_type, enable_libmigration_timing):
    cur_dir = os.getcwd()

    #=====================================================
    # CONFIGURE & INSTALL LIBELF x86_64-hermit (ARM TODO)
    #=====================================================
    os.chdir(os.path.join(base_path, 'lib/libelf_hermit'))

    print('Making & installing libelf x86_64-hermit...')
    try:
        rv = subprocess.check_call(['make',
            'INSTALL_PREFIX=%s' % (install_path),
            'POPCORN_INSTALL=%s' % (install_path),
            '-j', str(num_threads), 'install'])
    except Exception as e:
        print('Could not run Make install libelf x86_64-hermit ({})!'.format(e))
        sys.exit(1)
    else:
        if rv != 0:
            print('Make install failed.')
            sys.exit(1)

    os.chdir(cur_dir)

    #=====================================================
    # CONFIGURE & INSTALL LIBELF for the host
    #=====================================================

    os.chdir(os.path.join(base_path, 'lib/libelf'))

    if os.path.isfile('Makefile'):
        try:
            rv = subprocess.check_call(['make', 'distclean'])
        except Exception as e:
            print('Error running distclean!')
            sys.exit(1)
        else:
            if rv != 0:
                print('Make distclean failed.')
                sys.exit(1)

    print('Configuring libelf x86_64-host')
    try:
        cflags = 'CFLAGS="-O3 -ffunction-sections -fdata-sections"'
        rv = subprocess.check_call(" ".join([cflags,
                                    './configure',
                                    '--prefix=' + install_path + '/x86_64-host',
                                    '--enable-compat',
                                    '--enable-elf64',
                                    '--disable-shared',
                                    '--enable-extended-format']),
                                    #stdout=FNULL,
                                    stderr=subprocess.STDOUT,
                                    shell=True)
    except Exception as e:
        print('Could not configure libelf x86_64-host ({})'.format(e))
        sys.exit(1)
    else:
        if rv != 0:
            print('libelf x86_64-host configure failed')
            sys.exit(1)

    print('Making libelf...')
    try:
        print('Running Make...')
        rv = subprocess.check_call(['make', '-j', str(num_threads)])
        rv = subprocess.check_call(['make', 'install'])
    except Exception as e:
        print('Could not run Make ({})!'.format(e))
        sys.exit(1)
    else:
        if rv != 0:
            print('Make failed.')
            sys.exit(1)

    os.chdir(cur_dir)

    #=====================================================
    # CONFIGURE & INSTALL STACK TRANSFORMATION LIBRARY
    #=====================================================
    os.chdir(os.path.join(base_path, 'lib/stack_transformation_hermit'))

    if not st_debug:
        flags = ''
    else:
        flags = 'type=debug'

    print('Making stack_transformation...')
    try:
        print('Running Make...')
        if flags != '':
            rv = subprocess.check_call(['make', flags, '-j',
                                        str(num_threads),
                                        'POPCORN={}'.format(install_path)])
        else:
            rv = subprocess.check_call(['make', '-j', str(num_threads),
                                        'POPCORN={}'.format(install_path)])
        rv = subprocess.check_call(['make', 'install',
                                    'POPCORN={}'.format(install_path)])
    except Exception as e:
        print('Could not run Make ({})!'.format(e))
        sys.exit(1)
    else:
        if rv != 0:
            print('Make failed.')
            sys.exit(1)

    os.chdir(cur_dir)

def install_tools(base_path, install_path, num_threads):
    cur_dir = os.getcwd()

    #=====================================================
    # INSTALL ALIGNMENT TOOL
    #=====================================================
    os.chdir(os.path.join(base_path, 'tool/alignment/pyalign'))

    print('Making pyalign...')
    try:
        print('Running Make...')
        rv = subprocess.check_call(['make', '-j', str(num_threads),
           'POPCORN={}'.format(install_path), 'install'])
    except Exception as e:
         print('Could not run Make ({})!'.format(e))
         sys.exit(1)
    else:
        if rv != 0:
            print('Make failed')
            sys.exit(1)

    os.chdir(cur_dir)

    #=====================================================
    # INSTALL STACK METADATA TOOL
    #=====================================================
    os.chdir(os.path.join(base_path, 'tool/stack_metadata'))

    print('Making stack metadata tool...')
    try:
        print('Running Make...')
        rv = subprocess.check_call(['make', '-j', str(num_threads),
                                    'POPCORN={}'.format(install_path)])
        rv = subprocess.check_call(['make', 'install',
                                    'POPCORN={}'.format(install_path)])
    except Exception as e:
        print('Could not run Make ({})!'.format(e))
        sys.exit(1)
    else:
        if rv != 0:
            print('Make failed.')
            sys.exit(1)

    os.chdir(cur_dir)

def install_call_info_library(base_path, install_path, num_threads):
    cur_dir = os.getcwd()

    #=====================================================
    # INSTALL STACK DEPTH LIBRARY
    #=====================================================
    os.chdir(os.path.join(base_path, 'lib/stack_depth'))

    print('Making stack depth library...')
    try:
        print('Running Make...')
        rv = subprocess.check_call(['make', '-j', str(num_threads),
                                    'POPCORN={}'.format(install_path)])
        rv = subprocess.check_call(['make', 'install',
                                    'POPCORN={}'.format(install_path)])
    except Exception as e:
        print('Could not run Make ({})!'.format(e))
        sys.exit(1)
    else:
        if rv != 0:
            print('Make failed.')
            sys.exit(1)

    os.chdir(cur_dir)

def install_utils(base_path, install_path, num_threads):
    #=====================================================
    # MODIFY MAKEFILE TEMPLATE
    #=====================================================
    # Pierre TODO provide a Makefile template

#    print("Updating util/Makefile.pyalign.template to reflect install path...")
#
#    try:
#        tmp = install_path.replace('/', '\/')
#        sed_cmd = "sed -i -e 's/^POPCORN := .*/POPCORN := {}/g' " \
#                  "./util/Makefile.pyalign.template".format(tmp)
#        rv = subprocess.check_call(sed_cmd, stderr=subprocess.STDOUT,shell=True)
#    except Exception as e:
#        print('Could not modify Makefile.template ({})'.format(e))
#    else:
#        if rv != 0:
#            print('sed failed.')

    #=====================================================
    # COPY SCRIPTS
    #=====================================================
    print("Copying util/scripts to {}/x86_64-host/bin...".format(install_path))
    for item in os.listdir('./util/scripts'):
        s = os.path.join('./util/scripts/', item)
        d = os.path.join(os.path.join(install_path + '/x86_64-host/', 'bin'), item)
        if item != 'README':
            shutil.copy(s, d)

def install_newlib(base_path, install_path, threads):
    cur_dir = os.getcwd()
    newlib_download_path = os.path.join(install_path, 'x86_64-host/src/newlib')

    # Cleanup src dir if needed
    if(os.path.isdir(newlib_download_path)):
        shutil.rmtree(newlib_download_path)

    print('Downloading newlib')

    try:
        rv = subprocess.check_call(['git', 'clone', '--depth=50', '-b',
            newlib_git_branch, newlib_git_url, newlib_download_path])
    except Exception as e:
        print('Cannot download newlib: {}'.format(e))
        sys.exit(1)

    print('Configuring newlib')
    os.makedirs(newlib_download_path + '/build')
    os.chdir(newlib_download_path + '/build')

    newlib_conf = ['../configure', '--target=x86_64-hermit',
            '--prefix=%s' % install_path, '--disable-shared',
            '--disable-multilib', '--enable-lto', '--enable-newlib-hw-fp',
            '--enable-newlib-io-c99-formats', '--enable-newlib-multithread',
            'target_alias=x86_64-hermit',
            'CC=%s/x86_64-host/bin/clang' % install_path,
            'CC_FOR_TARGET=%s/x86_64-host/bin/clang' % install_path,
            'AS_FOR_TARGET=%s/x86_64-host/bin/x86_64-hermit-as' % install_path,
            'AR_FOR_TARGET=%s/x86_64-host/bin/x86_64-hermit-ar' % install_path,
            'RANLIB_FOR_TARGET=%s/x86_64-host/bin/x86_64-hermit-ranlib' % install_path,
            'CFLAGS_FOR_TARGET=-O3 -m64 -DHAVE_INITFINI_ARRAY -ftree-vectorize -mtune=native']

    try:
        rv = subprocess.check_call(newlib_conf)
    except Exception as e:
        print('Cannot configure newlib: {}'.format(e))
        sys.exit(1)

    print('Building and installing newlib')
    try:
        rv = subprocess.check_call(['make', '-j', str(threads)])
        rv = subprocess.check_call(['make', 'install'])
    except Exception as e:
        print('Cannot build/install newlib: {}'.format(e))

    os.chdir(cur_dir)

def install_hermit(base_path, install_path, threads):
    cur_dir = os.getcwd()
    hermit_download_path = os.path.join(install_path, 'x86_64-host/src/HermitCore')

    # Cleanup src dir if needed
    if(os.path.isdir(hermit_download_path)):
        shutil.rmtree(hermit_download_path)

    print('Downloading HermitCore kernel')

    try:
        rv = subprocess.check_call(['git', 'clone', '--depth=50',
            '--recurse-submodules', '-b', hermit_git_branch, hermit_git_url,
            hermit_download_path])
    except Exception as e:
        print('Cannot download HermitCore: {}'.format(e))
        sys.exit(1)

    print('Running Cmake for HermitCore')
   
    os.makedirs(hermit_download_path + '/build')
    os.chdir(hermit_download_path + '/build')

    try:
        rv = subprocess.check_call(['cmake', '-DCMAKE_INSTALL_PREFIX=%s' %
            install_path, '-DCOMPILER_BIN_DIR=%s' % install_path +
            '/x86_64-host/bin', '..'])
    except Exception as e:
        print('Error running hermitcore cmake: {}'.format(e))
        sys.exit(1)

    print('Building hermitcore')

    try:
        rv = subprocess.check_call(['make', '-j', str(threads), 'install'])
    except Exception as e:
        print('Error building hermitcore: {}'.format(e))
        sys.exit(1)

    os.chdir(cur_dir)

def install_binutils(base_path, install_path, threads):
    cur_dir = os.getcwd()
    binutils_download_path = os.path.join(install_path, 'x86_64-host/src/binutils')

    print('Downloading binutils (hermit)')

    # Cleanup src dir if needed
    if(os.path.isdir(binutils_download_path)):
        shutil.rmtree(binutils_download_path)

    try:
        rv = subprocess.check_call(['git', 'clone', '--depth=50', '-b',
            binutils_git_branch, binutils_git_url, binutils_download_path])
    except Exception as e:
        print('Cannot download binutils: {}'.format(e))
        sys.exit(1)

    print('Compiling binutils (hermit)')

    os.makedirs(binutils_download_path + '/build')
    os.chdir(binutils_download_path + '/build')

    binutils_prefix = install_path + '/' + 'x86_64-host'
    try:
        rv = subprocess.check_call(['../configure', '--target=x86_64-hermit',
            '--prefix=%s' % binutils_prefix, '--with-sysroot',
            '--disable-multilib', '--disable-shared', '--disable-nls',
            '--disable-gdb',
            '--disable-libdecnumber', '--disable-readline', '--disable-sim',
            '--disable-libssp', '--enable-tls', '--enable-lto',
            '--enable-plugin'])

        rv = subprocess.check_call(['make', '-j', str(threads)])
        rv = subprocess.check_call(['make', 'install', '-j', str(threads)])

    except Exception as e:
        print('Error building binutils: {}'.format(e))
        sys.exit(1)

    print('Compiling gold')

    # We use a hacky way to compile gold, TODO make things simpler...

    # bfd and libiberty
    for tool in ['bfd', 'libiberty']:
        os.chdir(binutils_download_path + '/' + tool)
        try:
            rv = subprocess.check_call(['./configure'])
            rv = subprocess.check_call(['make', '-j', str(threads)])
        except Exception as e:
            print('Error building %s: {}'.format(e) % tool)
            sys.exit(1)

    # gold
    os.chdir(binutils_download_path + '/' + 'gold')
    try:
        rv = subprocess.check_call(['./configure', '--target=x86_64-hermit',
            '--prefix=%s' % binutils_prefix])
        rv = subprocess.check_call(['make', '-j', str(threads)])
        rv = subprocess.check_call(['make', 'install'])
    except Exception as e:
        print('Error building gold: {}'.format(e))
        sys.exit(1)

    os.chdir(cur_dir)

def main(args):

    if not args.skip_llvm_clang_install:
        install_clang_llvm(args.base_path, args.install_path, args.threads,
                           args.llvm_targets)

    if not args.skip_binutils_install:
        install_binutils(args.base_path, args.install_path, args.threads)

    if not args.skip_hermit_install:
        install_hermit(args.base_path, args.install_path, args.threads)

    if not args.skip_newlib_install:
        install_newlib(args.base_path, args.install_path, args.threads)

    if not args.skip_libraries_install:
        install_libraries(args.base_path, args.install_path,
                          args.install_targets, args.threads,
                          args.debug_stack_transformation,
                          args.libmigration_type,
                          args.enable_libmigration_timing)

    if not args.skip_tools_install:
        install_tools(args.base_path, args.install_path, args.threads)

    if args.install_call_info_library:
        install_call_info_library(args.base_path, args.install_path,
                                  args.threads)

    if not args.skip_utils_install:
        install_utils(args.base_path, args.install_path, args.threads)

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
