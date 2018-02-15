''' APIs for configuring & running Popcorn VMs. '''

import utils
import kernel

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
                join : join the VM's process, return true if successfully
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

    class TerminalVM(VMProcess):
        ''' A VM subprocess maintained in another terminal. '''

        def __init__(self, process, tty):
            super().__init__(process)
            self.io = tty

        def join(self):
            if self.process.join(5) or not self.process.exitcode:
                utils.warn("could not join subprocess")

    class WindowedVM(VMProcess):
        ''' A VM subprocess maintained in a separate window. '''

        def __init__(self, process):
            super().__init__(process)

        def join(self):
            try: self.process.wait(5)
            except Exception as e:
                utils.warn("could not join subprocess")

    def __init__(self, cpu, mem, graphics, drive, kernel, kernelArgs):
        self.checkPrereqs()
        self.cpu = cpu
        self.mem = mem
        self.graphics = graphics
        self.drive = drive
        self.kernel = utils.sanitizeFile(kernel)
        self.kernelArgs = kernelArgs
        self.state = VM.Offline

    def getVMState(self):
        if self.state == VM.Offline: return "offline"
        elif self.state == VM.Booting: return "booting"
        elif self.state == VM.Running: return "running"
        else: return "unknown"

    def getRunArguments(self):
        ''' Generate common arguments needed to run the VM. '''
        args = [ "-cpu", self.cpu,
                 "-m", str(self.mem),
                 "-drive", self.drive,
                 "-kernel", self.kernel,
                 "-append", self.kernelArgs ]
        if not self.graphics: args.append("-nographic")
        return args

    # VM management APIs

    def startVM(self, windowed=False):
        if windowed:
            fullcmd = "sudo"
            for arg in self.getRunCommand(): fullcmd += " " + arg
            args = [ "xterm", "-hold", "-e", fullcmd ]
            self.process = VM.WindowedVM(utils.runCmd(args))
        else:
            assert False, "Not yet supported"
        self.state = VM.Booting

    def poll(self):
        assert False, "Not yet implemented!"
        # TODO verify this works -- need to set up network bridge so we can
        # ping/log in!

    def cmd(self, args):
        if self.state != VM.Running:
            utils.warn("cannot run command -- VM is not running")
            return
        # TODO implement

    def waitUntilVMBoots(self):
        assert False, "Not yet implemented!"
        # TODO implement

    def stopVM(self):
        if self.state == VM.Booting:
            self.process.terminate()

    def __str__(self):
        return "generic VM ({})".format(self.getVMState())

class ARM64(VM):
    ''' An ARM64 virtual machine '''
    def __init__(self, cpu="cortex-a57", mem=4096, graphics=False, image="arm.img",
                 kernel="linux-arm/arch/arm64/boot/Image", machine="virt"):
        self.image = utils.sanitizeFile(image)
        drive = "id=root,if=none,media=disk,file={}".format(self.image)
        kernelArgs = "\"root=/dev/vda console=ttyAMA0\""
        super().__init__(cpu, mem, graphics, drive, kernel, kernelArgs)
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
                 kernel="linux-x86/arch/x86/boot/bzImage", enableKVM=True,
                 smp=2, reboot=False):
        self.image = utils.sanitizeFile(image)
        drive = "id=root,media=disk,file={}".format(self.image)
        kernelArgs = "\"root=/dev/sda1 console=ttyS0\""
        super().__init__(cpu, mem, graphics, drive, kernel, kernelArgs)
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

def runX86VM(windowed):
    ''' Create & start an x86 VM.  Return the VM handle. '''
    kernels = kernel.Kernel()
    x86 = X86(kernel=kernels["x86_64"])
    x86.startVM(windowed)
    return x86

def runARM64VM(windowed):
    ''' Create & start an x86 VM.  Return the VM handle. '''
    kernels = kernel.Kernel()
    arm = ARM64(kernel=kernels["aarch64"])
    arm.startVM(windowed)
    return arm

