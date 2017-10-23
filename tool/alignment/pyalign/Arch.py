"""
C-style enum for architectures: x86, arm, and power
"""

import sys
from Globals import erStack

class Arch():
	X86 = 0
	ARM = 1
	POWER = 2

	@classmethod
	def sanityCheck(cls, val):
		""" Checik if val has a correct value """
		if val != cls.X86 and val != cls.ARM and val != cls.POWER:
			erStack("bad value for Arch enum: " + str(val) + "\n")
			sys.exit(-1)
