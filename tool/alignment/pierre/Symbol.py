from Arch import Arch

class Symbol:

	def __init__(self, name, address, size, alignment, arch):
		self._name = name
		self._addresses = { Arch.X86 : -1, Arch.ARM : -1, Arch.POWER : -1 }
		self._addresses[arch] = address
		self._sizes = { Arch.X86 : -1, Arch.ARM : -1, Arch.POWER : -1 }
		self._sizes[arch] = size
		self._alignments = { Arch.X86 : -1, Arch.ARM : -1, Arch.POWER : -1 }
		self._alignments[arch] = alignment

	def __str__(self):
		return ("Symbol: name=" + self.getName() + 
				", addressX86=" + str(hex(self.getAddress(Arch.X86))) +
				", addressArm=" + str(hex(self.getAddress(Arch.ARM))) +
				", addressPower=" + str(hex(self.getAddress(Arch.POWER))) +
				", sizeX86=" + str(hex(self.getSize(Arch.X86))) +
				", sizeArm=" + str(hex(self.getSize(Arch.ARM))) +
				", sizePower=" + str(hex(self.getSize(Arch.POWER))) +
				", alignmentX86=" + str(hex(self.getAlignment(Arch.X86))) +
				", alignmentArm=" + str(hex(self.getAlignment(Arch.ARM))) +
				", alignmentPower=" + str(hex(self.getAlignment(Arch.POWER))))

	def setName(self, name):
		self._name = name

	# arch should be one of the enum Arch.XX values
	def setAddress(self, address, arch):
		Arch.sanityCheck(arch)
		self._addresses[arch] = address

	def setSize(self, size, arch):
		Arch.sanityCheck(arch)
		self._sizes[arch] = size

	def setAlignment(self, alignment, arch):
		Arch.sanityCheck(arch)
		self._alignments[arch] = alignment

	def getName(self):
		return self._name

	def getAddress(self, arch):
		Arch.sanityCheck(arch)
		return self._addresses[arch]

	def getSize(self, arch):
		Arch.sanityCheck(arch)
		return self._sizes[arch]

	def getAlignment(self, arch):
		Arch.sanityCheck(arch)
		return self._alignments[arch]
