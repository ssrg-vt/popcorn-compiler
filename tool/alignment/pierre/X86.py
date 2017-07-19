import sys
from AbstractArchitecture import AbstractArchitecture 
from Arch import Arch

class X86(AbstractArchitecture):
	# Prefix for the gcc compiler name
	_gccPrefix = "x86_64-linux-gnu"
	# Executable name
	_executable = "bin_x86"
	# Name of the map file used in multiple steps of the alignment process
	_mapFile= "map_x86"
	# Linker script used in multiple steps
	_linker_script = "linker_script_x86.x"
	# set of libraries to search as a group during linking
	_goldSearchGroup = ""
	# ISA folder name in popcorn install dir
	_popcornIsaFolder = "x86_64"
	# GNU gold emulation target
	_goldEmulation = "elf_x86_64"

	# Hacky way to manage the difference in the way libgcc is linked between
	# different architectures (using the static archive libgcc.a for X86,
	# and -Lpath/to/libgcc.a/containing/folder for arm64 (for some reason
	# directly linking with the static archive doesnt work for arm
	def getLibGccGoldInclusion(self):
		return self.getLibGccLocation() + "/libgcc.a"

	def getExecutable(self):
		return self._executable

	def getMapFile(self):
		return self._mapFile

	def getLinkerScript(self):
		return self._linkerScript

	def getGoldSearchGroup(self):
		return self._goldSearchGroup

	def getPopcornIsaFolder(self):
		return self._popcornIsaFolder

	def getGoldEmulation(self):
		return self._goldEmulation

	def getArch(self):
		return Arch.X86
