''' Tools for running the test suite. '''

import sys
import os
from os import path
import shutil
import subprocess
import multiprocessing
import platform
import socket

###############################################################################
# Interfaces
###############################################################################

class Prereqs:
    ''' Preqequisite checking superclass. Sub-classes should override
        getPrereqs() to return a list of programs needed to implement
        functionality.  Additionally, programs should call checkPrereqs() in
        their __init__() to ensure the programs are available.
    '''

    def getPrereqs(self):
        ''' Return a list of prerequisite programs. '''
        raise NotImplementedError("Prerequisites not supplied")

    def checkPrereqs(self):
        ''' Check for existence of prerequisites returned by getPrereqs() '''
        prereqs = self.getPrereqs()
        for prereq in prereqs:
            if shutil.which(prereq) == None:
                die("Could not find prerequisite '{}'".format(prereq))

###############################################################################
# Targets
###############################################################################

''' Architecture prefixes supported by the toolchain '''
Targets = [ "aarch64", "x86_64" ]

''' Linux's name for the targets (should be 1-to-1 with Targets) '''
LinuxTargets = [ "arm64", "x86" ]

''' LLVM's names for the targets (should be 1-to-1 with Targets) '''
LLVMTargets = [ "AArch64", "X86" ]

assert len(Targets) == len(LinuxTargets) and \
       len(Targets) == len(LLVMTargets), "Mismatch between prefix/LLVM targets"

def getTargets():
    ''' Return a list of targets '''
    global Targets
    return Targets

def getLinuxTarget(target):
    ''' Return Linux's name for a target '''
    global Targets
    global LinuxTargets
    assert target in Targets, "Invalid target '{}'".format(target)
    return LinuxTargets[Targets.index(target)]

def getLinuxTargets():
    ''' Return a list of prefix/Linux architecture name tuples '''
    global Targets
    global LinuxTargets
    return zip(Targets, LinuxTargets)

def getLLVMTarget(target):
    ''' Return LLVM's name for a target '''
    global Targets
    global LLVMTargets
    assert target in Targets, "Invalid target '{}'".format(target)
    return LLVMTargets[Targets.index(target)]

def getLLVMTargets():
    ''' Return a list of prefix/LLVM architecture name tuples '''
    global Targets
    global LLVMTargets
    return zip(Targets, LLVMTargets)

###############################################################################
# Convenience functions
###############################################################################

def die(msg, code=-1):
    ''' Print message & exit with an error code. Note this should *only* be
        called for catastrophic errors, *not* test failures.  This doesn't give
        us a good mechanism for detecting what failed as it simply force-quits.
    '''
    print("ERROR: {}".format(msg))
    if code == 0: code = -1 # Ensure we're signaling an error
    sys.exit(code)

def warn(msg):
    ''' Print warning message. '''
    print("WARNING: {}".format(msg))

def runCmd(args, wait=False, output=False, environment=None):
    ''' Run a command.  The function returns different values based on whether
        wait=True or wait=False.

        wait=True:  wait for the process to exit and return it's code.  Raises
                    a subprocess.CalledProcessError if the executable returns
                    non-zero.  Use this to run tests and catch failures.

        wait=False: don't wait for the process to finish, but instead return
                    a handle for the underlying process.  The user is
                    responsible for reaping the child, checking error codes,
                    etc.

        Additionally, raises a FileNotFoundException if the executable is not
        found in the user's path.
     '''
    if not environment: environment = os.environ
    if output: out = None
    else: out = subprocess.PIPE

    if wait: return subprocess.check_call(args, env=environment,  stdout=out,
                                          stderr=subprocess.STDOUT)
    else: return subprocess.Popen(args, env=environment, stdout=out,
                                  stderr=subprocess.STDOUT)

def getCommandOutput(args):
    ''' Run a command and return any output. If the process doesn't exist or
        returns non-zero, return None.
    '''
    try:
        out = subprocess.check_output(args, stderr=subprocess.PIPE)
    except subprocess.CalledProcessError as e:
        return None

    return out.decode("utf-8")

###############################################################################
# Platform
###############################################################################

def arch():
    ''' Return the architecture of the current system. '''
    return platform.machine()

def cpus():
    ''' Return the number of CPUs in the current system. '''
    return multiprocessing.cpu_count()

def __parseCPUInfo():
    ''' Parse /proc/cpuinfo and return a list of per-core information. '''
    cores = []
    with open("/proc/cpuinfo", 'r') as info:
        for line in info:
            if "processor" in line:
                curCore = {}
                cores.append(curCore)
            elif "model name" in line: curCore["name"] = line.split(3)[3]
            elif "cpu MHz" in line: curCore["clock"] = line.split(3)[3]
    return cores

def speed(core=0):
    ''' Return clock speed in MHz for the specified processor core. '''
    cores = __parseCPUInfo()
    return cores[core]["clock"]

def ipAddr():
    ''' Return the IP address of the machine.  Finds the first
        internet-connected IP address.
    '''
    # TODO is there a better way than just connecting and seeing which IP
    # address the system picks?
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.connect(("8.8.8.8", 80))
    address = s.getsockname()
    s.close()
    return address[0]

###############################################################################
# Filesystem
###############################################################################

def sanitizeDir(dirPath, checkExists=False):
    ''' Convert a directory's path to an absolute path.  Optionally verify if
        the directory already exists.
    '''
    sanitized = path.abspath(dirPath)
    if checkExists:
        if not path.isdir(sanitized):
            die("directory '{}' does not exist".format(sanitized))
    return sanitized

def sanitizeFile(filePath, checkExists=False):
    ''' Convert a file's path to an absolute path.  Optionally verify if the
        file already exists.
    '''
    sanitized = path.abspath(filePath)
    if checkExists:
        if not path.isfile(sanitized):
            die("file '{}' does not exist".format(sanitized))
    return sanitized

def createBackup(filePath):
    ''' Create a backup of a file '''
    backup = sanitizeFile(filePath, checkExists=True) + ".bak"
    shutil.copy(filePath, backup)

