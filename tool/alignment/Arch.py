"""
C-style enum for architectures: x86, arm, power, and riscv
"""

import sys
from Globals import erStack

class Arch():
	X86 = 0
	ARM = 1
	POWER = 2
        RISCV = 3

	@classmethod
	def sanityCheck(cls, val):
		""" Checik if val has a correct value """
		if val != cls.X86 and val != cls.ARM and val != cls.POWER \
                   and val != cls.RISCV:
			erStack("bad value for Arch enum: " + str(val) + "\n")
			sys.exit(-1)
