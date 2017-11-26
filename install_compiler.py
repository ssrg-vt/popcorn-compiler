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

# Supported targets
supported_targets = ['aarch64', 'x86_64']
# LLVM names for targets
llvm_targets = {
    'aarch64' : 'AArch64',
    'powerpc64' : 'PowerPC',
    'powerpc64le' : 'PowerPC',
    'x86_64' : 'X86'
}

# LLVM 3.7.1 SVN URL
llvm_url = 'http://llvm.org/svn/llvm-project/llvm/tags/RELEASE_371/final'
# Clang SVN URL
clang_url = 'http://llvm.org/svn/llvm-project/cfe/tags/RELEASE_371/final'
# Binutils 2.27 URL
binutils_url = 'http://ftp.gnu.org/gnu/binutils/binutils-2.27.tar.bz2'

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
                        help="Base path of popcorn-x86-arm compiler repo",
                        default=os.getcwd(),
                        dest="base_path")
    config_opts.add_argument("--install-path",
                        help="Install path of popcorn-x86-arm compiler",
                        default="/usr/local/popcorn",
                        dest="install_path")
    config_opts.add_argument("--threads",
                        help="Number of threads to build compiler with",
                        type=int,
                        default=multiprocessing.cpu_count())
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
    process_opts.add_argument("--skip-namespace",
                        help="Skip building namespace tools (deprecated)",
                        action="store_false",
                        dest="skip_namespace")
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
#   See popcorn-compiler-arm-x86/README for more details
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

def _check_javac():
    try:
        out = subprocess.check_output(['javac', '-version'],
                    stderr=subprocess.STDOUT)
    except Exception:
        print('javac not found!')
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
    other_prequisites = ['flex', 'bison', 'svn', 'cmake', 'make']

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

    if not _check_javac():
        success = False

    return success

def install_clang_llvm(base_path, install_path, num_threads, llvm_targets):

    llvm_download_path = os.path.join(install_path, 'src/llvm')
    clang_download_path = os.path.join(llvm_download_path, 'tools/clang')

    llvm_patch_path = os.path.join(base_path, 'patches/llvm/llvm-3.7.1.patch')
    clang_patch_path = os.path.join(base_path, 'patches/llvm/clang-3.7.1.patch')

    cmake_flags = ['-DCMAKE_INSTALL_PREFIX={}'.format(install_path),
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
            rv = subprocess.check_call(['svn', 'co', llvm_url,
                                        llvm_download_path],
                                        #stdout=FNULL,
                                        stderr=subprocess.STDOUT)
        except Exception as e:
            print('Could not download LLVM source ({})!'.format(e))
            sys.exit(1)
        else:
            if rv != 0:
                print('LLVM source download failed.')
                sys.exit(1)

        #=====================================================
        # DOWNLOAD CLANG
        #=====================================================
        print('Downloading Clang source...')
        try:
            rv = subprocess.check_call(['svn', 'co', clang_url,
                                        clang_download_path],
                                        #stdout=FNULL,
                                        stderr=subprocess.STDOUT)
        except Exception as e:
            print('Could not download Clang source ({})!'.format(e))
            sys.exit(1)
        else:
            if rv != 0:
                print('Clang source download failed.')
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

def install_binutils(base_path, install_path, num_threads):

    binutils_install_path = os.path.join(install_path, 'src/binutils-2.27')

    patch_path = os.path.join(base_path,
                              'patches/binutils-gold/binutils-2.27-gold.patch')

    configure_flags = ['--prefix={}'.format(install_path),
                       '--enable-gold',
                       '--disable-ld',
                       '--disable-libquadmath',
                       '--disable-libquadmath-support',
                       '--disable-libstdcxx']

    with open(os.devnull, 'wb') as FNULL:

        # DOWNLOAD BINUTILS
        print('Downloading binutils source...')
        try:
            urllib.urlretrieve(binutils_url, 'binutils-2.27.tar.bz2')
        except Exception as e:
            print('Could not download binutils source ({})!'.format(e))
            sys.exit(1)
        else:
            with tarfile.open('binutils-2.27.tar.bz2', 'r:bz2') as f:
                f.extractall(path=os.path.join(install_path, 'src'))


        # PATCH BINUTILS
        print("Patching binutils...")
        with open(patch_path, 'r') as patch_file:
            try:
                rv = subprocess.check_call(['patch', '-p0', '-d',
                                            binutils_install_path],
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

        # BUILD AND INSTALL BINUTILS
        cur_dir = os.getcwd()
        os.chdir(binutils_install_path)
        os.mkdir('build')
        os.chdir('build')

        print("Configuring binutils...")
        try:
            rv = subprocess.check_call(['../configure'] + configure_flags,
                                        #stdout=FNULL,
                                        stderr=subprocess.STDOUT)
        except Exception as e:
            print('Could not configure binutils({})!'.format(e))
            sys.exit(1)
        else:
            if rv != 0:
                print('binutils configure failed.')
                sys.exit(1)

        print('Making binutils...')
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

def install_libraries(base_path, install_path, targets, num_threads, st_debug,
                      libmigration_type, enable_libmigration_timing):
    cur_dir = os.getcwd()

    # musl-libc & libelf are built individually per target

    for target in targets:
        target_install_path = os.path.join(install_path, target)

        #=====================================================
        # CONFIGURE & INSTALL MUSL
        #=====================================================
        os.chdir(os.path.join(base_path, 'lib/musl-1.1.18'))

        if os.path.isfile('Makefile'):
            try:
                rv = subprocess.check_call(['make', 'distclean'])
            except Exception as e:
                print('ERROR running distclean!')
                sys.exit(1)
            else:
                if rv != 0:
                    print('Make distclean failed.')
                    sys.exit(1)

        print("Configuring musl ({})...".format(target))
        try:
            rv = subprocess.check_call(" ".join(['./configure',
                                '--prefix=' + target_install_path,
                                '--target={}-linux-gnu'.format(target),
                                '--enable-optimize',
                                '--enable-debug',
                                '--enable-warnings',
                                '--enable-wrapper=all',
                                '--disable-shared',
                                'CC={}/bin/clang'.format(install_path),
                                'CFLAGS="-target {}-linux-gnu -popcorn-libc"' \
                                .format(target)]),
                                        #stdout=FNULL,
                                        stderr=subprocess.STDOUT,
                                        shell=True)
        except Exception as e:
            print('Could not configure musl({})!'.format(e))
            sys.exit(1)
        else:
            if rv != 0:
                print('musl configure failed.')
                sys.exit(1)

        print('Making musl...')
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
        # CONFIGURE & INSTALL LIBELF
        #=====================================================
        os.chdir(os.path.join(base_path, 'lib/libelf'))

        if os.path.isfile('Makefile'):
            try:
                rv = subprocess.check_call(['make', 'distclean'])
            except Exception as e:
                print('ERROR running distclean!')
                sys.exit(1)
            else:
                if rv != 0:
                    print('Make distclean failed.')
                    sys.exit(1)

        print("Configuring libelf ({})...".format(target))
        try:
            cflags = 'CFLAGS="-O3 -ffunction-sections -fdata-sections ' + \
                     '-specs {}"'.format(os.path.join(target_install_path,
                                                     'lib/musl-gcc.specs'))
            rv = subprocess.check_call(" ".join([cflags,
                                        './configure',
                                        '--host={}-linux-gnu'.format(target),
                                        '--prefix=' + target_install_path,
                                        '--enable-compat',
                                        '--enable-elf64',
                                        '--disable-shared',
                                        '--enable-extended-format']),
                                        #stdout=FNULL,
                                        stderr=subprocess.STDOUT,
                                        shell=True)
        except Exception as e:
           print('Could not configure libelf ({})!'.format(e))
           sys.exit(1)
        else:
           if rv != 0:
               print('libelf configure failed.')
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

    # The build systems for the following already build for all ISAs

    #=====================================================
    # CONFIGURE & INSTALL STACK TRANSFORMATION LIBRARY
    #=====================================================
    os.chdir(os.path.join(base_path, 'lib/stack_transformation'))

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

    #=====================================================
    # CONFIGURE & INSTALL MIGRATION LIBRARY
    #=====================================================
    os.chdir(os.path.join(base_path, 'lib/migration'))

    if not libmigration_type and not enable_libmigration_timing:
        flags = ''
    else:
        flags = 'type='
        if libmigration_type and enable_libmigration_timing:
            flags += libmigration_type + ',timing'
        elif libmigration_type:
            flags += libmigration_type
        elif enable_libmigration_timing:
            flags += 'timing'

    print('Making libmigration...')
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
    print("Updating util/Makefile.pyalign.template to reflect install path...")

    try:
        tmp = install_path.replace('/', '\/')
        sed_cmd = "sed -i -e 's/^POPCORN := .*/POPCORN := {}/g' " \
                  "./util/Makefile.pyalign.template".format(tmp)
        rv = subprocess.check_call(sed_cmd, stderr=subprocess.STDOUT,shell=True)
    except Exception as e:
        print('Could not modify Makefile.template ({})'.format(e))
    else:
        if rv != 0:
            print('sed failed.')

    #=====================================================
    # COPY SCRIPTS
    #=====================================================
    print("Copying util/scripts to {}/bin...".format(install_path))
    for item in os.listdir('./util/scripts'):
        s = os.path.join('./util/scripts/', item)
        d = os.path.join(os.path.join(install_path, 'bin'), item)
        shutil.copy(s, d)

def build_namespace(base_path):
    print("Building namespace tools...")
    try:
        make_cmd = "make -C {}/tool/namespace".format(base_path)
        rv = subprocess.check_call(["make", "-C",
                                    base_path + "/tool/namespace"])
    except Exception as e:
        print('Could not build namespace tools ({})'.format(e))
    else:
        if rv != 0:
            print('make failed')

def main(args):

    cpus = multiprocessing.cpu_count()

    if not args.skip_llvm_clang_install:
        install_clang_llvm(args.base_path, args.install_path, cpus,
                           args.llvm_targets)

    if not args.skip_binutils_install:
        install_binutils(args.base_path, args.install_path, cpus)

    if not args.skip_libraries_install:
        install_libraries(args.base_path, args.install_path,
                          args.install_targets, cpus,
                          args.debug_stack_transformation,
                          args.libmigration_type,
                          args.enable_libmigration_timing)

    if not args.skip_tools_install:
        install_tools(args.base_path, args.install_path, cpus)

    if args.install_call_info_library:
        install_call_info_library(args.base_path, args.install_path,
                                  cpus)

    if not args.skip_utils_install:
        install_utils(args.base_path, args.install_path, cpus)

    if not args.skip_namespace:
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
