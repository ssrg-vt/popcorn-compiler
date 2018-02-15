''' APIs for configuring & running Popcorn VMs. '''

import utils
import kernel
from time import sleep

class VM(utils.Prereqs):
    ''' Manage virtual machine execution '''

    ''' VM states '''
    Offline = 0
    Booting = 1
    Running = 2

    @classmethod
    def getArch(cls):
        ''' Get the name of architecture for the VM '''
        return "none"

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
        ''' A VM subprocess maintained in another terminal. '''

        def __init__(self, process, tty):
            super().__init__(process)
            self.io = tty

        def alive(self):
            return self.process.is_alive()

        def join(self):
            if self.process.join(5) or not self.process.exitcode:
                utils.warn("could not join subprocess")

    class WindowedVM(VMProcess):
        ''' A VM subprocess maintained in a separate window. '''

        def __init__(self, process):
            super().__init__(process)

        def alive(self):
            return self.process.poll() == None

        def join(self):
            try: self.process.wait(5)
            except Exception as e:
                utils.warn("could not join subprocess")

    class VMBootException(Exception):
        ''' Exception for when a VM does not boot correctly. '''
        def __init__(self, message):
            self.message = message

        def __str__(self):
            return "VM did not boot -- {}".format(self.message)

    def __init__(self, cpu, mem, graphics, drive, kernel, kernelArgs, ip,
                 windowed):
        self.checkPrereqs()
        self.cpu = cpu
        self.mem = mem
        self.graphics = graphics
        self.drive = drive
        self.kernel = utils.sanitizeFile(kernel)
        self.kernelArgs = kernelArgs
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

    def getRunArguments(self):
        ''' Get arguments needed to run the VM. '''
        args = [ "-cpu", self.cpu,
                 "-m", str(self.mem),
                 "-drive", self.drive,
                 "-kernel", self.kernel,
                 "-append", self.kernelArgs ]
        if not self.graphics: args.append("-nographic")
        return args

    def ping(self):
        ''' Ping the VM to see if it's responding to network requests.  Return
            true if so, or false otherwise.
        '''
        ping = ["ping", "-c", "1", "-W", "1", self.ip]
        if utils.runCmd(ping, wait=True) == 0: return True
        else: return False

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
            subprocess and manage I/O through a separate terminal.

            Note: currently windowed is the only supported method!
        '''
        if self.windowed:
            fullcmd = "sudo"
            for arg in self.getRunCommand(): fullcmd += " " + arg
            args = [ "xterm", "-hold", "-e", fullcmd ]
            self.process = VM.WindowedVM(utils.runCmd(args))
        else:
            assert False, "Not yet supported"
        self.state = VM.Booting

    def waitUntilVMBoots(self, timeout=60):
        ''' Wait for the VM to boot.  Raise a VMBootException if the VM didn't
            boot correctly or is not responding to requests after the timeout
            expires.
        '''
        # Check if the child process is alive & responding to pings for up to
        # 60 seconds (ping will wait up to a second each time)
        curWait = 0
        while not self.alive() and curWait < timeout: curWait += 1

        if curWait >= timeout:
            message = "not reachable after {} seconds".format(timeout)
            raise VM.VMBootException(message)
        elif not self.process.alive():
            raise VM.VMBootException("the VM child process is not running")
        else: self.state = VM.Running

    def cmd(self, cmd):
        ''' Run a command on the VM and return the output. '''
        if self.state != VM.Running:
            utils.warn("cannot run command -- VM is not running")
            return None

        run = [ "ssh", "popcorn@{}".format(self.ip), cmd ]
        return utils.getCommandOutput(run)

    def sendFile(self, name, dest):
        ''' Send a file to the VM. Return true if the file was sent correctly
            or false otherwise.
        '''
        if self.state != VM.Running:
            utils.warn("cannot run command -- VM is not running")
            return False

        name = utils.sanitizeFile(name, True)
        dest = utils.sanitizeFile(dest)
        scp = [ "scp", name, "popcorn@{}:{}".format(self.ip, dest) ]
        if utils.runCmd(scp, wait=True) != 0:
            warn("Could not send file '{}' to VM".format(name))
            return False
        return True

    def stopVM(self):
        if self.state == VM.Booting:
            self.process.terminate()
            self.process.join()
        elif self.state == VM.Running:
            self.cmd("sudo shutdown -h now")
            sleep(5) # TODO detect when subprocess exits?
            self.process.terminate()
            self.process.join()
        self.state = VM.Offline

    def __str__(self):
        return "generic VM ({})".format(self.getVMState())

class ARM64(VM):
    ''' An ARM64 virtual machine '''
    def __init__(self, cpu="cortex-a57", mem=4096, graphics=False, image="arm.img",
                 kernel="linux-arm/arch/arm64/boot/Image", ip="10.1.1.253",
                 machine="virt", windowed=True):
        self.image = utils.sanitizeFile(image)
        drive = "id=root,if=none,media=disk,file={}".format(self.image)
        kernelArgs = "\"root=/dev/vda console=ttyAMA0\""
        super().__init__(cpu, mem, graphics, drive, kernel, kernelArgs, ip,
                         windowed)
        self.machine = machine
        self.devices = [ "virtio-blk-device,drive=root",
                         "virtio-net-device,netdev=net0,mac=00:da:bc:de:02:11" ]
        self.netDevices = [ "type=tap,id=net0" ]

    @classmethod
    def getArch(cls):
        ''' Get the architecture name for the VM '''
        return "aarch64"

    @classmethod
    def getQEMU(cls): return "qemu-system-aarch64"

    def getPrereqs(self): return [ self.getQEMU() ]

    def getRunCommand(self):
        args = [ self.getQEMU() ]
        args += super().getRunArguments()
        args.append("-machine")
        args.append(self.machine)
        for device in self.devices:
            args.append("-device")
            args.append(device)
        for device in self.netDevices:
            args.append("-netdev")
            args.append(device)
        return args

    def __str__(self):
        return "ARM64 VM ({})".format(self.getVMState())

class X86(VM):
    ''' An x86-64 virtual machine '''
    def __init__(self, cpu="host", mem=4096, graphics=False, image="x86.img",
                 kernel="linux-x86/arch/x86/boot/bzImage", ip="10.1.1.254",
                 enableKVM=True, smp=2, reboot=False, windowed=True):
        self.image = utils.sanitizeFile(image)
        drive = "id=root,media=disk,file={}".format(self.image)
        kernelArgs = "\"root=/dev/sda1 console=ttyS0\""
        super().__init__(cpu, mem, graphics, drive, kernel, kernelArgs, ip,
                         windowed)
        self.enableKVM = enableKVM
        self.smp = smp
        self.reboot = reboot
        self.net = [ "nic,macaddr=00:da:bc:de:00:13", "tap" ]

    @classmethod
    def getArch(cls):
        ''' Get the architecture name for the VM '''
        return "x86_64"

    @classmethod
    def getQEMU(cls): return "qemu-system-x86_64"

    def getPrereqs(self): return [ self.getQEMU() ]

    def getRunCommand(self):
        args = [ self.getQEMU() ]
        args += super().getRunArguments()
        if self.enableKVM: args.append("-enable-kvm")
        args.append("-smp")
        args.append(str(self.smp))
        if not self.reboot: args.append("-no-reboot")
        for device in self.net:
            args.append("-net")
            args.append(device)
        return args

    def __str__(self):
        return "x86-64 VM ({})".format(self.getVMState())

def runX86VM(windowed=True, wait=True):
    ''' Create & start an x86 VM.  Return the VM handle. '''
    kernels = kernel.Kernel()
    x86 = X86(kernel=kernels["x86_64"], windowed=windowed)
    x86.startVM()
    if wait: x86.waitUntilVMBoots()
    return x86

def runARM64VM(windowed=True, wait=True):
    ''' Create & start an x86 VM.  Return the VM handle. '''
    kernels = kernel.Kernel()
    arm = ARM64(kernel=kernels["aarch64"], windowed=windowed)
    arm.startVM()
    if wait: arm.waitUntilVMBoots()
    return arm

