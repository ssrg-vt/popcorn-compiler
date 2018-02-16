''' APIs for configuring & running Popcorn VMs. '''

import utils
import kernel
from time import sleep
import multiprocessing
import subprocess
import os

class VM(utils.Prereqs):
    ''' Manage virtual machine execution '''

    ''' VM states '''
    Offline = 0
    Booting = 1
    Running = 2

    # VM process containers

    class VMProcess:
        ''' A VM subprocess.  Takes different shapes depending on how the VM is
            started.

            Child classes must implement the following APIs:
                alive : return true if the process is still alive or false
                        otherwise
                join  : join the VM's process, return true if successfully
                        joined or false otherwise
        '''

        def __init__(self, process):
            self.process = process

        def pid(self):
            ''' Return the process' PID '''
            return self.process.pid

        def terminate(self):
            ''' Terminate the process by sending SIGTERM '''
            self.process.terminate()

        def alive(self):
            ''' Return true if the process is still alive or false otherwise.
            '''
            assert False, "Must be implemented in sub-class"

        def join(self):
            ''' Join the VM's process, return true if successfully joined or
                false otherwise.
            '''
            assert False, "Must be implemented in sub-class"

    class TerminalVM(VMProcess):
        ''' A VM subprocess maintained in another terminal.  Process is a
            multiprocessing.Process object.
        '''

        def __init__(self, process, tty):
            assert type(process) == multiprocessing.Process, \
                   "Invalid process type"
            super().__init__(process)
            self.io = tty

        def alive(self):
            return self.process.is_alive()

        def join(self):
            if self.process.join(5) or not self.process.exitcode:
                utils.warn("could not join subprocess")

    class WindowedVM(VMProcess):
        ''' A VM subprocess maintained in a separate window.  Process is a
            subprocess.Popen object.
        '''

        def __init__(self, process):
            assert type(process) == subprocess.Popen, "Invalid process type"
            super().__init__(process)

        def alive(self):
            return self.process.poll() == None

        def join(self):
            try: self.process.wait(5)
            except subprocess.TimeoutExpired as e:
                utils.warn("could not join subprocess")

    # VM exceptions

    class VMBootException(Exception):
        ''' Exception for when a VM does not boot correctly. '''
        def __init__(self, message):
            self.message = message

        def __str__(self):
            return "VM did not boot -- {}".format(self.message)

    class VMCommandException(Exception):
        ''' Exception for when a command cannot complete on the VM. '''
        def __init__(self, message, command):
            self.message = message
            self.command = command

        def __str__(self):
            fullCommand = " ".join(self.command)
            return "could not execute command '{}' -- {}" \
                   .format(fullCommand, self.message)

    # VM object methods

    def __init__(self, cpu, mem, graphics, drive, kernel, kernelArgs,
                 msgLayer, ip, windowed):
        self.checkPrereqs()
        self.cpu = cpu
        self.mem = mem
        self.graphics = graphics
        self.drive = drive
        self.kernel = utils.sanitizeFile(kernel, True)
        self.kernelArgs = kernelArgs
        self.msgLayer = msgLayer
        self.ip = ip
        self.windowed = windowed
        self.state = VM.Offline

    def __del__(self):
        ''' Stop the VM when the object is destroyed. '''
        # TODO this is known to throw exceptions and not clean up correctly
        # when the object is being deleted during interpreter shutdown
        self.stopVM()

    def __enter__(self):
        ''' Context syntactic sugar. '''
        self.startVM()
        self.waitUntilVMBoots()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        ''' Context syntactic sugar. '''
        self.stopVM()

    def __str__(self):
        return "{} VM ({})".format(self.getArch(), self.getVMState())

    # VM property query APIs

    @classmethod
    def getArch(cls):
        ''' Get the name of the architecture for the VM. '''
        assert False, "Must be implemented in sub-class"

    @classmethod
    def getQEMU(cls): return "qemu-system-{}".format(cls.getArch())

    def getPrereqs(self): return [ self.getQEMU(), "ssh", "scp" ]

    def getRunCommand(self):
        ''' Get arguments needed to run the VM. '''
        args = [ "-cpu", self.cpu,
                 "-m", str(self.mem),
                 "-drive", self.drive,
                 "-kernel", self.kernel,
                 "-append", self.kernelArgs ]
        if not self.graphics: args.append("-nographic")
        return args

    # VM health APIs

    def ping(self):
        ''' Ping the VM to see if it's responding to network requests.  Return
            true if so, or false otherwise.
        '''
        ping = ["ping", "-c", "1", "-W", "1", self.ip]
        try:
            utils.runCmd(ping, wait=True)
            return True
        except subprocess.CalledProcessError as e: return False

    def alive(self):
        ''' Return true if the VM is alive and responding, or false otherwise.
        '''
        return self.process.alive() and self.ping()

    # VM management APIs

    def getVMState(self):
        ''' Get a human-readable string describing the VM state. '''
        if self.state == VM.Offline: return "offline"
        elif self.state == VM.Booting: return "booting"
        elif self.state == VM.Running: return "running"
        else: return "unknown"

    def startVM(self):
        ''' Start the VM.  If initialized with windowed=True, open an xterm
            window in which to run the VM.  Otherwise, start the VM in a
            subprocess and manage I/O through a separate terminal.  Raises a
            VMBootException if the boot command couldn't be executed.

            Note: currently windowed is the only supported method!
        '''
        if self.windowed:
            try:
                fullcmd = " ".join([ "sudo" ] + self.getRunCommand())
                args = [ "xterm", "-hold", "-e", fullcmd ]
                self.process = VM.WindowedVM(utils.runCmd(args))
            except Exception as e:
                message = "could not start windowed VM -- {}".format(e)
                raise VM.VMBootException(message)
        else:
            assert False, "Not yet supported"
        self.state = VM.Booting

    def waitUntilVMBoots(self, timeout=60):
        ''' Wait for the VM to boot.  Raise a VMBootException if the VM didn't
            boot correctly or is not responding to requests after the timeout
            expires.
        '''
        # Check if the child process is alive & responding to pings for up to
        # timeout seconds (ping will wait up to a second each time)
        curWait = 0
        while not self.alive() and curWait < timeout: curWait += 1

        if curWait >= timeout:
            message = "not reachable after {} seconds".format(timeout)
            raise VM.VMBootException(message)
        if not self.process.alive():
            raise VM.VMBootException("the child process is not running")
        else: self.state = VM.Running

    def cmd(self, cmd):
        ''' Run a command on the VM and return the output.  If the command fails
            or returns non-zero, return None.
        '''
        if self.state != VM.Running:
            utils.warn("cannot run command -- VM is not running " \
                       "(did you call waitUntilVMBoots()?)")
            return None

        try:
            run = [ "ssh", "popcorn@{}".format(self.ip), cmd ]
            return utils.getCommandOutput(run)
        except Exception as e: return None

    def sendFile(self, name, dest):
        ''' Send a file to the VM.  Raises a VMCommandException if not
            successfully copied to the VM.
        '''
        if self.state != VM.Running:
            utils.warn("cannot run command -- VM is not running " \
                       "(did you call waitUntilVMBoots()?)")
        else:
            name = utils.sanitizeFile(name, True)
            dest = utils.sanitizeFile(dest)
            scp = [ "scp", name, "popcorn@{}:{}".format(self.ip, dest) ]
            try: utils.runCmd(scp, wait=True)
            except subprocess.CalledProcessError as e:
                raise VM.VMCommandException("could not copy file", scp)

    def startMessagingLayer(self):
        dest = os.path.join("/home/popcorn", os.path.basename(self.msgLayer))
        dest = utils.sanitizeDir(dest)
        self.sendFile(self.msgLayer, dest)
        self.cmd("sudo insmod {}".format(dest))

    def stopVM(self, force=False):
        ''' Shut down the VM and terminate the underlying process. '''
        if self.state == VM.Booting or force:
            self.process.terminate()
            self.process.join()
        elif self.state == VM.Running:
            self.cmd("sudo shutdown -h now")
            sleep(5) # TODO detect when subprocess exits?
            self.process.terminate()
            self.process.join()
        self.state = VM.Offline

class ARM64(VM):
    ''' An ARM64 virtual machine '''
    def __init__(self, cpu="cortex-a57", mem=4096, graphics=False,
                 image="arm.img", kernel="linux-arm/arch/arm64/boot/Image",
                 msgLayer="linux-arm/msg_layer/msg_socket.ko",
                 ip="10.1.1.253", windowed=True, machine="virt"):
        self.image = utils.sanitizeFile(image, True)
        drive = "id=root,if=none,media=disk,file={}".format(self.image)
        kernelArgs = "\"root=/dev/vda console=ttyAMA0\""
        super().__init__(cpu, mem, graphics, drive, kernel, kernelArgs,
                         msgLayer, ip, windowed)
        self.machine = machine
        self.devices = [ "virtio-blk-device,drive=root",
                         "virtio-net-device,netdev=net0,mac=00:da:bc:de:02:11" ]
        self.netDevices = [ "type=tap,id=net0" ]

    @classmethod
    def getArch(cls): return "aarch64"

    def getRunCommand(self):
        args = [ self.getQEMU() ]
        args += super().getRunCommand()
        args.append("-machine")
        args.append(self.machine)
        for device in self.devices:
            args.append("-device")
            args.append(device)
        for device in self.netDevices:
            args.append("-netdev")
            args.append(device)
        return args

class X86(VM):
    ''' An x86-64 virtual machine '''
    def __init__(self, cpu="host", mem=4096, graphics=False, image="x86.img",
                 kernel="linux-x86/arch/x86/boot/bzImage",
                 msgLayer="linux-x86/msg_layer/msg_socket.ko",
                 ip="10.1.1.254", windowed=True, enableKVM=True, smp=2,
                 reboot=False):
        self.image = utils.sanitizeFile(image, True)
        drive = "id=root,media=disk,file={}".format(self.image)
        kernelArgs = "\"root=/dev/sda1 console=ttyS0\""
        super().__init__(cpu, mem, graphics, drive, kernel, kernelArgs,
                         msgLayer, ip, windowed)
        self.enableKVM = enableKVM
        self.smp = smp
        self.reboot = reboot
        self.net = [ "nic,macaddr=00:da:bc:de:00:13", "tap" ]

    @classmethod
    def getArch(cls): return "x86_64"

    def getRunCommand(self):
        args = [ self.getQEMU() ]
        args += super().getRunCommand()
        if self.enableKVM: args.append("-enable-kvm")
        args.append("-smp")
        args.append(str(self.smp))
        if not self.reboot: args.append("-no-reboot")
        for device in self.net:
            args.append("-net")
            args.append(device)
        return args

def runX86VM(windowed=True, wait=True):
    ''' Create/start an x86-64 VM.  Return VM handle & kernel information. '''
    x86Info = kernel.Kernel()["x86_64"]
    x86 = X86(kernel=x86Info["kernel"], msgLayer=x86Info["msglayer"],
              windowed=windowed)
    x86.startVM()
    if wait: x86.waitUntilVMBoots()
    return x86

def runARMVM(windowed=True, wait=True):
    ''' Create/start an ARM64 VM.  Return VM handle & kernel information. '''
    armInfo = kernel.Kernel()["aarch64"]
    arm = ARM64(kernel=armInfo["kernel"], msgLayer=armInfo["msglayer"],
                windowed=windowed)
    arm.startVM()
    if wait: arm.waitUntilVMBoots()
    return arm

def setupTestEnvironment(windowed=True):
    ''' Start VMs for all architectures.  Set up the messaging layer and return
        the started VMs.
    '''
    def loadMsgLayer(vm): vm.startMessagingLayer()

    arm = runARMVM(windowed, True)
    x86 = runX86VM(windowed, True)
    armLoader = multiprocessing.Process(target=loadMsgLayer, args=(arm,))
    x86Loader = multiprocessing.Process(target=loadMsgLayer, args=(x86,))
    armLoader.start()
    x86Loader.start()
    armLoader.join()
    x86Loader.join()

    return arm, x86

