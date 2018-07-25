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

llvm_git_url = 'https://github.com/ssrg-vt/llvm.git'
llvm_git_branch = 'hermit-popcorn-master'
clang_git_url = 'https://github.com/ssrg-vt/clang.git'
clang_git_branch = 'hermit-popcorn-master'

binutils_git_url = 'https://github.com/ssrg-vt/binutils.git'
binutils_git_branch = 'hermit-popcorn-master'

hermit_git_url = 'https://github.com/ssrg-vt/HermitCore'
hermit_x86_64_git_branch = 'hermit-popcorn-x86-master'
hermit_aarch64_git_branch = 'hermit-pierre-arm-migration'

newlib_git_url = 'https://github.com/ssrg-vt/newlib'
newlib_x86_64_git_branch = 'hermit-popcorn-x86-master'
newlib_aarch64_git_branch = 'hermit-popcorn-aarch64-master'

pte_git_url = "https://github.com/ssrg-vt/pthread-embedded.git"
pte_git_branch = "hermit-popcorn-master"

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
    process_opts.add_argument("--only-llvm-clang-install",
                        help="Install only llvm and clang",
                        action="store_true",
                        dest="only_llvm_clang_install")

    process_opts.add_argument("--skip-binutils-install",
                        help="Skip installation of binutils",
                        action="store_true",
                        dest="skip_binutils_install")
    process_opts.add_argument("--only-binutils-install",
                        help="Install only binutils",
                        action="store_true",
                        dest="only_binutils_install")

    process_opts.add_argument("--skip-hermit-x86-64-install",
                        help="Skip installation of the HermitCore x86_64 kernel",
                        action="store_true",
                        dest="skip_hermit_x86_64_install")
    process_opts.add_argument("--only-hermit-x86-64-install",
                        help="Install only hermit x86-64",
                        action="store_true",
                        dest="only_hermit_x86_64_install")

    process_opts.add_argument("--skip-hermit-aarch64-install",
                        help="Skip installation of the HermitCore aarch64 kernel",
                        action="store_true",
                        dest="skip_hermit_aarch64_install")
    process_opts.add_argument("--only-hermit-aarch64-install",
                        help="Install only hermit aarch64",
                        action="store_true",
                        dest="only_hermit_aarch64_install")

    process_opts.add_argument("--skip-newlib-x86-64-install",
                        help="Skip installation of newlib x86_64",
                        action="store_true",
                        dest="skip_newlib_x86_64_install")
    process_opts.add_argument("--only-newlib-x86-64-install",
                        help="Install only newlib x86-64",
                        action="store_true",
                        dest="only_newlib_x86_64_install")

    process_opts.add_argument("--skip-newlib-aarch64-install",
                        help="Skip installation of newlib aarch64",
                        action="store_true",
                        dest="skip_newlib_aarch64_install")
    process_opts.add_argument("--only-newlib-aarch64-install",
                        help="Install only newlib aarch64",
                        action="store_true",
                        dest="only_newlib_aarch64_install")

    process_opts.add_argument("--skip-pte-install",
                        help="Skip installation of pthread-embedded",
                        action="store_true",
                        dest="skip_pte_install")
    process_opts.add_argument("--only-pte-install",
                        help="Install only pthread-embedded",
                        action="store_true",
                        dest="only_pte_install")

    process_opts.add_argument("--skip-libraries-install",
                        help="Skip installation of libraries",
                        action="store_true",
                        dest="skip_libraries_install")
    process_opts.add_argument("--only-libraries-install",
                        help="Install only libraries",
                        action="store_true",
                        dest="only_libraries_install")

    process_opts.add_argument("--skip-tools-install",
                        help="Skip installation of tools",
                        action="store_true",
                        dest="skip_tools_install")
    process_opts.add_argument("--only-tools-install",
                        help="Install only tools",
                        action="store_true",
                        dest="only_tools_install")

    process_opts.add_argument("--skip-utils-install",
                        help="Skip installation of util scripts",
                        action="store_true",
                        dest="skip_utils_install")
    process_opts.add_argument("--only-utils-install",
                        help="Install only utils",
                        action="store_true",
                        dest="only_utils_install")

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
                sys.exit("Unsupported target '{}'!".format(target))
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
# Pierre: for some reason we need libc6-devi-i386 which conflicts with aarch64-gcc
# any way we will probably not use this cross compiler so let's comment this for
# now
#    for target in args.install_targets:
#        gcc_prerequisites.append('{}-linux-gnu-gcc'.format(target))
    other_prequisites = ['flex', 'bison', 'cmake', 'make', 'wget', 'nasm',
            'zip', 'svn']

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

    # hexdump
    if not os.path.isfile('/usr/bin/hexdump'):
        print('Cannot find hexdump! please install debian package bsdmainutils')
        success = False

    # texinfo
    if not os.path.isfile('/usr/bin/makeinfo'):
        print('Cannot find makeinfo! please install debian package texinfo')
        success = False

    # Check if /usr/include/asm exists
    if not os.path.isdir('/usr/include/asm'):
        print('/usr/include/asm does not exists, you probably want to create it:')
        print('sudo ln -s /usr/include/asm-generic /usr/include/asm')
        success = False

    return success

def install_clang_llvm(base_path, install_path, num_threads, llvm_targets):

    llvm_download_path = os.path.join(install_path, 'x86_64-host/src/llvm')
    clang_download_path = os.path.join(llvm_download_path, 'tools/clang')

#    llvm_patch_path = os.path.join(base_path, 'patches/llvm/llvm-3.7.1.patch')
#    clang_patch_path = os.path.join(base_path, 'patches/llvm/clang-3.7.1.patch')
#    llvm_hermit_patch_path = os.path.join(base_path, 'patches/llvm/hermit-llvm-3.7.1.patch')
#    clang_hermit_patch_path = os.path.join(base_path, 'patches/llvm/hermit-clang-3.7.1.patch')

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
            rv = subprocess.check_call(['git', 'clone', '--depth=50',
                '-b', llvm_git_branch, llvm_git_url, llvm_download_path])
        except Exception as e:
            sys.exit('Could not download LLVM source ({})!'.format(e))
        else:
            if rv != 0:
                sys.exit('LLVM source download failed.')

        #=====================================================
        # DOWNLOAD CLANG
        #=====================================================
        print('Downloading Clang source...')
        try:
            rv = subprocess.check_call(['git', 'clone', '--depth=50',
                '-b', clang_git_branch, clang_git_url, clang_download_path])
        except Exception as e:
            sys.exit('Could not download Clang source ({})!'.format(e))
        else:
            if rv != 0:
                sys.exit('Clang source download failed.')

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
            sys.exit('Could not run CMake ({})!'.format(e))
        else:
            if rv != 0:
                sys.exit('CMake failed.')

        try:
            print('Running Make...')
            rv = subprocess.check_call(['make', 'REQUIRES_RTTI=1',
                                        '-j', str(num_threads)])
            rv = subprocess.check_call(['make', 'install',
                                        '-j', str(num_threads)])
        except Exception as e:
            sys.exit('Could not run Make ({})!'.format(e))
        else:
            if rv != 0:
                sys.exit('Make failed.')

        os.chdir(cur_dir)

def install_libraries(base_path, install_path, targets, num_threads, st_debug,
                      libmigration_type, enable_libmigration_timing):
    cur_dir = os.getcwd()

    #=====================================================
    # CONFIGURE & INSTALL LIBELF x86_64-hermit
    #=====================================================
    os.chdir(os.path.join(base_path, 'lib/libelf_hermit'))

    print('Making & installing libelf x86_64-hermit...')
    try:
        rv = subprocess.check_call(['make',
	    'ARCH=x86_64',
            'INSTALL_PREFIX=%s' % (install_path),
            'POPCORN_INSTALL=%s' % (install_path),
            '-j', str(num_threads), 'install'])
    except Exception as e:
        sys.exit('Could not run Make install libelf x86_64-hermit ({})!'.format(e))
    else:
        if rv != 0:
            sys.exit('Make install libelf x86_64-hermit failed.')

    #=====================================================
    # CONFIGURE & INSTALL LIBELF aarch64-hermit
    #=====================================================

    print('Making & installing libelf aarch64-hermit...')
    try:
        rv = subprocess.check_call(['make', '-B',
	    'ARCH=aarch64',
            'INSTALL_PREFIX=%s' % (install_path),
            'POPCORN_INSTALL=%s' % (install_path),
            '-j', str(num_threads), 'install'])
    except Exception as e:
        sys.exit('Could not run Make install libelf aarch64-hermit ({})!'.format(e))
    else:
        if rv != 0:
            sys.exit('Make install libelf aarch64-hermit failed.')

    os.chdir(cur_dir)

    #=====================================================
    # CONFIGURE & INSTALL LIBELF for the x86_64-host
    #=====================================================

    os.chdir(os.path.join(base_path, 'lib/libelf'))

    if os.path.isfile('Makefile'):
        try:
            rv = subprocess.check_call(['make', 'distclean'])
        except Exception as e:
            sys.exit('Error running distclean!')
        else:
            if rv != 0:
                sys.exit('Make distclean failed.')

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
        sys.exit('Could not configure libelf x86_64-host ({})'.format(e))
    else:
        if rv != 0:
            sys.exit('libelf x86_64-host configure failed')

    print('Making libelf...')
    try:
        print('Running Make...')
        rv = subprocess.check_call(['make', '-j', str(num_threads)])
        rv = subprocess.check_call(['make', 'install'])
    except Exception as e:
        sys.exit('Could not run Make ({})!'.format(e))
    else:
        if rv != 0:
            sys.exit('Make failed.')

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
        sys.exit('Could not run Make ({})!'.format(e))
    else:
        if rv != 0:
            sys.exit('Make failed.')

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
         sys.exit('Could not run Make ({})!'.format(e))
    else:
        if rv != 0:
            sys.exit('Make failed')

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
        sys.exit('Could not run Make ({})!'.format(e))
    else:
        if rv != 0:
            sys.exit('Make failed.')

    #====================================================
    # INSTALL hermit x86 object files + linker script
    #====================================================
    objs_dir = base_path + '/util/hermit/x86_64-objs'
    os.chdir(objs_dir)
    for f in ['crti.o', 'crtn.o', 'crtbegin.o', 'crtend.o', 'libgcc.a']:
        shutil.copyfile(objs_dir + '/' + f, install_path + '/x86_64-hermit/lib/' + f)

    shutil.copyfile(base_path + '/util/hermit/x86_64_ls.x',
            install_path + '/x86_64-hermit/lib/ls.x')

    os.chdir(cur_dir)

    #====================================================
    # INSTALL hermit aarch64 object files + linker script
    #====================================================
    objs_dir = base_path + '/util/hermit/aarch64-objs'
    os.chdir(objs_dir)
    for f in ['crti.o', 'crtn.o', 'crtbegin.o', 'crtend.o', 'libgcc.a']:
        shutil.copyfile(objs_dir + '/' + f, install_path + '/aarch64-hermit/lib/' + f)

    shutil.copyfile(base_path + '/util/hermit/aarch64_ls.x',
            install_path + '/aarch64-hermit/lib/ls.x')

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
        sys.exit('Could not run Make ({})!'.format(e))
    else:
        if rv != 0:
            sys.exit('Make failed.')

    os.chdir(cur_dir)

def install_utils(base_path, install_path, num_threads):
    #=====================================================
    # COPY SCRIPTS
    #=====================================================
    print("Copying util/scripts to {}/x86_64-host/bin...".format(install_path))
    for item in os.listdir('./util/scripts'):
        s = os.path.join('./util/scripts/', item)
        d = os.path.join(os.path.join(install_path + '/x86_64-host/', 'bin'), item)
        if item != 'README':
            shutil.copy(s, d)

def install_pte(base_path, install_path, threads):
    cur_dir = os.getcwd()
    pte_download_path = os.path.join(install_path, 'x86_64-host/src/pte')

    # Cleanup src dir if needed
    if(os.path.isdir(pte_download_path)):
        shutil.rmtree(pte_download_path)

    print('Downloading pthread-embedded')

    try:
        rv = subprocess.check_call(['git', 'clone', '--depth=50', '-b',
            pte_git_branch, pte_git_url, pte_download_path])
    except Exception as e:
        sys.exit('Cannot download pthread-embedded: {}'.format(e))

    print('Configuring and building pthread-embedded')
    os.chdir(pte_download_path)

    try:
        rv = subprocess.check_call(['./configure', '--target=x86_64-hermit',
        '--prefix=%s' % install_path,
        'CC_FOR_TARGET=%s/x86_64-host/bin/clang' % install_path])

        rv = subprocess.check_call(['make',
            'CC_FOR_TARGET=%s/x86_64-host/bin/clang' % install_path,
            'AR_FOR_TARGET=%s/x86_64-host/bin/x86_64-hermit-ar' % install_path])

        rv = subprocess.check_call(['make', 'install'])
    except Exception as e:
        sys.exit('Cannot build/install pthread-embedded: {}'.format(e))

    os.chdir(cur_dir)

def install_newlib_x86_64(base_path, install_path, threads):
    cur_dir = os.getcwd()
    newlib_download_path = os.path.join(install_path, 'x86_64-host/src/newlib_x86_64')

    # Cleanup src dir if needed
    if(os.path.isdir(newlib_download_path)):
        shutil.rmtree(newlib_download_path)

    print('Downloading newlib x86_64')

    try:
        rv = subprocess.check_call(['git', 'clone', '--depth=50', '-b',
            newlib_x86_64_git_branch, newlib_git_url, newlib_download_path])
    except Exception as e:
        sys.exit('Cannot download newlib for x86_64: {}'.format(e))

    print('Configuring newlib for x86_64')
    os.makedirs(newlib_download_path + '/build')
    os.chdir(newlib_download_path + '/build')

    newlib_conf = ['../configure', '--target=x86_64-hermit',
            '--prefix=%s' % install_path, '--disable-shared',
            '--disable-multilib', '--enable-lto', '--enable-newlib-hw-fp',
            '--enable-newlib-io-c99-formats', '--enable-newlib-multithread',
            '--disable-newlib-nano-formatted-io',
            'target_alias=x86_64-hermit',
            'CC=%s/x86_64-host/bin/clang' % install_path,
            'CC_FOR_TARGET=%s/x86_64-host/bin/clang' % install_path,
            'AS_FOR_TARGET=%s/x86_64-host/bin/x86_64-hermit-as' % install_path,
            'AR_FOR_TARGET=%s/x86_64-host/bin/x86_64-hermit-ar' % install_path,
            'RANLIB_FOR_TARGET=%s/x86_64-host/bin/x86_64-hermit-ranlib' % install_path,
            'CFLAGS_FOR_TARGET=-O2 -m64 -DHAVE_INITFINI_ARRAY -ffunction-sections -fdata-sections -mtune=native -popcorn-libc']

    try:
        rv = subprocess.check_call(newlib_conf)
    except Exception as e:
        sys.exit('Cannot configure newlib x86_64: {}'.format(e))

    print('Building and installing newlib x86_64')
    try:
        rv = subprocess.check_call(['make', '-j', str(threads)])
        rv = subprocess.check_call(['make', 'install'])
    except Exception as e:
        sys.exit('Cannot build/install newlib x86_64: {}'.format(e))

    os.chdir(cur_dir)

def install_newlib_aarch64(base_path, install_path, threads):
    cur_dir = os.getcwd()
    newlib_download_path = os.path.join(install_path, 'x86_64-host/src/newlib_aarch64')

    # Cleanup src dir if needed
    if(os.path.isdir(newlib_download_path)):
        shutil.rmtree(newlib_download_path)

    print('Downloading newlib aarch64')

    try:
        rv = subprocess.check_call(['git', 'clone', '--depth=50', '-b',
            newlib_aarch64_git_branch, newlib_git_url, newlib_download_path])
    except Exception as e:
        sys.exit('Cannot download newlib aarch64: {}'.format(e))

    print('Configuring newlib aarch64')
    os.makedirs(newlib_download_path + '/build')
    os.chdir(newlib_download_path + '/build')

    try:
       rv = os.environ["CFLAGS_FOR_TARGET"] = "-m64 -DHAVE_INITFINI_ARRAY -O2 -ftree-vectorize -target aarch64-hermit -ffunction-sections -fdata-sections -popcorn-libc"
       rv = os.environ["CXXFLAGS_FOR_TARGET"] = "-m64 -O3 -ftree-vectorize"
       rv = os.environ["AS_FOR_TARGET"] = "%s/x86_64-host/bin/aarch64-hermit-as" % install_path
       rv = os.environ["AR_FOR_TARGET"] = "%s/x86_64-host/bin/aarch64-hermit-ar" % install_path
       rv = os.environ["RANLIB_FOR_TARGET"] = "%s/x86_64-host/bin/aarch64-hermit-ranlib" % install_path
       rv = os.environ["CC_FOR_TARGET"] = "%s/x86_64-host/bin/clang" % install_path
       rv = os.environ["CC"] = "%s/x86_64-host/bin/clang" % install_path

    except Exception as e:
        sys.exit('Cannot export newlib aarch64 Environment: {}'.format(e))

    newlib_conf = ['../configure', '--target=aarch64-hermit',
            '--prefix=%s' % install_path, '--disable-shared',
            '--disable-multilib', '--enable-lto', '--enable-newlib-hw-fp',
            '--enable-newlib-io-c99-formats', '--enable-newlib-multithread',
            'target_alias=aarch64-hermit']

    try:
        rv = subprocess.check_call(newlib_conf)
    except Exception as e:
       sys.exit('Cannot configure newlib aarch64: {}'.format(e))

    print('Building and installing newlib aarch64')
    try:
        rv = subprocess.check_call(['make', '-j', str(threads)])
        rv = subprocess.check_call(['make', 'install'])
        rv = shutil.copyfile(newlib_download_path + '/build/aarch64-hermit/newlib/libc/sys/hermit/crt0.o', install_path + '/aarch64-hermit/lib/crt0.o')
    except Exception as e:
       sys.exit('Cannot build/install newlib aarch64: {}'.format(e))

    os.chdir(cur_dir)

def install_hermit_x86_64(base_path, install_path, threads):
    cur_dir = os.getcwd()
    hermit_download_path = os.path.join(install_path, 'x86_64-host/src/HermitCore_x86_64')

    # Cleanup src dir if needed
    if(os.path.isdir(hermit_download_path)):
        shutil.rmtree(hermit_download_path)

    print('Downloading HermitCore x86_64 kernel')

    try:
        rv = subprocess.check_call(['git', 'clone',
            '--recurse-submodules', '-b', hermit_x86_64_git_branch, hermit_git_url,
            hermit_download_path])
    except Exception as e:
        sys.exit('Cannot download HermitCore x86_64: {}'.format(e))

    print('Running Cmake for HermitCore x86_64')

    os.makedirs(hermit_download_path + '/build')
    os.chdir(hermit_download_path + '/build')

    try:
        rv = subprocess.check_call(['cmake', '-DCMAKE_INSTALL_PREFIX=%s' %
            install_path, '-DCOMPILER_BIN_DIR=%s' % install_path +
            '/x86_64-host/bin', '-DHERMIT_PREFIX=%s' % install_path + '/x86_64-host',
            '-DMIGRATION_LOG=1', '-DKERNEL_DEBUG=1', #TODO remove debug options or make it optional at some point
            '..'])
    except Exception as e:
        sys.exit('Error running hermitcore x86_64 cmake: {}'.format(e))

    print('Building hermitcore x86_64')

    try:
        rv = subprocess.check_call(['make', '-j', str(threads), 'install'])
    except Exception as e:
        sys.exit('Error building hermitcore x86_64: {}'.format(e))

    os.chdir(cur_dir)

def install_hermit_aarch64(base_path, install_path, threads):
    cur_dir = os.getcwd()
    hermit_download_path = os.path.join(install_path, 'x86_64-host/src/HermitCore_aarch64')

    # Cleanup src dir if needed
    if(os.path.isdir(hermit_download_path)):
        shutil.rmtree(hermit_download_path)

    print('Downloading HermitCore aarch64 kernel')

    try:
        rv = subprocess.check_call(['git', 'clone',
            '--recurse-submodules', '-b', hermit_aarch64_git_branch, hermit_git_url,
            hermit_download_path])
    except Exception as e:
        sys.exit('Cannot download HermitCore aarch64: {}'.format(e))

    print('Running Cmake for HermitCore aarch64')

    os.makedirs(hermit_download_path + '/build')
    os.chdir(hermit_download_path + '/build')

    try:
        rv = subprocess.check_call(['cmake', '-DHERMIT_ARCH=aarch64',
            '-DTARGET_ARCH=aarch64-hermit', '-DCMAKE_INSTALL_PREFIX=%s' %
            install_path, '-DCOMPILER_BIN_DIR=%s' % install_path +
            '/x86_64-host/bin', '-DHERMIT_PREFIX=%s' % install_path + '/x86_64-host',
            '-DMIGRATION_LOG=1', '-DKERNEL_DEBUG=1', #TODO remove debug options or make it optional at some point
            '..'])
    except Exception as e:
        sys.exit('Error running hermitcore aarch64 cmake: {}'.format(e))

    print('Building hermitcore aarch64')

    try:
        rv = subprocess.check_call(['make', '-j', str(threads), 'install'])
    except Exception as e:
        sys.exit('Error building hermitcore aarch64: {}'.format(e))

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
        sys.exit('Cannot download binutils: {}'.format(e))

    print('Compiling binutils (hermit x86_64)')

    os.makedirs(binutils_download_path + '/build_x86_64')
    os.chdir(binutils_download_path + '/build_x86_64')

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
        sys.exit('Error building binutils x86_64: {}'.format(e))

    print('Compiling binutils (hermit aarch64)')

    os.makedirs(binutils_download_path + '/build_aarch64')
    os.chdir(binutils_download_path + '/build_aarch64')

    #compile and install for x86_64 target
    try:
        rv = subprocess.check_call(['../configure', '--target=aarch64-hermit',
            '--prefix=%s' % binutils_prefix, '--with-sysroot',
            '--disable-multilib', '--disable-shared', '--disable-nls',
            '--disable-gdb',
            '--disable-libdecnumber', '--disable-readline', '--disable-sim',
            '--disable-libssp', '--enable-tls', '--enable-lto',
            '--enable-plugin'])

        rv = subprocess.check_call(['make', '-j', str(threads)])
        rv = subprocess.check_call(['make', 'install', '-j', str(threads)])

    except Exception as e:
        sys.exit('Error building binutils for aarch64 target: {}'.format(e))

    print('Compiling gold')

    # We use a hacky way to compile gold, TODO make things simpler...

    # bfd and libiberty
    for tool in ['bfd', 'libiberty']:
        os.chdir(binutils_download_path + '/' + tool)
        try:
            rv = subprocess.check_call(['./configure'])
            rv = subprocess.check_call(['make', '-j', str(threads)])
        except Exception as e:
            sys.exit('Error building %s: {}'.format(e) % tool)

    # gold
    os.chdir(binutils_download_path + '/' + 'gold')
    try:
        rv = subprocess.check_call(['./configure', '--target=x86_64-hermit',
            '--prefix=%s' % binutils_prefix])
        rv = subprocess.check_call(['make', '-j', str(threads)])
        rv = subprocess.check_call(['make', 'install'])
    except Exception as e:
        sys.exit('Error building gold: {}'.format(e))

    os.chdir(cur_dir)

def main(args):

    # Check if we have one of the 'only-*' parameters enabled
    if args.only_llvm_clang_install:
        install_clang_llvm(args.base_path, args.install_path, args.threads,
                           args.llvm_targets)
        return

    if args.only_binutils_install:
        install_binutils(args.base_path, args.install_path, args.threads)
        return

    if args.only_hermit_x86_64_install:
        install_newlib_x86_64(args.base_path, args.install_path, args.threads)
        return

    if args.only_newlib_aarch64_install:
        install_newlib_aarch64(args.base_path, args.install_path, args.threads)
        return

    if args.only_newlib_x86_64_install:
        install_newlib_x86_64(args.base_path, args.install_path, args.threads)
        return

    if args.only_hermit_aarch64_install:
        install_hermit_aarch64(args.base_path, args.install_path, args.threads)
        return

    if args.only_pte_install:
        install_pte(args.base_path, args.install_path, args.threads)
        return

    if args.only_libraries_install:
        install_libraries(args.base_path, args.install_path,
                          args.install_targets, args.threads,
                          args.debug_stack_transformation,
                          args.libmigration_type,
                          args.enable_libmigration_timing)
        return

    if args.only_tools_install:
        install_tools(args.base_path, args.install_path, args.threads)
        return

    if args.only_utils_install:
        install_utils(args.base_path, args.install_path, args.threads)
        return

    # Regular install path below:

    if not args.skip_llvm_clang_install:
        install_clang_llvm(args.base_path, args.install_path, args.threads,
                           args.llvm_targets)

    if not args.skip_binutils_install:
        install_binutils(args.base_path, args.install_path, args.threads)

    if not args.skip_newlib_x86_64_install:
        install_newlib_x86_64(args.base_path, args.install_path, args.threads)

    if not args.skip_newlib_aarch64_install:
        install_newlib_aarch64(args.base_path, args.install_path, args.threads)

    if not args.skip_hermit_x86_64_install:
        install_hermit_x86_64(args.base_path, args.install_path, args.threads)

    if not args.skip_hermit_aarch64_install:
        install_hermit_aarch64(args.base_path, args.install_path, args.threads)

    if not args.skip_pte_install:
        install_pte(args.base_path, args.install_path, args.threads)

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
            sys.exit('All prerequisites were not satisfied!')

    main(args)
