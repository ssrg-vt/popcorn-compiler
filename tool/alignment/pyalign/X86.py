"""
Class representing the x86_64 architecture specific attributes
"""
import Symbol, Globals
from AbstractArchitecture import AbstractArchitecture
from Arch import Arch

class X86(AbstractArchitecture):

	def __init__(self):
		# Executable name
		self._executable = ""
		# Name of the map file used in multiple steps of the alignment process
		self._mapFile= ""
		# Linker script template
		self._linkerScriptTemplate = "ls_x86.template"

	def getArch(self):
		return Arch.X86

	def getArchString(self):
		return "X86"
