"""
Class representing the power8 architecture specific attributes
"""

# TODO This file needs to be completed when the power compiler is working

from AbstractArchitecture import AbstractArchitecture
from Arch import Arch
import Symbol

class Power(AbstractArchitecture):
	
	def __init__(self):
		# Executable name
		self._executable = "bin_power"
		# Name of the map file output by gold
		self._mapFile = "map_power"
		# Linker script used for the last linking step
		self._linkerScript = "linker_script_power.x"
		# Linker script template
		self._linkerScriptTemplate = "ls_power.template"
		# Subdirectory of the working directory we put the object files into
		self._objDir = "obj_power"
		# list of object files, to be set at runtime
		self._objectFiles = []

	def getArch(self):
		return Arch.POWER

	def getArchString(self):
		return "POWER"
