from AbstractArchitecture import AbstractArchitecture
from Arch import Arch
import Symbol

class Arm(AbstractArchitecture):
	
	def __init__(self):
		# Prefix for the gcc compiler name
		self._gccPrefix = "aarch64-linux-gnu"
		# Executable name
		self._executable = "bin_arm"
		# Name of the map file used in multiple steps of the alignment process
		self._mapFile = "map_arm"
		# Linker linker script file used in multiple steps
		self._linkerScript = "linker_script_arm.x"
		# set of libraries to search as a group during linking
		self._goldSearchGroup = "-lgcc -lgcc_eh"
		# ISA folder name in popcorn install dir
		self._popcornIsaFolder = "aarch64"
		# GNU gold emulation target
		self._goldEmulation = "aarch64linux"

	# Hacky way to manage the difference in the way libgcc is linked between
	# different architectures (using the static archive libgcc.a for X86,
	# and -Lpath/to/libgcc.a/containing/folder for arm64 (for some reason
	# directly linking with the static archive doesnt work for arm
	def getLibGccGoldInclusion(self):
		return "-L" + self.getLibGccLocation()

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
		return Arch.ARM

	def getArchString(self):
		return "ARM"
