# This file needs to be completed when the power compiler is working

import Globals
from AbstractArchitecture import AbstractArchitecture

class Sparc(AbstractArchitecture):
	# Prefix for the gcc compiler name
	_gcc_prefix = "sparc64-linux-gnu"
	# Executable name for the output of the last linking step
	_gold_output = "TODO"
	# Name of the map file taken as input by gold
	_gold_map = "TODO"
	# Linker script used for the last linking step
	_linker_script = "TODO"
	# log file containing gold std/err output
	_linker_log = "TODO"
	# set of libraries to search as a group during the last linking step
	_gold_search_group = ""
	# ISA folder name in popcorn install dir
	_popcorn_isa_folder = "TODO"
	# GNU gold emulation
	_gold_emulation = "TODO" # probably elf64_sparc


	def getCrossCompile(self):
		return True

	# Hacky way to manage the difference in the way libgcc is linked between
	# different architectures (using the static archive libgcc.a for X86,
	# and -Lpath/to/libgcc.a/containing/folder for arm64 (for some reason
	# directly linking with the static archive doesnt work for arm
	def getLibGccGoldInclusion(self):
		raise NotImplementedError

	def getGoldOutput(self):
		raise NotImplementedError
		#return self._gold_output

	def getGoldMap(self):
		raise NotImplementedError
		#return self._gold_map

	def getLinkerScript(self):
		raise NotImplementedError
		#return self._linker_script

	def getLinkerLog(self):
		raise NotImplementedError
		#return self._linker_log

	def getGoldSearchGroup(self):
		raise NotImplementedError
		#return self._gold_search_group

	def getPopcornIsaFolder(self):
		raise NotImplementedError
		#return self._popcorn_isa_folder

	def getGoldEmulation(self):
		raise NotImplementedError
		#return self._gold_emulation
