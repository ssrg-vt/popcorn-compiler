from X86 import X86
from Arm import Arm
from Power import Power

class Arch():
	X86 = 0
	ARM = 1
	POWER = 2


x86_obj = X86()
arm_obj = Arm()
power_obj = Power()

Archs = { 	Arch.X86 : x86_obj,
			Arch.ARM : arm_obj,
			Arch.POWER : power_obj,
		}
