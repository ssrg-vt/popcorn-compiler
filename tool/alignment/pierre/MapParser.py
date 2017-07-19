import re
import sys
import os
from Arch import Arch

class Symbol:
	_name = ""
	_address = 0x0
	_sizes = { Arch.X86 : -1, Arch.ARM : -1, Arch.POWER : -1 }
	_alignments = { Arch.X86 : -1, Arch.ARM : -1, Arch.POWER : -1 }

	def __init__(self, name, address, sizeX86=-1, sizeArm=-1, sizePower=-1,
		alignmentX86=-1, alignmentArm=-1, alignmentPower=-1):
		self._name = name
		self._address = address
		self._sizes[Arch.X86] = sizeX86
		self._sizes[Arch.ARM] = sizeArm
		self._sizes[Arch.POWER] = sizePower
		self._alignments[Arch.X86] = alignmentX86
		self._alignments[Arch.ARM] = alignmentArm
		self._alignments[Arch.POWER] = alignmentPower

	def __str__(self):
		return ("Symbol: name=" + self.getName() + 
				", address=" + str(self.getAddress()) +
				", sizeX86=" + str(self.getSizeX86()) +
				", sizeArm=" + str(self.getSizeArm()) +
				", sizePower=" + str(self.getSizePower()) +
				", alignmentX86=" + str(self.getAlignmentX86()) +
				", alignmentArm=" + str(self.getAlignmentArm()) +
				", alignmentPower=" + str(self.getAlignmentPower()))

	def setName(self, name):
		self._name = name

	def setAddress(self, address):
		self._address = address

	def setSizeX86(self, size):
		self._sizes[Arch.X86] = size

	def setSizeArm(self, size):
		self._sizes[Arch.ARM] = size

	def setSizePower(self, size):
		self._sizes[Arch.POWER] = size

	def setAlignmentX86(self, alignment):
		self._alignments[Arch.X86] = alignment
	
	def setAlignmentArm(self, alignment):
		self._alignments[Arch.ARM] = alignment

	def setAlignmentPower(self, alignment):
		self._alignments[Arch.POWER] = alignment

	def getName(self):
		return self._name

	def getAddress(self):
		return self._address;

	def getSizeX86(self):
		return self._sizes[Arch.X86]

	def getSizeArm(self):
		return self._sizes[Arch.ARM]

	def getSizePower(self):
		return self._sizes[Arch.POWER]

	def getAlignmentX86(self):
		return self._alignments[Arch.X86]

	def getAlignmentArm(self):
		return self._alignments[Arch.ARM]

	def getAlignmentPower(self):
		return self._alignments[Arch.POWER]


# Returns a list of Symbols instances extracted from the map file which 
# path is filePath (should have been generalted by gold.ld -Map <file>
def parseMapFile(filePath):
	
	res = []

	# The symbols described in the parsed file can mostly have 2 forms: 
	# - 1 line description: "<name> <addr> <size> <alignment> <object file>"
	# - 2 lines description:
	#   "<name>
	#                 <addr> <size> <alignment> <object file>"
	# So here I have 1 regexp for the single line scenario and 2 for the double 
	# lines one. This filters out a lot of uneeded stuff, but we still need to
	# only keep symbols related to text/data/rodata/bss so an additional check
	# is performed on the extracted symbosl before addign it to the result set
	twoLinesRe1 = "^[\s]+(\.[texrodalcbs\.]+[\S]*)$"
	twoLinesRe2 = ("^[\s]+(0x[0-9a-f]+)[\s]+(0x[0-9a-f]+)[\s]+" + 
		"(0x[0-9a-f]+)[\s]+(.*)$")
	oneLineRe = ("^[\s]+(\.[texrodalcbs\.]+[\S]+)[\s]+(0x[0-9a-f]+)[\s]+" + 
		"(0x[0-9a-f]+)[\s]+(0x[0-9a-f]+)[\s]+(.*)$")

	with open(filePath, "r") as mapfile:
		lines = mapfile.readlines()
		for index, line in enumerate(lines):
			s = None
			matchResult = re.match(twoLinesRe1, line)
			if matchResult: # probably a 2-lines symbol description
				nextLine = lines[index+1]
				matchResult2 = re.match(twoLinesRe2, nextLine)
				if matchResult2:
					name = matchResult.group(1)
					address = int(matchResult2.group(1), 0)
					size = int(matchResult2.group(2), 0)
					alignment = int(matchResult2.group(3), 0)
					s = Symbol(name, address, size, alignment)
				else:
					print ("ERROR! missed a two lines symbol while parsing "
						+ "mapfile")
					print "line1: " + line
					print "line2: " + nextLine
					sys.exit(-1)
			else:
				matchResult3 = re.match(oneLineRe, line)
				if matchResult3: # one line symbol description
					name = matchResult3.group(1)
					address = int(matchResult3.group(2), 0)
					size = int(matchResult3.group(3), 0)
					alignment = int(matchResult3.group(4), 0)
					s = Symbol(name, address, size, alignment)
			if s:
				for section_name in ["text", "data", "rodata", "bss"]:
					if s.getName().startswith("." + section_name + "."):
						# We are only interested in text/data/rodata/bss
						res.append(s)
	#			print "Symbol found: "
	#			print " " + str(s)
	#			print "Line: "
	#			print " " + line.replace("\n", "")
	#			print "--------------------------------------"
			else:
				pass
				#print "Unmatched line: " + line.replace("\n", "")
				#print "--------------------------------------"

	return res

