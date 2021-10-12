"""
Class representing the riscv64 architecture specific attributes
"""
from AbstractArchitecture import AbstractArchitecture
from Arch import Arch
import Symbol

class RISCV(AbstractArchitecture):

	def __init__(self):
		# Executable name
		self._executable = ""
		# Name of the map file used in multiple steps of the alignment process
		self._mapFile = ""
		# Linker script template
		self._linkerScriptTemplate = "ls_riscv.template"

	def getArch(self):
		return Arch.RISCV

	def getArchString(self):
		return "RISCV"
