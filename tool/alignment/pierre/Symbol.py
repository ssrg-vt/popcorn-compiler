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
		self._isReferenced = { Arch.X86 : False, Arch.ARM : False, 
			Arch.POWER : False }
		self._isReferenced[arch] = True
		self._paddingBefore = { Arch.X86 : 0, Arch.ARM : 0, Arch.POWER : 0 }
		self._paddingAfter = { Arch.X86 : 0, Arch.ARM : 0, Arch.POWER : 0 }

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
				", alignmentPower=" + str(hex(self.getAlignment(Arch.POWER))) +
				", referencedX86=" + str(self.getReference(Arch.X86)) +
				", referencedARM=" + str(self.getReference(Arch.ARM)) +
				", referencedPOWER=" + str(self.getReference(Arch.POWER)) +
			", paddingBeforeX86=" + hex(str(self.getPaddingBefore(Arch.x86))) +
			", paddingBeforeARM=" + hex(str(self.getPaddingBefore(Arch.ARM))) +
			", paddingBeforePOWER=" + hex(str(self.getPaddingBefore(Arch.POWER))) +
			", paddingAfterX86=" + hex(str(self.getPaddingAfter(Arch.x86))) +
			", paddingAfterARM=" + hex(str(self.getPaddingAfter(Arch.ARM))) +
			", paddingAfterPOWER=" + hex(str(self.getPaddingAfter(Arch.POWER))))

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

	def setReference(self, arch):
		self._isReferenced[arch] = True

	def getReference(self, arch):
		return self._isReferenced(arch)

	def getPaddingBefore(self, arch):
		return self._paddingBefore[arch]

	def getPaddingAfter(self, arch):
		return self._paddingAfter[arch]

	def incrPaddingBefore(self, paddingToAdd, arch):
		self._paddingBefore[arch] += paddingToAdd

	def incrPaddingAfter(self, paddingToAdd, arch):
		self._paddingAfter[arch] += paddingToAdd

	def getLargetSizeArch(self):
		res = None
		largestSize = -1
		for arch in self._sizes.keys():
			size = self.getSize(arch)
			if size > largestSize:
				largestSize = size
				res = arch
		return res

	def getLargetSizeVal(self):
		return self.getSize(self.getLargetSizeArch())

	# Returns a value from the enum Arch.Arch.XXXX, not an Arch.Arch instance
	def getArchitecturesReferencing(self):
		res = []
		for arch in self._isReferenced.keys():
			if self._isReferenced[arch]:
				res.append(arch)
		return res

	def getArchitecturesNotReferencing(self):
		archsReferencing = self.getArchitecturesReferencing()
		return [arch for arch in self._isReferenced.keys() if arch not in
			archsReferencing]

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

	# set the alignement for all referenced arch to the largest alignement of 
	# the referencing archs
	# also return the alignement
	def setLargestAlignment(self):
		largestAlignment = -1
		for arch in self._alignments.keys():
			alignment = self.getAlignment(arch)
			if alignment > largestAlignment:
				largestAlignment = alignment

		for arch in self.getArchitecturesReferencing():
			self.setAlignment(largestAlignment, arch)

		return largestAlignment
