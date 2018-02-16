''' Utilites for managing Popcorn git repositories. '''

from os import path
import utils

class GitRepo(utils.Prereqs):
    def __init__(self, url):
        self.checkPrereqs()
        if url[-4:] != ".git":
            utils.die("Invalid git repository url '{}'".format(url))
        self.url = url
        self.name = url[url.rfind("/")+1:-4]
        self.clonedLocations = []

    def getPrereqs(self): return [ "git" ]

    def cloneRepo(self, destination=None):
        if destination: destination = utils.sanitizeDir(destination)
        else: destination = utils.sanitizeDir(self.name)

        if path.isdir(destination):
            print("Found existing repository at '{}'".format(destination))
            if destination not in self.clonedLocations:
                self.clonedLocations.append(destination)
        else:
            args = [ "git", "clone", self.url, destination ]
            utils.runCmd(args, wait=True, interactive=True)
            self.clonedLocations.append(destination)

def getPopcornKernelRepo():
    return GitRepo("https://github.com/ssrg-vt/popcorn-kernel.git")

def getPopcornCompilerRepo():
    return GitRepo("https://github.com/ssrg-vt/popcorn-compiler.git")

