# This file needs to be completed when the power compiler is working

import Globals
from AbstractArchitecture import AbstractArchitecture
from Arch import Arch

class Power(AbstractArchitecture):
	# Prefix for the gcc compiler name
	_gccPrefix = "powerpc64-linux-gnu"
	# Executable name
	_executable = "bin_power"
	# Name of the map file output by gold
	_mapFile = "map_power"
	# Linker script used for the last linking step
	_linkerScript = "linker_script_power.x"
	# set of libraries to search as a group during the last linking step
	_goldSearchGroup = "TODO"
	# ISA folder name in popcorn install dir
	_popcornIsaFolder = "TODO"
	# GNU gold emulation
	_goldEmulation = "TODO" # probably elf64-powerpc

	# Hacky way to manage the difference in the way libgcc is linked between
	# different architectures (using the static archive libgcc.a for X86,
	# and -Lpath/to/libgcc.a/containing/folder for arm64 (for some reason
	# directly linking with the static archive doesnt work for arm
	def getLibGccGoldInclusion(self):
		raise exception.NotImplementedError

	def getExecutable(self):
		return self._executable

	def getMapFile(self):
		return self._mapFile

	def getLinkerScript(self):
		return self._linkerScript

	def getGoldSearchGroup(self):
		raise exception.NotImplementedError
		#return self._gold_search_group

	def getPopcornIsaFolder(self):
		raise exception.NotImplementedError
		#return self._popcorn_isa_folder

	def getGoldEmulation(self):
		raise exception.NotImplementedError
		#return self._gold_emulation

	def getArch(self):
		return Arch.POWER
