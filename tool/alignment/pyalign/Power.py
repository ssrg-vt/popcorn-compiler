# This file needs to be completed when the power compiler is working

from AbstractArchitecture import AbstractArchitecture
from Arch import Arch
import Symbol

class Power(AbstractArchitecture):
	
	def __init__(self):
		# Prefix for the gcc compiler name
		self._gccPrefix = "powerpc64-linux-gnu"
		# Executable name
		self._executable = "bin_power"
		# Name of the map file output by gold
		self._mapFile = "map_power"
		# Linker script used for the last linking step
		self._linkerScript = "linker_script_power.x"
		# Linker script template
		self._linkerScriptTemplate = "ls_power.template"
		# set of libraries to search as a group during the last linking step
		self._goldSearchGroup = "TODO"
		# ISA folder name in popcorn install dir
		self._popcornIsaFolder = "TODO"
		# GNU gold emulation
		self._goldEmulation = "TODO" # probably elf64-powerpc
		# list of object files, to be set at runtime
		self._objectFiles = []

	# Hacky way to manage the difference in the way libgcc is linked between
	# different architectures (using the static archive libgcc.a for X86,
	# and -Lpath/to/libgcc.a/containing/folder for arm64 (for some reason
	# directly linking with the static archive doesnt work for arm
	def getLibGccGoldInclusion(self):
		raise exception.NotImplementedError

	def getLsTemplate(self):
		return self._linkerScriptTemplate

	def setLsTemplate(self, template):
		self._linkerScriptTemplate = template

	def getExecutable(self):
		return self._executable

	def getMapFile(self):
		return self._mapFile

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

	def getArchString(self):
		return "POWER"
