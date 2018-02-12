''' Utilities for downloading & building the Popcorn kernel. '''

import os
import errno
import shutil
import utils
import repository

class Kernel(utils.Prereqs):
    ''' Download & build the kernel for all available targets. '''

    Config = { "aarch64" : ".config-qemu-arm64",
               "x86_64" : ".config-qemu-x86_64" }
    Image = { "aarch64" : "Image", "x86_64" : "bzImage" }

    def __init__(self):
        self.checkPrereqs()
        self.repo = repository.getPopcornKernelRepo()
        self.targetRepo = {}
        self.targetImg = {}

    def getPrereqs(self):
        prereqs = []
        for target in utils.getTargets():
            prereqs.append("{}-linux-gnu-gcc".format(target))
        return prereqs

    @classmethod
    def getImageName(cls, target):
        ''' Return the name of the kernel image file for an architecture.  This
            should be appended to the kernel repository's directory name.
        '''
        linuxArch = utils.getLinuxTarget(target)
        imgname = cls.Image[target]
        return os.path.join("arch", linuxArch, "boot", imgname)

    def getRepoName(self, target):
        ''' Return the name of the kernel repository for an architecture. '''
        return self.repo.name + "." + target

    def buildKernelForTarget(self, target):
        ''' Build the Popcorn kernel for a target architecture. '''
        print("-> BUILDING POPCORN KERNEL FOR {} <-".format(target))

        # Clone the repository
        destination = utils.sanitizeDir(self.getRepoName(target))
        self.targetRepo[target] = destination
        self.repo.cloneRepo(destination)

        # Prepare for building
        archConfig = os.path.join(destination, self.Config[target])
        archConfig = utils.sanitizeFile(archConfig, True)
        buildConfig = utils.sanitizeFile(os.path.join(destination, ".config"))
        shutil.copy(archConfig, buildConfig)

        # Build the kernel
        theEnv = os.environ.copy()
        theEnv["ARCH"] = utils.getLinuxTarget(target)
        buildArgs = [ "make", "-C", destination, "-j", str(utils.cpus()) ]
        utils.runCmd(buildArgs, wait=True, output=True, environment=theEnv)

        # Record the kernel image filename
        theImg = os.path.join(destination, self.getImageName(target))
        if not os.path.isfile(theImg):
            raise FileNotFoundError(errno.ENOENT,
                                    os.strerror(errno.ENOENT),
                                    theImg)
        self.targetImg[target] = theImg

    def buildKernelForAllTargets(self, forceCompile=False):
        ''' Build the Popcorn kernel for all architectures, if necessary.  If
            forceCompile is set, re-build the kernel regardless if the image
            file exists.
        '''
        for target in utils.getTargets():
            img = utils.sanitizeDir(os.path.join(self.getRepoName(target),
                                                 self.getImageName(target)))
            if not os.path.isfile(img) or forceCompile:
                self.buildKernelForTarget(target)
            else:
                self.targetImg[target] = img
                print("Found {} kernel image '{}'".format(target, img))

    def getKernelImage(self, target):
        ''' Return the kernel image for an architecture.  If not available,
            build the kernel.
        '''
        if target not in self.targetImg: self.buildKernelForTarget(target)
        return self.targetImg[target]

