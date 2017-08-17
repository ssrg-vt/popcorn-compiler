"""
Class representing the aarch64 architecture specific attributes
"""
from AbstractArchitecture import AbstractArchitecture
from Arch import Arch
import Symbol

class Arm(AbstractArchitecture):

	def __init__(self):
		# Executable name
		self._executable = ""
		# Name of the map file used in multiple steps of the alignment process
		self._mapFile = ""
		# Linker script template
		self._linkerScriptTemplate = "ls_arm.template"

	def getArch(self):
		return Arch.ARM

	def getArchString(self):
		return "ARM"
