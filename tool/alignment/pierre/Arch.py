from X86 import X86
from Arm import Arm
from Power import Power
from Sparc import Sparc

class Arch():
	X86 = 0
	ARM = 1
	POWER = 2
	SPARC = 3


x86_obj = X86()
arm_obj = Arm()
power_obj = Power()
sparc_obj = Sparc()

Archs = { 	Arch.X86 : x86_obj,
			Arch.ARM : arm_obj,
			Arch.POWER : power_obj,
			Arch.SPARC : sparc_obj
		}
