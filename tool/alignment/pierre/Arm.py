import Globals
from AbstractArchitecture import AbstractArchitecture

class Arm(AbstractArchitecture):
	# Prefix for the gcc compiler name
	_gcc_prefix = "aarch64-linux-gnu"
	# Executable name for the output of the last linking step
	_gold_output = "armBinGold_musl"
	# Name of the map file taken as input by gold
	_gold_map = "map_aarchgold.txt"
	# Linker script used for the last linking step
	_linker_script = "modified__aarch64.x"
	# log file containing gold std/err output
	_linker_log = "out_aarch64.txt"
	# set of libraries to search as a group during the last linking step
	_gold_search_group = "-lgcc -lgcc_eh"
	# ISA folder name in popcorn install dir
	_popcorn_isa_folder = "aarch64"
	# GNU gold emulation
	_gold_emulation = "aarch64linux"

	def getCrossCompile(self):
		return True

	# Hacky way to manage the difference in the way libgcc is linked between
	# different architectures (using the static archive libgcc.a for X86,
	# and -Lpath/to/libgcc.a/containing/folder for arm64 (for some reason
	# directly linking with the static archive doesnt work for arm
	def getLibGccGoldInclusion(self):
		return "-L" + self.getLibGccLocation()

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
