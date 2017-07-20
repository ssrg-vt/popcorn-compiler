import Symbol
from AbstractArchitecture import AbstractArchitecture 
from Arch import Arch

class X86(AbstractArchitecture):

	def __init__(self):
		# Prefix for the gcc compiler name
		self._gccPrefix = "x86_64-linux-gnu"
		# Executable name
		self._executable = "bin_x86"
		# Name of the map file used in multiple steps of the alignment process
		self._mapFile= "map_x86"
		# Linker script used in multiple steps
		self._linkerScript = "linker_script_x86.x"
		# Linker script template
		self._linkerScriptTemplate = "ls_x86.template"
		# set of libraries to search as a group during linking
		self._goldSearchGroup = ""
		# ISA folder name in popcorn install dir
		self._popcornIsaFolder = "x86_64"
		# GNU gold emulation target
		self._goldEmulation = "elf_x86_64"

	# Hacky way to manage the difference in the way libgcc is linked between
	# different architectures (using the static archive libgcc.a for X86,
	# and -Lpath/to/libgcc.a/containing/folder for arm64 (for some reason
	# directly linking with the static archive doesnt work for arm
	def getLibGccGoldInclusion(self):
		return self.getLibGccLocation() + "/libgcc.a"

	def getExecutable(self):
		return self._executable

	def getLsTemplate(self):
		return self._linkerScriptTemplate

	def setLsTemplate(self, template):
		self._linkerScriptTemplate = template

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

	def getArchString(self):
		return "X86"
