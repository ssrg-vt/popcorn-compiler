import re
import sys
import os

class Symbol:
	_name = ""
	_address = 0x0
	_size = 0x0
	_alignment = 0x0

	def __init__(self, name, address, size, alignment):
		self._name = name
		self._address = address
		self._size = size
		self._alignment = alignment

	def __str__(self):
		return ("Symbol: name=" + self._name + ", address=" + 
			str(self._address) + ", size=" + str(self._size) + ", alignment=" 
			+ str(self._alignment))

	def setName(self, name):
		self._name = name

	def setAddress(self, address):
		self._address = address

	def setSize(self, size):
		self._siwe = size

	def setAlignment(self, alignment):
		self._alignment = alignment

	def getName(self):
		return self._name

	def getAddress(self):
		return self._address;

	def getSize(self):
		return self._size;

	def getAlignment(self):
		return self._alignment

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
	twoLinesRe2 = "^[\s]+(0x[0-9a-f]+)[\s]+(0x[0-9a-f]+)[\s]+(0x[0-9a-f]+)[\s]+(.*)$"
	oneLineRe = "^[\s]+(\.[texrodalcbs\.]+[\S]+)[\s]+(0x[0-9a-f]+)[\s]+(0x[0-9a-f]+)[\s]+(0x[0-9a-f]+)[\s]+(.*)$"

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

