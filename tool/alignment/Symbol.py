"""
Represent a symbol. One instance per architecture. However, some of the
attributes are per-architecture (ex: size, etc.) so they are arrays indexed
by the Arch enum (see Arch.py)
"""

import sys, re
from Arch import Arch
from Globals import er

def symbolObjectFileSanityCheck(obj):
        reLib = "^(.+\.a)\((.+\.l?o)\)"
	reObj = "^(.+\.o)"				# or an object file
	if (not re.match(reLib, obj)) and (not re.match(reObj, obj)):
		return False
	return True

class Symbol:

	def __init__(self, name, address, size, alignment, objectFile, arch):
		# Symbol name
		self._name = name
		# Symbol address for each arch
		self._addresses = { Arch.X86 : -1, Arch.ARM : -1, Arch.POWER : -1 }
		self._addresses[arch] = address
		# Symbol size for each arch
		self._sizes = { Arch.X86 : -1, Arch.ARM : -1, Arch.POWER : -1 }
		self._sizes[arch] = size
		# Alignemt rule for each arch
		self._alignments = { Arch.X86 : -1, Arch.ARM : -1, Arch.POWER : -1 }
		self._alignments[arch] = alignment
		# Which architecture are referencing this symbol?
		self._isReferenced = { Arch.X86 : False, Arch.ARM : False,
			Arch.POWER : False }
		self._isReferenced[arch] = True
		# padding to add before and after the symbol in the produced linker
		# script, for each architecture
		self._paddingBefore = { Arch.X86 : 0, Arch.ARM : 0, Arch.POWER : 0 }
		self._paddingAfter = { Arch.X86 : 0, Arch.ARM : 0, Arch.POWER : 0 }
		# Object file the symbol initially comes from
		self._objectFiles =  { Arch.X86 : "NULL", Arch.ARM : "NULL",
			Arch.POWER : "NULL" }

		if not symbolObjectFileSanityCheck(objectFile):
			er("Failed sanity check on object file during symbol instance " +
				"creation\n")
			sys.exit(-1);

		self._objectFiles[arch] = objectFile

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
			", paddingBeforeX86=" + str(hex(self.getPaddingBefore(Arch.X86))) +
			", paddingBeforeARM=" + str(hex(self.getPaddingBefore(Arch.ARM))) +
			", paddingBeforePOWER=" + str(hex(self.getPaddingBefore(Arch.POWER))) +
			", paddingAfterX86=" + str(hex(self.getPaddingAfter(Arch.X86))) +
			", paddingAfterARM=" + str(hex(self.getPaddingAfter(Arch.ARM))) +
			", paddingAfterPOWER=" + str(hex(self.getPaddingAfter(Arch.POWER)))+
			", objX86=" + self.getObjectFile(Arch.X86) +
			", objARM=" + self.getObjectFile(Arch.ARM) +
			", objPOWER=" + self.getObjectFile(Arch.POWER))


	def getObjectFile(self, arch):
		return self._objectFiles[arch]

	def setObjectFile(self, obj, arch):
		if not symbolObjectFileSanityCheck(obj):
			er("Failed sanity check on object file during symbol update\n")
			sys.exit(-1);
		self._objectFiles[arch] = obj

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
		return self._isReferenced[arch]

	def getPaddingBefore(self, arch):
		return self._paddingBefore[arch]

	def getPaddingAfter(self, arch):
		return self._paddingAfter[arch]

	def incrPaddingBefore(self, paddingToAdd, arch):
		self._paddingBefore[arch] += paddingToAdd

	def incrPaddingAfter(self, paddingToAdd, arch):
		self._paddingAfter[arch] += paddingToAdd

	def getLargetSizeArch(self):
		""" Return the architecture presenting the larget size for this symbol,
		 Returns a value from the enum Arch.Arch.XXXX, not an Arch.Arch instance
		"""
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

	def getArchitecturesReferencing(self):
		""" Returns a value from the enum Arch.Arch.XXXX, not an Arch.Arch
		instance
		"""
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

	def setLargestAlignment(self):
		"""set the alignement for all referenced arch to the largest alignement
		of 	the referencing archs, also returns the alignement
		"""
		largestAlignment = -1
		for arch in self._alignments.keys():
			alignment = self.getAlignment(arch)
			if alignment > largestAlignment:
				largestAlignment = alignment

		for arch in self.getArchitecturesReferencing():
			self.setAlignment(largestAlignment, arch)

		return largestAlignment

	def compare(self, anotherSymbol):
		""" Compare two symbols to check if they correspond to the same. They
		are the same if the name is the same AND if they correspond to the
		same original object file
		"""
		# Quick path: first check the name
		if self.getName() != anotherSymbol.getName():
			return False

		# Then check the object paths
		res = None
		otherObjs = [   anotherSymbol.getObjectFile(Arch.X86),
						anotherSymbol.getObjectFile(Arch.ARM),
						anotherSymbol.getObjectFile(Arch.POWER)]
		for objf1 in self._objectFiles.values():
			for objf2 in otherObjs:
				if objf1 != "NULL" and objf2 != "NULL":
					cmpstr1 = objf1.split("/")[-1]
					cmpstr2 = objf2.split("/")[-1]

					# First handle the special case of user object files that
					# differs by name because they are for different archs
					# but are the result of the compilation of the same user
					# source file
					# FIXME this is hardcoded for x86-ARM for now, we need a
					# convention for the user object files created by the
					# popcorn compiler. Need a better way to handle that when
					# we get to power8
					if cmpstr1.endswith("_x86_64.o"):   # s1 is x86
						s1_base = cmpstr1.replace("_x86_64.o", "")
						if (s1_base == cmpstr2.replace(".o", "")):
							# s2 is arm (v1)
							res = True
							continue
						elif (s1_base == cmpstr2.replace("_aarch64.o", "")):
							# s2 is arm (v2)
							res = True
							continue
						elif (s1_base == cmpstr2.replace("_powerpc64le.o", "")):
							# s2 is ppc
							res = True
							continue

					elif cmpstr1.endswith("_aarch64.o"):   # s1 is arm (v1)
						s1_base = cmpstr1.replace("_aarch64.o", "")
						if (s1_base == cmpstr2.replace("_x86_64.o", "")):
							# s2 is x86
							res = True
							continue
						elif (s1_base == cmpstr2.replace("_powerpc64le.o", "")):
							# s2 is ppc
							res = True
							continue

					elif cmpstr1.endswith("_powerpc64le.o"):   # s1 is ppc
						s1_base = cmpstr1.replace("_powerpc64le.o", "")
						if (s1_base == cmpstr2.replace("_x86_64.o", "")):
							# s2 is x86
							res = True
							continue
						elif (s1_base == cmpstr2.replace(".o", "")):
							# s2 is arm (v1)
							res = True
							continue
						elif (s1_base == cmpstr2.replace("_aarch64.o", "")):
							# s2 is arm (v2)
							res = True
							continue

					elif cmpstr1.endswith(".o"):   # s1 is arm (v2)
						s1_base = cmpstr1.replace(".o", "")
						if (s1_base == cmpstr2.replace("_x86_64.o", "")):
							# s2 is x86
							res = True
							continue
						elif (s1_base == cmpstr2.replace("_powerpc64le.o", "")):
							# s2 is ppc
							res = True
							continue

					if cmpstr1 != cmpstr2:
						return False
					else:
						res = True

		if res == None:
			er("Could not find object files to compare...\n")
			sys.exit(-1)

		return res

