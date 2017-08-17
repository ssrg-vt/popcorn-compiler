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
		self._executable = ""
		# Name of the map file output by gold
		self._mapFile = ""
		# Linker script used for the last linking step
		self._linkerScript = "linker_script_power.x"
		# Linker script template
		self._linkerScriptTemplate = "ls_power.template"

	def getArch(self):
		return Arch.POWER

	def getArchString(self):
		return "POWER"
