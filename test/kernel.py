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
        self.targetMsgLayer = {}

    def getPrereqs(self):
        prereqs = []
        for target in utils.getTargets():
            prereqs.append("{}-linux-gnu-gcc".format(target))
        return prereqs

    def getRepoName(self, target):
        ''' Return the name of the kernel repository for an architecture. '''
        return self.repo.name + "." + target

    @classmethod
    def getImageName(cls, target):
        ''' Return the name of the kernel image file for an architecture. '''
        linuxArch = utils.getLinuxTarget(target)
        imgname = cls.Image[target]
        return os.path.join("arch", linuxArch, "boot", imgname)

    @classmethod
    def getMsgLayerModuleName(cls):
        ''' Return the name of the messaging layer module file. '''
        return os.path.join("msg_layer", "msg_socket.ko")

    def checkForRepo(self, target):
        ''' Set the kernel repository for an architecture if it exists.  Return
            true if it exists or false if it needs to be cloned.
        '''
        if target in self.targetRepo: return True
        else:
            repoName = utils.sanitizeDir(self.getRepoName(target))
            if os.path.isdir(repoName):
                self.targetRepo[target] = repoName
                return True
        return False

    def checkForKernelImage(self, target):
        ''' Set the kernel image for an architecture if it exists.  Return true
            if it exists or false if it needs to be built.
        '''
        if target in self.targetImg: return True
        elif target in self.targetRepo:
            img = os.path.join(self.targetRepo[target],
                               self.getImageName(target))
            if os.path.isfile(img):
                self.targetImg[target] = img
                return True
        return False

    def checkForMsgLayerModule(self, target):
        ''' Set the message layer module for an architecture.  Return true if it
            exists or false if it needs to be built.
        '''
        if target in self.targetMsgLayer: return True
        elif target in self.targetRepo:
            module = os.path.join(self.targetRepo[target],
                                  self.getMsgLayerModuleName())
            if os.path.isfile(module):
                self.targetMsgLayer[target] = module
                return True
        return False

    def cloneRepositoryForTarget(self, target):
        ''' Clone the repository for a target architecture. '''
        print("-> CLONING THE POPCORN KERNEL REPO FOR {} <-".format(target))
        destination = utils.sanitizeDir(self.getRepoName(target))
        self.targetRepo[target] = destination
        self.repo.cloneRepo(destination)

    def buildKernelForTarget(self, target):
        ''' Build the Popcorn kernel for a target architecture. '''
        print("-> BUILDING POPCORN KERNEL FOR {} <-".format(target))
        assert target in self.targetRepo, "Need to clone repo before building"
        repo = self.targetRepo[target]

        # Set the correct configuration for the target
        archConfig = os.path.join(repo, self.Config[target])
        archConfig = utils.sanitizeFile(archConfig, True)
        buildConfig = utils.sanitizeFile(os.path.join(repo, ".config"))
        shutil.copy(archConfig, buildConfig)

        # Build the kernel
        theEnv = os.environ.copy()
        theEnv["ARCH"] = utils.getLinuxTarget(target)
        buildArgs = [ "make", "-C", repo, "-j", str(utils.cpus()) ]
        utils.runCmd(buildArgs, wait=True, interactive=True, environment=theEnv)

        # Record the kernel image filename
        img = os.path.join(repo, self.getImageName(target))
        img = utils.sanitizeFile(img, True)
        self.targetImg[target] = img

    def buildMsgLayerForTarget(self, target):
        ''' Build the Popcorn kernel message layer for a target architecture.
        '''
        print("-> BUILDING MESSAGING LAYER FOR {} <-".format(target))
        assert target in self.targetRepo, "Need to clone repo before building"
        repo = utils.sanitizeDir(self.targetRepo[target])

        # Build the module
        subdir = os.path.join(repo, "msg_layer")
        theEnv = os.environ.copy()
        theEnv["ARCH"] = utils.getLinuxTarget(target)
        buildArgs = [ "make", "-C", subdir, "-j", str(utils.cpus()) ]
        utils.runCmd(buildArgs, wait=True, interactive=True, environment=theEnv)

        # Record the module filename
        module = os.path.join(repo, self.getMsgLayerModuleName())
        module = utils.sanitizeFile(module, True)
        self.targetMsgLayer[target] = module

    def initializeTarget(self, target, forceCompile=False):
        ''' Initialize kernel information for a target. If not available,
            clone the repository/build the kernel & messaging layer.
        '''
        haveRepo = self.checkForRepo(target)
        haveImg = self.checkForKernelImage(target)
        haveModule = self.checkForMsgLayerModule(target)

        if not haveRepo: self.cloneRepositoryForTarget(target)
        if not haveImg or forceCompile: self.buildKernelForTarget(target)
        if not haveModule or forceCompile: self.buildMsgLayerForTarget(target)

    def initializeAllTargets(self, forceCompile=False):
        ''' Initialize all targets. '''
        for target in utils.getTargets(): initializeTarget(target, forceCompile)

    def __getitem__(self, target):
        ''' Return a dictionary containing the kernel image ("kernel") and
            messaging layer module ("msglayer") for an architecture.  If either
            is not available, build them.
        '''
        if target not in self.targetImg or target not in self.targetMsgLayer:
            self.initializeTarget(target)
        return { "kernel" : self.targetImg[target],
                 "msglayer" : self.targetMsgLayer[target] }

    def getKernelImage(self, target):
        ''' Return the kernel image for an architecture.  If not available,
            build it.
        '''
        if target not in self.targetImg: self.initializeTarget(target)
        return self.targetImg[target]

    def getMsgLayerModule(self, target):
        ''' Return the messaging layer module for an architecture. If not
            available, build it.
        '''
        if target not in self.targetMsgLayer: self.initializeTarget(target)
        return self.targetMsgLayer[target]

