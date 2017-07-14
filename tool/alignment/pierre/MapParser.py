import re
import sys

class Symbol:
	_name = ""
	_address = 0x0
	_size = 0x0
	_alignment = 0x0

	def __init__(self, name, address, size, alignment):
		self._name = name
		self._address = address
		self._size = size
		self._alignement = alignment

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

	twoLinesRe1 = "^ .([texrodalcbs]+).[\S]*$"
	twoLinesRe2 = "^[\s]+(0x[0-9a-f]+)[\s]+(0x[0-9a-f]+)[\s]+(0x[0-9a-f]+)[\s]+(.*)$"

	with open(filePath, "r") as mapfile:
		lines = mapfile.readlines()
		for index, line in enumerate(lines):
			matchResult = re.match(twoLinesRe1, line)
			if matchResult:
				nextLine = lines[index+1]
				matchResult2 = re.match(twoLinesRe2, nextLine)
				if matchResult2:
					name = matchResult.group(0).replace(" ", "")
					address = matchResult2.group(0)
					size = matchResult2.group(1)
					alignment = matchResult2.group(2)
					s = Symbol(name, address, size, alignment)
					print s
				else:
					print ("ERROR! missed a two line symbol while parsing "
						+ "mapfile")
					print "line1: " + line
					print "line2: " + nextLine
					sys.exit()

# TODO remove me
if __name__ == "__main__":
	parseMapFile(sys.argv[1])
