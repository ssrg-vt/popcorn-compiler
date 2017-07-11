import subprocess
import sys
from AbstractArchitecture import AbstractArchitecture 

class X86(AbstractArchitecture):
	# Prefix for the gcc compiler name
	_gcc_prefix = "x86_64-linux-gnu"
	# Executable name for the output of the last linking step
	_gold_output = "x86BinGold_musl"
	# Name of the map file taken as input by gold
	_gold_map = "map_x86gold.txt"
	# Linker script used for the last linking step
	_linker_script = "modified__elf_x86_64.x"
	# log file containing gold std/err output
	_linker_log = "out_x86_64.txt"
	# set of libraries to search as a group during the last linking step
	_gold_search_group = ""
	# ISA folder name in popcorn install dir
	_popcorn_isa_folder = "x86_64"
	# GNU gold emulation
	_gold_emulation = "elf_x86_64"

	def getCrossCompile(self):
		return False

	# Hacky way to manage the difference in the way libgcc is linked between
	# different architectures (using the static archive libgcc.a for X86,
	# and -Lpath/to/libgcc.a/containing/folder for arm64 (for some reason
	# directly linking with the static archive doesnt work for arm
	def getLibGccGoldInclusion(self):
		return self.getLibGccLocation() + "/libgcc.a"

	def getGoldOutput(self):
		return self._gold_output

	def getGoldMap(self):
		return self._gold_map

	def getLinkerScript(self):
		return self._linker_script

	def getLinkerLog(self):
		return self._linker_log

	def getGoldSearchGroup(self):
		return self._gold_search_group

	def getPopcornIsaFolder(self):
		return self._popcorn_isa_folder

	def getGoldEmulation(self):
		return self._gold_emulation
